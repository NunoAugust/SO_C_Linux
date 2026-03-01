#ifndef COMUM_H
#define COMUM_H

#define PIPE_CONTROLADOR "pipe_servidor"
#define TAM_MSG 4096
#define PIPE_CLIENTE "pipe_cliente"
#define DEBUG 0
typedef struct pedido
{
    char tipo_msg;
    char identificador[200];
    char mensagem[TAM_MSG];
    int pidCliente;
} pedido;

typedef struct resposta
{
    char tipo_msg;
    char identificador[200];
    char mensagem[TAM_MSG];
} resposta;

typedef enum
{
    AGENDADO,
    ENCURSO,
    FINALIZADO,
    PAGO,
    CANCELADO
} service_status;


typedef struct servico
{
    int nroServico;
    int hora;
    char local[20];
    int distancia;
    service_status estado;
} servico;

typedef struct agendamento
{
    int pid;            // guarda o do cliente? + 1 para guardar o do veiculo
    pthread_t threadId; // thread?
    char nome[200];      // nome do cliente? sim
    servico servico;
} agendamento;

typedef struct cliente
{
    char nome[200];
    int pid;
    servico servicos[10];
    int nServicos;
} cliente;

typedef struct msgVeiculo
{
    // char mensagemDebug[200];
    int pidVeiculo;
    int kmsPercorridos;
    int terminou; // 0 ou 1
} msgVeiculo;

typedef struct veiculo
{
    int pidVeiculo;
    pthread_t pidThread;
    char nomeCliente[10];
    servico servico;
} veiculo;
/*
typedef struct msgControlador
{
    int id;//1
    char mensagem[TAM_MSG];
} msgControlador;

typedef struct msgVeiculo
{
    int id;//2
    char mensagem[TAM_MSG];
} msgVeiculo;

typedef struct msgCliente
{
    int id;//3
    char mensagem[TAM_MSG];
} msgCliente;
*/

#endif