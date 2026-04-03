#define _POSIX_C_SOURCE 200809L //ativa funcionalidades do padrão POSIX, como chamadas de sistema tipo read, write e getline
#include "banco.h"
#include <errno.h>
#include <fcntl.h> // necessário pra usar open()
#include <stdio.h> 
#include <stdlib.h> // exit()
#include <string.h> // strlen, strcmp, strcspn
#include <strings.h>
#include <unistd.h> // read, write, close

static void erro(const char *mensagem) { //essa função é só pra não repetir código quando da erro
    perror(mensagem); //imprime erro
    exit(1);
}

static void enviar_comando(int fd, const char *comando) {
    size_t tamanho = strlen(comando); // pega o tamanho da string digitada
    
    for (size_t i = 0; i < tamanho; i++) {
        ssize_t w;
        do w = write(fd, comando + i, 1);
        while (w < 0 && errno == EINTR);
        if (w != 1) { //fd é o "canal" de comunicação com o servidor
            erro("write"); // escreve caractere por caractere no FIFO
        }
    }
    
    do {
        ssize_t w = write(fd, "\n", 1);
        if (w == 1) break;
        if (errno != EINTR) {
            erro("write"); // manda o ENTER (fim da linha)
        }
    } while (1);
}
static void ler_resposta(int fd, char *buffer, size_t capacidade) {
    size_t i = 0;
    
    while (i + 1 < capacidade) {
        char caractere;
        ssize_t lido = read(fd, &caractere, 1); //le 1 caractere por vez do fifo
        
        if (lido <= 0) {
            erro("read"); // erro na leitura
        }
        
        if (caractere == '\n') {
            break; // chegou no fim da resposta
        }
        
        buffer[i++] = caractere; // guarda o caractere no buffer
    }
    
    buffer[i] = '\0'; //transforma o buffer em uma string válida
}

int main(void) {
    int fd_resposta = open(FIFO_RESPOSTA, O_RDONLY); //abre o fifo de resposta
    if (fd_resposta < 0) {
        erro("open FIFO_RESPOSTA servidor rodando?"); //se não conseguir abrir, provavelmente o servidor não está rodando
    }
    
    int fd_pedido = open(FIFO_PEDIDO, O_WRONLY); //abre o fifo de pedido
    if (fd_pedido < 0) {
        erro("open FIFO_PEDIDO");
    }
    
    printf("Comandos:\n");
    printf("INSERT id=N nome=...\n");
    printf("DELETE id=N\n");
    printf("SELECT WHERE id=N\n");
    printf("UPDATE id=N nome=...\n");
    printf("QUIT\n");
    
    char linha[LINHA_MAX]; // guarda o que o usuário digita
    char resposta[RESPOSTA_MAX]; // guarda resposta do servidor
    
    while (fgets(linha, sizeof(linha), stdin)) { //lê o que o usuário digitar no terminal
        linha[strcspn(linha, "\r\n")] = '\0'; //remove o ENTER que o fgets adiciona
        
        if (linha[0] == '\0') {
            continue; // ignora linha vazia
        }
        
        enviar_comando(fd_pedido, linha); //envia pro servidor
        ler_resposta(fd_resposta, resposta, sizeof(resposta)); //le resposta
        
        printf("%s\n", resposta); // mostra resposta na tela
        
        if (strcmp(linha, "QUIT") == 0) { //se digitar quit encerra
            break;
        }
    }
    
    close(fd_pedido); //fecha fifo envioo
    close(fd_resposta); //fecha fifo resposta
    
    return 0;
}
