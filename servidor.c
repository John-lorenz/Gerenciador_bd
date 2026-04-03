#define _POSIX_C_SOURCE 200809L // ativa funções POSIX tipo read, write, getline
#include "banco.h"
#include <fcntl.h> // open
#include <stdio.h> 
#include <stdlib.h> // exit, free
#include <string.h> // strlen, strcmp, strstr
#include <strings.h>
#include <sys/stat.h> // mkfifo
#include <unistd.h> // read, write, close, unlink

static Registro g_db[MAX_REG]; // vetor que guarda os dados
static size_t g_n = 0; // quantidade atual de registros
static pthread_mutex_t db_mtx = PTHREAD_MUTEX_INITIALIZER; // protege acesso ao banco

static char q[FILA_MAX][LINHA_MAX]; // fila circular de comandos
static int q_n = 0; // quantos itens tem na fila
static int q_h = 0; // posição inicial da fila
static pthread_mutex_t q_mtx = PTHREAD_MUTEX_INITIALIZER; // protege fila
static pthread_cond_t q_cv = PTHREAD_COND_INITIALIZER; // acorda threads
static volatile int g_ok = 1; // controla se servidor continua rodando

static int g_wr = -1; // FIFO de resposta
static pthread_mutex_t wr_mtx = PTHREAD_MUTEX_INITIALIZER; // evita conflito ao escrever

static void erro(const char *mensagem) {
    perror(mensagem); // mostra erro do sistema
    exit(1); // encerra programa
}

static void wr_buf(int fd, const void *buf, size_t n) {
    const char *p = buf;
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w <= 0) break;
        p += (size_t)w;
        n -= (size_t)w;
    }
}

static void salvar(void) {
    FILE *arquivo = fopen(ARQ_DB, "w"); // abre arquivo
    if (arquivo == NULL) return;

    for (size_t i = 0; i < g_n; i++) {
        fprintf(arquivo, "%d|%s\n", g_db[i].id, g_db[i].nome); //salva cada registro no arquivo
    }

    fclose(arquivo);
}

static void carregar(void) {
    FILE *arquivo = fopen(ARQ_DB, "r"); // abre arquivo
    if (arquivo == NULL) return;

    char linha[LINHA_MAX];
    g_n = 0; 

    while (g_n < MAX_REG && fgets(linha, sizeof(linha), arquivo)) { // lê linha por linha do arquivo
        int id;
        char nome[MAX_NOME];

        if (sscanf(linha, "%d|%49[^\n]", &id, nome) == 2) { // extrai id e nome da linha
            g_db[g_n].id = id; //guarda id no banco
            strcpy(g_db[g_n].nome, nome);  //guarda nome no banco
            g_n++; //incrementa quantidade de registros
        }
    }

    fclose(arquivo);
}

static ssize_t busca_id(int id) { // procura registro pelo id, retorna índice ou -1 se não achar
    for (size_t i = 0; i < g_n; i++) {
        if (g_db[i].id == id) return (ssize_t)i; 
    }
    return -1;
}

static int pega_id(const char *comando, int *id) {
    const char *pos = strstr(comando, "id="); // procura "id=" no comando
    if (!pos) return -1; // se não achar, retorna erro
    return sscanf(pos + 3, "%d", id) == 1 ? 0 : -1; // tenta extrair número depois de "id=", retorna 0 se sucesso, -1 se falha
}

static int pega_nome(const char *comando, char *nome) {
    const char *pos = strstr(comando, "nome="); // procura "nome=" no comando
    if (!pos) return -1; // se não achar, retorna erro
    return sscanf(pos + 5, " %[^\n]", nome) == 1 ? 0 : -1; // tenta extrair nome depois de "nome=", retorna 0 se sucesso, -1 se falha
}

static void envia(const char *texto) {
    pthread_mutex_lock(&wr_mtx); // garante que só uma thread escreve na FIFO de resposta por vez

    if (g_wr >= 0) {
        wr_buf(g_wr, texto, strlen(texto)); // escreve resposta no FIFO
        wr_buf(g_wr, "\n", 1); // escreve ENTER
    }

    pthread_mutex_unlock(&wr_mtx); // libera mutex
}

