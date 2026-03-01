#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include "comum.h"
int running = 1;
int fd_servidor_global = 0;
pedido ped_global;

typedef struct
{
    int pipe_cliente;
    resposta res;
    int pipe_servidor;
} Args;

void handle_signal2(int signal, siginfo_t *info, void *context)
{
    // printf("\n\n[Cliente] - A terminar por Sinal \n\n");

    strcpy(ped_global.mensagem, "terminar");
}

void handle_signal(int signal, siginfo_t *info, void *context)
{
    if (running)
        printf("\n\n[Cliente] - A terminar por Sinal\n\n");
    running = 0;
}

void *recebeMensagens(void *arg)
{
    Args *args = (Args *)arg;
    resposta res = args->res;
    int fd_cliente = args->pipe_cliente;
    int fd_servidor = args->pipe_servidor;
    int nbytes = 0;
    while (running)
    {
        resposta res;
        nbytes = read(fd_cliente, &res, sizeof(resposta));
        if (nbytes == -1)
        {
            if (errno != EPIPE)
            {
                perror("[Cliente] - Erro na leitura do named pipe\n");
            }
        }
        if (strcmp(res.mensagem, "shutdown") == 0)
        {
            union sigval sigValue;
            sigqueue(getpid(), SIGINT, sigValue); // desbloquear o write do main
            running = 0;
        }
        printf("%s", res.mensagem);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    setbuf(stdout, NULL); // desativa o buffer
                          // passa direto ao que estiver ligado ao stdout
    // printf("%s", argv[0]); //nome programa
    char nome[100];
    if (argc != 2)
    {
        printf("Nome para o pipe : ");
        scanf("%s", nome);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); // limpa a estrutura
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handle_signal;
    sigaction(SIGPIPE, &sa, NULL); // Pipe sinal
    sigaction(SIGUSR1, &sa, NULL);

    struct sigaction sa2;
    memset(&sa2, 0, sizeof(sa2)); // limpa a estrutura
    sa2.sa_flags = SA_SIGINFO;
    sa2.sa_sigaction = handle_signal2;
    sigaction(SIGINT, &sa2, NULL); // CTRL + C
    //  Verificar se o pipe ja existe
    if (access(PIPE_CONTROLADOR, F_OK) == -1) // pode ser usado para verificar permissoes tambem
    {
        printf("[Cliente] - O servidor nao esta em execucao");
        exit(EXIT_SUCCESS);
    }
    // Abrir o pipe para escrita
    int fd_servidor = open(PIPE_CONTROLADOR, O_WRONLY);
    if (fd_servidor == -1)
    {
        perror("[Cliente] - Erro na abertura para escrita");
        exit(EXIT_FAILURE);
    }
    fd_servidor_global = fd_servidor;
    int pid = getpid();
    char pipe_cliente[100];

    if (argc != 2) // se NAO for passado nome do pipe como argumento de linha de comandos
    {
        strcpy(pipe_cliente, nome);
    }
    else // se FOI passado nome do pipe como argumento de linha de comandos
    {
        sprintf(pipe_cliente, "%s", argv[1]); //
    }
    if (access(pipe_cliente, F_OK) != -1)
    {
        perror("Cliente já online\n");
        return 0;
    }
    if (mkfifo(pipe_cliente, 0666) == -1)
    {
        perror("[Cliente] - Erro na criacao do pipe\n");
        exit(EXIT_FAILURE);
    }
    int fd_cliente = open(pipe_cliente, O_RDWR);
    if (fd_cliente == -1)
    {
        perror("[Cliente] - Impossivel abrir para leitura");
        unlink(pipe_cliente);
        exit(EXIT_FAILURE);
    }

    // primeiro contacto com o servidor

    pedido pedido_inicial;
    pedido_inicial.tipo_msg = 'c';
    strcpy(pedido_inicial.mensagem, "");
    strcpy(pedido_inicial.identificador, pipe_cliente);
    pedido_inicial.pidCliente = getpid();
    // prepara mensagem global para o CTRL + C
    ped_global.tipo_msg = 'c';
    strcpy(ped_global.mensagem, "terminar");
    strcpy(ped_global.identificador, pipe_cliente);
    ped_global.pidCliente = getpid();
    /////////////////////////////
    int nbytes0 = write(fd_servidor, &pedido_inicial, sizeof(pedido));
    if (nbytes0 == -1)
    {
        if (errno != EPIPE)
        {
            perror("[Cliente] - Erro na escrita do named pipe\n");
        }
    }

    resposta resposta_inicial;
    nbytes0 = read(fd_cliente, &resposta_inicial, sizeof(resposta));
    if (nbytes0 == -1)
    {
        if (errno != EPIPE)
        {
            perror("[Cliente] - Erro na leitura do named pipe\n");
        }
    }

    printf("%s", resposta_inicial.mensagem);

    pedido ped;
    ped.pidCliente = getpid();
    ped.tipo_msg = 'c';
    strcpy(ped.identificador, pipe_cliente);
    resposta res;
    pthread_t leMensagemServidor;
    Args args = {fd_cliente, res, fd_servidor};

    if (pthread_create(&leMensagemServidor, NULL, recebeMensagens, &args) != 0)
    {
        perror("Erro ao criar thread");
        return 1;
    }
    while (running)
    {
        if (DEBUG)
            printf("[Cliente] - Entrou no while\n");

        memset(ped_global.mensagem, 0, sizeof ped_global.mensagem);
        printf("[Cliente] - Insira a mensagem \n");
        fgets(ped_global.mensagem, sizeof(ped_global.mensagem), stdin);
        ped_global.mensagem[strcspn(ped_global.mensagem, "\n")] = '\0';
        // thread escreve no cliente
        int nbytes = write(fd_servidor, &ped_global, sizeof(pedido));
        if (nbytes == -1)
        {
            if (errno != EPIPE)
            {
                perror("[Cliente] - Erro na escrita do named pipe\n");
            }
        }
    }

    printf("[Cliente] - Verificou (%d) e termina\n", fd_cliente);
    close(fd_cliente);
    close(fd_servidor);
    unlink(pipe_cliente);
}