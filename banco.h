#ifndef BANCO_H
#define BANCO_H

#include <pthread.h>

#define MAX_NOME 50 //tamanho máximo do nome
#define MAX_REG 256 //número máximo de registros no banco
#define LINHA_MAX 256 //tamanho máximo de uma linha
#define RESPOSTA_MAX 512 //tamanho máximo de uma resposta
#define ARQ_DB "banco.txt" //nome do arquivo
#define FIFO_PEDIDO "bd_req.fifo" //nome da FIFO de pedido
#define FIFO_RESPOSTA "bd_resp.fifo" //nome da FIFO de resposta
#define N_THREADS 2 //número de threads do servidor da pra mudar
#define FILA_MAX 32 //tamanho da fila de requisições da pra mudar

typedef struct {
    int id;
    char nome[MAX_NOME];
} Registro;

#endif