static void acorda_fila(void) {
    pthread_mutex_lock(&q_mtx);
    pthread_cond_broadcast(&q_cv);
    pthread_mutex_unlock(&q_mtx);
}

static void fila_push(const char *comando) {
    pthread_mutex_lock(&q_mtx);

    if (q_n < FILA_MAX) {
        int pos = (q_h + q_n) % FILA_MAX; // calcula posição de inserção na fila circular
        strcpy(q[pos], comando); // copia comando para a fila
        q_n++; // incrementa tamanho da fila
        pthread_cond_signal(&q_cv); // acorda uma thread
    }

    pthread_mutex_unlock(&q_mtx);
}

static void fila_pop(char *comando) {
    pthread_mutex_lock(&q_mtx);

    while (q_n == 0 && g_ok) { // se fila vazia, espera até ter algo ou servidor ser desativado
        pthread_cond_wait(&q_cv, &q_mtx); // espera por sinal de que tem algo na fila
    }

    if (q_n == 0) {
        comando[0] = '\0'; // se ainda estiver vazia, retorna string vazia
        pthread_mutex_unlock(&q_mtx); // libera mutex
        return;
    }

    strcpy(comando, q[q_h]); // copia comando da fila para variável de saída
    q_h = (q_h + 1) % FILA_MAX; // incrementa inicio da fila
    q_n--; // decrementa tamanho da fila

    pthread_mutex_unlock(&q_mtx);
}

static void processar(char *linha, char *resposta, size_t oz) { 

    linha[strcspn(linha, "\r\n")] = '\0';
    if (!*linha) {
        snprintf(resposta, oz, "ERR|vazio");
        return;
    }
    if (!strcasecmp(linha, "QUIT")) { // se comando for QUIT, prepara resposta e desativa servidor
        snprintf(resposta, oz, "OK|QUIT"); 
        g_ok = 0; // sinaliza para threads pararem
        return;
    }

    char operacao[20]; // guarda o tipo da operação 
    operacao[0] = '\0';
    sscanf(linha, "%19s", operacao); // extrai operação
    if (operacao[0] == '\0') {
        snprintf(resposta, oz, "ERR|comando");
        return;
    }

    int id;
    char nome[MAX_NOME];
    ssize_t i;

    if (!strcasecmp(operacao, "INSERT")) {
        if (pega_id(linha, &id) || pega_nome(linha, nome)) { // tenta extrair id e nome, se falhar prepara resposta de erro
            snprintf(resposta, oz, "ERR|INSERT id=N nome=texto");
            return;
        }
        pthread_mutex_lock(&db_mtx);

        if (busca_id(id) >= 0 || g_n >= MAX_REG)
            snprintf(resposta, oz, "ERR|id ou cheio");
        else {
            g_db[g_n].id = id; // adiciona novo registro no banco
            strcpy(g_db[g_n].nome, nome);
            g_n++;
            salvar();
            snprintf(resposta, oz, "OK|INSERT");
        }

        pthread_mutex_unlock(&db_mtx);
        return;
    }

    if (!strcasecmp(operacao, "DELETE")) {
        if (pega_id(linha, &id)) {
            snprintf(resposta, oz, "ERR|DELETE id=N");
            return;
        }
        pthread_mutex_lock(&db_mtx);

        i = busca_id(id);
        if (i < 0) {
            snprintf(resposta, oz, "ERR|nao achou");
        } else {
            memmove(g_db + i, g_db + i + 1, (g_n - (size_t)i - 1) * sizeof(Registro)); // desloca registros para "fechar" o espaço do registro deletado
            g_n--;
            salvar();
            snprintf(resposta, oz, "OK|DELETE");
        }

        pthread_mutex_unlock(&db_mtx);
        return;
    }

    if (!strcasecmp(operacao, "SELECT")) {
        const char *w = strstr(linha, "WHERE");
        id = -1;
        pega_id(w ? w : linha, &id);
        if (id < 0) {
            snprintf(resposta, oz, "ERR|SELECT WHERE id=N");
            return;
        }

        pthread_mutex_lock(&db_mtx);

        i = busca_id(id);
        if (i < 0) {
            snprintf(resposta, oz, "OK|SELECT|vazio");
        } else {
            snprintf(resposta, oz, "OK|SELECT|%d|%s", g_db[i].id, g_db[i].nome);
        }

        pthread_mutex_unlock(&db_mtx);
        return;
    }

    if (!strcasecmp(operacao, "UPDATE")) {
        if (pega_id(linha, &id) || pega_nome(linha, nome)) {
            snprintf(resposta, oz, "ERR|UPDATE id=N nome=texto");
            return;
        }
        pthread_mutex_lock(&db_mtx);

        i = busca_id(id);
        if (i < 0) {
            snprintf(resposta, oz, "ERR|nao achou");
        } else {
            strcpy(g_db[i].nome, nome); // atualiza nome do registro encontrado
            salvar();
            snprintf(resposta, oz, "OK|UPDATE");
        }

        pthread_mutex_unlock(&db_mtx);
        return;
    }

    snprintf(resposta, oz, "ERR|?");
}

