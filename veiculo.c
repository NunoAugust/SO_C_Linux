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

void handle_signal(int signal, siginfo_t *info, void *context)
{
    // printf("\n\n[Veiculo] - Recebeu e vou de vela \n\n");
    running = 0;
}

int main(int argc, char *argv[])
{
    setbuf(stdout, NULL);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); // limpa a estrutura
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handle_signal;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGINT, &sa, NULL); // CTRL + C
    sigaction(SIGPIPE, &sa, NULL);

    int kmsObjetivo = atoi(argv[2]);
    int kmsPercorridos = 0;
    int varDezPor = kmsObjetivo * 0.1;
    msgVeiculo respostaVei;
    respostaVei.pidVeiculo = getpid();
    respostaVei.terminou = 0;
    respostaVei.kmsPercorridos = 0;
    while (running)
    {
        setbuf(stdout, NULL);
        sleep(1);
        ++kmsPercorridos;
        if (kmsObjetivo < 10 || kmsPercorridos % varDezPor == 0 )//avisa a cada 10% da viagem
        {
            respostaVei.kmsPercorridos = kmsPercorridos;
            write(1, &respostaVei, sizeof(msgVeiculo));
        }
        if (kmsPercorridos == kmsObjetivo)
            break;
    }
    respostaVei.kmsPercorridos = kmsPercorridos;
    respostaVei.terminou = 1;
    write(1, &respostaVei, sizeof(respostaVei));
    return 1;
}