static void *worker(void *arg) { // função executada por cada thread do servidor
    (void)arg; // pthread_create exige assinatura void *; não usamos o argumento
    char comando[LINHA_MAX]; // guarda comando retirado da fila
    char resposta[RESPOSTA_MAX]; // guarda resposta

    while (1) { // loop infinito para processar comandos
        fila_pop(comando); // espera até ter um comando na fila e o retira

        if (!g_ok && comando[0] == '\0') break; // se servidor foi desativado e fila está vazia, encerra thread
        if (comando[0] == '\0') continue; // se comando vazio, volta pro início do loop para esperar próximo comando

        processar(comando, resposta, sizeof resposta); // processa comando e prepara resposta
        if (!strncmp(resposta, "OK|QUIT", 7)) acorda_fila();
        envia(resposta); // envia resposta para o cliente
    }

    return NULL;
}

int main(void) {
    carregar();

    pthread_t th[N_THREADS];
    for (int i = 0; i < N_THREADS; i++) {
        pthread_create(&th[i], NULL, worker, NULL);
    }

    unlink(FIFO_PEDIDO);
    unlink(FIFO_RESPOSTA);
    mkfifo(FIFO_PEDIDO, 0666); // permissao de arquivo para leitura e escrita
    mkfifo(FIFO_RESPOSTA, 0666);

    /* O_RDWR na FIFO de resposta: com O_WRONLY o open bloqueia até haver leitor;
     * o cliente abre primeiro para leitura e também bloqueia — deadlock.
     * Ler+escrever na mesma FIFO pelo servidor não bloqueia (ver fifo(7)). */
    g_wr = open(FIFO_RESPOSTA, O_RDWR);
    if (g_wr < 0)
        erro("open FIFO resposta");

    fprintf(stderr, "Servidor à espera do cliente. Noutro terminal: ./cliente\n");
    fflush(stderr);

    int fd_pedido = open(FIFO_PEDIDO, O_RDONLY); //Só leitura
    if (fd_pedido < 0)
        erro("open FIFO pedido");

    fprintf(stderr, "Cliente ligado. Servidor a processar pedidos (Ctrl+D no cliente ou QUIT encerra).\n");
    fflush(stderr);

    FILE *fp = fdopen(fd_pedido, "r"); // abre fifo de pedido
    char *linha = NULL; // guarda linha lida do fifo
    size_t cap = 0; 

    while (g_ok && getline(&linha, &cap, fp) > 0) { // lê linha por linha do fifo
        linha[strcspn(linha, "\n")] = '\0'; // remove o ENTER
        if (linha[0]) fila_push(linha); // se linha não vazia, adiciona na fila para ser processada pelas threads
    }

    g_ok = 0;
    acorda_fila();

    for (int i = 0; i < N_THREADS; i++) {
        pthread_join(th[i], NULL); // aguarda threads terminarem
    }

    free(linha); // libera memória
    fclose(fp); // fecha fifo de pedido
    close(g_wr); // fecha fifo resposta

    unlink(FIFO_PEDIDO);
    unlink(FIFO_RESPOSTA);
    return 0;
}
