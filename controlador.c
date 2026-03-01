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
#include <ctype.h>
#include <sys/wait.h>
#include "comum.h"
#define stringVazia "Vazia"
#define tamMaxClientes 10
cliente clientesStruct[30];
int tamClientes = 0;
veiculo veiculos[10];
int tamVeiculos = 0;
int running = 1;
int tempo_simulado = 0;
agendamento agendamentos[100];
int numAgendamentos = 0;
int numGlobalServico = 1;
int varAmbienteVeiculos = 10; // getEnv // 10 = DEFAULT
int kmsGlobais = 0;

pthread_mutex_t mutexKmsGlobais = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexClientes = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexAgendamentos = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexVeiculos = PTHREAD_MUTEX_INITIALIZER;

void encerra();

int isDebug()
{
    if (DEBUG)
        return 1;
    return 0;
}

void handle_signal(int signal, siginfo_t *info, void *context)
{
    if (isDebug())
        printf("\n\nSinal\n\n");

    if (signal == SIGUSR1)
        running = 0;
    else
        encerra();
}

int verificaCliente(pedido *ped)
{
    for (int i = 0; i < tamClientes; ++i)
        if (strcmp(ped->identificador, clientesStruct[i].nome) == 0)
            return i;
    return -1; // cliente novo
}

void enviaResposta(char pipe_cliente[200], char string[4096])
{
    resposta res;
    res.tipo_msg = 's';
    sprintf(res.identificador, "%d", getpid());
    strcpy(res.mensagem, string);

    int fd_cliente = open(pipe_cliente, O_WRONLY); // Escrita para o Cliente
    if (fd_cliente == -1)
    {
        perror("[Controlador] - Erro na abertura do named pipe do Cliente");
        close(fd_cliente);
    }

    int nbytes = write(fd_cliente, &res, sizeof(resposta));
    if (nbytes == -1)
    {
        if (errno != EPIPE)
        {
            perror("[Controlador] - Erro na escrita do named pipe do Cliente\n");
            close(fd_cliente);
        }
    }
    close(fd_cliente);
}

int verificaInteiro(char *parametro)
{
    if (isDebug())
        printf("[Controlador] - Chamou a funcao\n");
    for (int i = 0; i < strlen(parametro); ++i)
    {
        if (isdigit(parametro[i]) != 0)
            ;
        else
        {
            printf("%s \n", parametro);
            printf("Nao é um numero\n");
            return 0;
        }
    }
    return 1;
}

void removeCliente(int indice)
{
    if (isDebug())
        printf("[Controlador] - Remove o cliente no final da thread");

    cliente aux = clientesStruct[indice];
    clientesStruct[indice] = clientesStruct[tamClientes - 1];
    --tamClientes;
}

void consultaServicos(int indice) // teste fRONTEND
{
    char respostaConsulta[4096] = "";

    sprintf(respostaConsulta, "%s[Controlador]\n %-8s | %-5s | %-7s | %-10s\n",
            respostaConsulta,
            "NumGlobal",
            "HORA",
            "ESTADO",
            "DISTANCIA");

    sprintf(respostaConsulta, "%s %s\n", respostaConsulta, "------------------------------------------------------------------");

    for (int i = 0; i < clientesStruct[indice].nServicos; ++i)
    {
        sprintf(respostaConsulta, "%s %-8d  | %-5d | %-7d | %-10d\n",
                respostaConsulta,
                clientesStruct[indice].servicos[i].nroServico, // NumeroGlobal
                clientesStruct[indice].servicos[i].hora,       // HORA
                clientesStruct[indice].servicos[i].estado,     // ESTADO
                clientesStruct[indice].servicos[i].distancia); // DISTANCIA
    }

    sprintf(respostaConsulta, "%s[Controlador] - Numero total servicos %d \n", respostaConsulta, clientesStruct[indice].nServicos);

    char pipe_cliente[200];
    strcpy(pipe_cliente, clientesStruct[indice].nome);
    enviaResposta(pipe_cliente, respostaConsulta);
}

void consultaAgendamentos()
{
    printf("\n## Agendamentos Atuais\n");

    printf("[Controlador]\n %-10s | %-7s | %-7s | %-5s | %-5s | %-9s | %-8s\n",
           "Cliente",
           "Estado",
           "PID",
           "NrSer",
           "Hora",
           "Distancia",
           "Local");

    printf("%s\n", "--------------------------------------------------------------------");

    for (int i = 0; i < numAgendamentos; ++i)
    {
        printf(" %-10s | %-7d | %-7d | %-5d | %-5d | %-9d | %-8s\n",
               agendamentos[i].nome,
               agendamentos[i].servico.estado,
               agendamentos[i].pid,
               agendamentos[i].servico.nroServico,
               agendamentos[i].servico.hora,
               agendamentos[i].servico.distancia,
               agendamentos[i].servico.local);
    }
    printf("\n[Controlador] - Total de %d agendamentos.\n", numAgendamentos);
}

void cancelaAgendamento(int nrServico)
{
    pthread_mutex_lock(&mutexAgendamentos);
    for (int i = 0; i < numAgendamentos; ++i)
    {
        if (nrServico == agendamentos[i].servico.nroServico)
            agendamentos[i].servico.estado = CANCELADO;
    }
    pthread_mutex_unlock(&mutexAgendamentos);
}

void cancelaServico(char nome[200], int nrServico)
{
    pthread_mutex_lock(&mutexClientes);
    if (isDebug)
        printf("CancelaServico %s %d", nome, nrServico);

    int servicosCancelados = 0;
    for (int i = 0; i < tamClientes; ++i)
    {
        if (strcmp(nome, clientesStruct[i].nome) == 0)
        {
            if (isDebug)
                printf("Encontrou o cliente %s\n", nome);
            for (int j = 0; j < clientesStruct[i].nServicos; ++j)
            {
                if (nrServico == 0 && clientesStruct[i].servicos[j].estado == AGENDADO)
                { // cliente cancela todos os seus servicos agendados
                    cancelaAgendamento(clientesStruct[i].servicos[j].nroServico);
                    ++servicosCancelados;
                }
                else if (nrServico == clientesStruct[i].servicos[j].nroServico && clientesStruct[i].servicos[j].estado == AGENDADO)
                { // cancela so um
                    if (isDebug)
                        printf("Vai cancelar %d\n", nrServico);

                    strcpy(clientesStruct[i].servicos[j].local, clientesStruct[i].servicos[clientesStruct[i].nServicos - 1].local);
                    clientesStruct[i].servicos[j].hora = clientesStruct[i].servicos[clientesStruct[i].nServicos - 1].hora;
                    clientesStruct[i].servicos[j].distancia = clientesStruct[i].servicos[clientesStruct[i].nServicos - 1].distancia;
                    clientesStruct[i].servicos[j].nroServico = clientesStruct[i].servicos[clientesStruct[i].nServicos - 1].nroServico;
                    clientesStruct[i].servicos[j].estado = clientesStruct[i].servicos[clientesStruct[i].nServicos - 1].estado;
                    cancelaAgendamento(nrServico);
                    clientesStruct[i].nServicos--;
                    break; // ja cancelou o servico a cancelar
                }
            }
            if (nrServico == 0)
            {
                clientesStruct[i].nServicos -= servicosCancelados;
            }
            break; // já encontrou o cliente
        }
    }
    pthread_mutex_unlock(&mutexClientes);
}

void removeVeiculo(int forkARemover)
{
    pthread_mutex_lock(&mutexVeiculos);
    for (int i = 0; i < tamVeiculos; ++i)
    {
        if (forkARemover == veiculos[i].pidVeiculo)
        {
            veiculo aux = veiculos[i];
            veiculos[i] = veiculos[tamVeiculos - 1];
            --tamVeiculos;
        }
    }
    pthread_mutex_unlock(&mutexVeiculos);
}

void atualizaEstadoServico(char nome[200], int nServico, int estadoEnum)
{
    pthread_mutex_lock(&mutexClientes);
    for (int i = 0; i < tamClientes; ++i)
    {
        if (strcmp(clientesStruct[i].nome, nome) == 0)
        {
            for (int j = 0; j < clientesStruct[i].nServicos; ++j)
            {
                if (clientesStruct[i].servicos[j].nroServico == nServico)
                    clientesStruct[i].servicos[j].estado = estadoEnum;
            }
        }
    }
    pthread_mutex_unlock(&mutexClientes);

    pthread_mutex_lock(&mutexAgendamentos);
    for (int k = 0; k < numAgendamentos; ++k)
    {
        if (agendamentos[k].servico.nroServico == nServico)
            agendamentos[k].servico.estado = estadoEnum;
    }
    pthread_mutex_unlock(&mutexAgendamentos);
}

void *enviaThreadVeiculo(void *arg)
{
    agendamento *serVeiculo = (agendamento *)arg;
    char destino[10];
    char distancia[10];
    strcpy(destino, serVeiculo->servico.local);
    sprintf(distancia, "%d", serVeiculo->servico.distancia);
    printf("Local %s, distancia no inicio da thread: %d\n", serVeiculo->servico.local, serVeiculo->servico.distancia);
    int canal[2];
    int canal_res[2];

    pipe(canal);
    pipe(canal_res);

    int pidFork;
    if ((pidFork = fork()) == 0) // FILHO
    {
        // Redireciona stdin e stdout
        // close(0);      // fecha stdin
        // dup(canal[0]); // stdin = canal[0]
        close(canal[0]);
        close(canal[1]); // stdout

        close(1);
        dup(canal_res[1]); // passa a ser este o stdout
        close(canal_res[0]);
        close(canal_res[1]);
        // close(0); // nao vai ler nada de lado nenhum, recebe por argumentos
        // sera boa pratica fechar? nao
        execl("./veiculo", "veiculo", destino, distancia, NULL);
        perror("execl falhou");
        exit(1);
    }
    else
    {
        msgVeiculo respostaVei;
        respostaVei.terminou = 0;
        int kmsVeiculo = 0;
        int nbytes;
        pthread_t idThread = pthread_self();
        for (int i = 0; i < tamVeiculos; ++i)
        {
            if (veiculos[i].pidThread == idThread)
            {
                veiculos[i].pidVeiculo = pidFork;
                break;
            }
        }
        close(canal[0]);
        close(canal[1]);
        close(canal_res[1]); // le do canal res[0]

        while (!respostaVei.terminou)
        {
            nbytes = read(canal_res[0], &respostaVei, sizeof(msgVeiculo));
            if (nbytes == 0)
                break; // pipe fechado -> fim
            if (nbytes < 0)
            {
                perror("read");
                break;
            }

            // printf("AQUI APOS O READ \n %s\n", respostaVei.mensagemDebug);
            printf("Percorreu [%d] %d kms\n", respostaVei.pidVeiculo, respostaVei.kmsPercorridos);
            kmsVeiculo = respostaVei.kmsPercorridos;
        }

        close(canal_res[0]);
        char resposta[4096];
        sprintf(resposta, "Terminou[%d] a viagem com %d kms \n", respostaVei.pidVeiculo, respostaVei.kmsPercorridos);
        char nome[200];
        int indiceVeiculo;
        for (int i = 0; i < tamVeiculos; ++i)
        {
            if (pidFork == veiculos[i].pidVeiculo)
            {
                indiceVeiculo = i;
                break;
            }
        }
        pthread_mutex_lock(&mutexKmsGlobais);
        kmsGlobais += kmsVeiculo;
        pthread_mutex_unlock(&mutexKmsGlobais);

        strcpy(nome, serVeiculo->nome);
        enviaResposta(nome, resposta);

        if (kmsVeiculo == serVeiculo->servico.distancia) // senao for igual é porque recebeu sinal
            waitpid(pidFork, NULL, 0);

        atualizaEstadoServico(serVeiculo->nome, serVeiculo->servico.nroServico, FINALIZADO);
        removeVeiculo(pidFork);
    }
}

void lancaVeiculo(agendamento *agendamentoALancar) // recebe agendamento e lanca thread
{
    // lanca thread
    agendamento *ptrAgenda;
    ptrAgenda = agendamentoALancar;
    veiculos[tamVeiculos].servico.distancia = ptrAgenda->servico.distancia;
    veiculos[tamVeiculos].servico.hora = ptrAgenda->servico.hora;
    veiculos[tamVeiculos].servico.nroServico = ptrAgenda->servico.nroServico;
    if (pthread_create(&veiculos[tamVeiculos].pidThread, NULL, enviaThreadVeiculo, ptrAgenda) != 0)
    {
        printf("Erro na criacao da thread do taxi\n");
        return;
    }
    atualizaEstadoServico(agendamentoALancar->nome, agendamentoALancar->servico.nroServico, ENCURSO);
    ++tamVeiculos;
}

void mostraClientes()
{
    if (tamClientes == 0)
        printf("Nao existem clientes online\n");
    else
    {

        printf("\n--- Utilizadores Online ---\n");

        printf("%-15s | %-10s | %-10s | %-10s\n",
               "NOME", "ESPERA (A)", "VIAGEM (V)", "PAGAM. (P)");

        printf("----------------|------------|------------|------------\n");

        for (int i = 0; i < tamClientes; ++i)
        {
            int emEspera = 0;
            int emViagem = 0;
            int emPagamento = 0;

            for (int j = 0; j < clientesStruct[i].nServicos; ++j)
            {
                if (clientesStruct[i].servicos[j].estado == AGENDADO)
                {
                    emEspera = 1;
                }
                if (clientesStruct[i].servicos[j].estado == ENCURSO)
                {
                    emViagem = 1;
                }
                if (clientesStruct[i].servicos[j].estado == FINALIZADO)
                {
                    emPagamento = 1;
                }
            }

            printf("%-15s | %-10s | %-10s | %-10s\n",
                   clientesStruct[i].nome,
                   emEspera ? "A" : "-",
                   emViagem ? "V" : "-",
                   emPagamento ? "P" : "-");
        }

        printf("----------------------------------------------------\n");
        printf("(A=Agendado/Espera, V=Em Viagem, P=Pendente Pagamento)\n");
    }
}

void cancelaNrServicoCliente(int nrServico, char nome[200])
{
    for (int i = 0; i < tamClientes; ++i)
    {
        if (strcmp(nome, clientesStruct[i].nome) == 0)
        {
            for (int j = 0; j < clientesStruct[i].nServicos; ++j)
            {
                if (nrServico == clientesStruct[i].servicos[j].nroServico)
                    cancelaServico(clientesStruct[i].nome, clientesStruct[i].servicos[j].nroServico); // temos o indice do cliente e o indice do servico no array do cliente
            }
        }
    }
}

void adminEliminaAgendamento(int nrServico)
{
    if (nrServico == 0)
    {
        for (int i = 0; i < numAgendamentos; ++i)
            cancelaAgendamento(agendamentos[i].servico.nroServico);

        pthread_mutex_lock(&mutexVeiculos);
        for (int i = tamVeiculos - 1; i >= 0; i--)
        {
            union sigval sigValue;
            sigqueue(veiculos[i].pidVeiculo, SIGUSR1, sigValue);
            waitpid(veiculos[i].pidVeiculo, NULL, 0);
        }
        pthread_mutex_unlock(&mutexVeiculos);

        pthread_mutex_lock(&mutexClientes);
        for (int i = 0; i < tamClientes; ++i)
            clientesStruct[i].nServicos = 0;

        pthread_mutex_unlock(&mutexClientes);
    }
    else
    {

        for (int i = 0; i < numAgendamentos; ++i)
            if (nrServico == agendamentos[i].servico.nroServico)
            {

                cancelaAgendamento(agendamentos[i].servico.nroServico);                            ///////
                cancelaNrServicoCliente(agendamentos[i].servico.nroServico, agendamentos[i].nome); //////
                pthread_mutex_lock(&mutexVeiculos);
                for (int i = tamVeiculos - 1; i >= 0; i--)
                {
                    if (veiculos[i].servico.nroServico == nrServico)
                    {
                        union sigval sigValue;
                        sigqueue(veiculos[i].pidVeiculo, SIGUSR1, sigValue);
                        waitpid(veiculos[i].pidVeiculo, NULL, 0);
                    }
                }
                pthread_mutex_unlock(&mutexVeiculos);
            }
    }
}

void encerra()
{

    /* for (int i = tamVeiculos; i >= 0; i--)
     {
         union sigval sigValue;
         sigqueue(veiculos[i].pidVeiculo, SIGINT, sigValue);
         waitpid(veiculos[i].pidVeiculo, NULL, 0);
     }*/
    adminEliminaAgendamento(0);

    for (int i = tamClientes - 1; i >= 0; i--)
    {
        char resposta[4096] = "O servidor vai encerrar";
        enviaResposta(clientesStruct[i].nome, resposta);
        union sigval sigValue;
        sigqueue(clientesStruct[i].pid, SIGUSR1, sigValue); // o sigint sera tratado de forma diferente no cliente
        removeCliente(i);
    }

    union sigval sigValue;
    sigqueue(getpid(), SIGUSR1, sigValue); // sinal para o proprio controlador running =0 e desbloquear operacoes bloqueantes
}

void listaVeiculos()
{
    if (tamVeiculos == 0)
        printf("Nao existem veiculos em viagem\n");
    for (int i = 0; i < tamVeiculos; ++i)
    {

        int horaAtual = tempo_simulado;
        int horaInicio = veiculos[i].servico.hora;
        int kmsServico = veiculos[i].servico.distancia;
        float total = (horaAtual * 1.0 - horaInicio * 1.0) / kmsServico * 1.0;
        printf("Veiculo[%d] percorreu: %.1f %%\n", veiculos[i].pidVeiculo, total * 100);
    }
}

void analisaComandoTeclado(char comando[4096])
{
    char parametros[5][50];

    int i = 0; // índice da frase
    int p = 0; // índice da palavra atual
    int j = 0; // índice da letra atual

    for (int k = 0; k < 5; ++k)
        strcpy(parametros[k], stringVazia);

    while (comando[i] != '\0' && p < 5)
    {
        // ignorar espaços
        while (comando[i] == ' ')
            i++;

        // fim da string
        if (comando[i] == '\0')
            break;

        // copiar palavra
        j = 0;
        while (comando[i] != ' ' && comando[i] != '\0' && j < 49)
            parametros[p][j++] = comando[i++];

        parametros[p][j] = '\0'; // terminar string

        p++; // próxima palavra
    }

    if (strcmp(parametros[0], "listar") == 0)
    {
        if (strcmp(parametros[1], stringVazia) == 0)
            consultaAgendamentos();
    }
    else if (strcmp(parametros[0], "utiliz") == 0)
    {
        if (strcmp(parametros[1], stringVazia) == 0)
            mostraClientes();
    }
    else if (strcmp(parametros[0], "frota") == 0)
    {
        if (strcmp(parametros[1], stringVazia) == 0)
            listaVeiculos();
    }
    else if (strcmp(parametros[0], "cancelar") == 0)
    {
        if (strcmp(parametros[2], stringVazia) == 0)
        {
            if (verificaInteiro(parametros[1]) == 1)
            {
                int valor = atoi(parametros[1]);
                adminEliminaAgendamento(valor);
            }
        }
    }
    else if (strcmp(parametros[0], "km") == 0)
    {
        if (strcmp(parametros[1], stringVazia) == 0)
            printf("Kms Totais Percorridos %d \n", kmsGlobais);
    }
    else if (strcmp(parametros[0], "hora") == 0)
    {
        if (strcmp(parametros[1], stringVazia) == 0)
            printf("São %d\n", tempo_simulado);
    }
    else if (strcmp(parametros[0], "terminar") == 0)
    {
        encerra();
    }
    else
        printf("Comando invalido\n");
}

void clienteAtivo()
{
    for (int i = 0; i < tamClientes; ++i)
    {
        if (access(clientesStruct[i].nome, F_OK) != -1)
            ;
        else
        {
            removeCliente(i);
            --i;
        }
    }
}

int possivelAgendar(servico servicoCliente)
{
    int p1 = servicoCliente.hora;
    int p2 = servicoCliente.hora + servicoCliente.distancia;
    int veiculosOcupados = 0;

    for (int i = 0; i < numAgendamentos && veiculosOcupados != varAmbienteVeiculos; ++i)
    {
        if (agendamentos[i].servico.estado != AGENDADO && agendamentos[i].servico.estado != ENCURSO)
            continue;

        int serIni = agendamentos[i].servico.hora;
        int serFim = agendamentos[i].servico.hora + agendamentos[i].servico.distancia;

        if (p1 >= serIni && p1 <= serFim)
        {
            ++veiculosOcupados;
            continue;
        }
        else if (p2 >= serIni && p2 <= serFim)
        {
            ++veiculosOcupados;
            continue;
        }
        else if (serIni >= p1 && serIni <= p2)
        {
            ++veiculosOcupados;
            continue;
        }
        else if (serFim >= p1 && serFim <= p2)
        {
            ++veiculosOcupados;
            continue;
        }
    }

    if (veiculosOcupados == varAmbienteVeiculos)
        return 0;

    return 1;
}

void verificaHoraServico() // verifica se esta na hora de lancar veiculo
{

    for (int i = 0; i < numAgendamentos; ++i)
    {
        if (agendamentos[i].servico.hora <= tempo_simulado && agendamentos[i].servico.estado == AGENDADO)
        {
            char respostaPositiva[4096] = "Carro enviado com sucesso\n";
            char pipe_cliente[200];
            strcpy(pipe_cliente, agendamentos[i].nome);
            enviaResposta(pipe_cliente, respostaPositiva);
            lancaVeiculo(&agendamentos[i]); // lanca veiculo
        }
    }
}

void *analisaTempoFuncao(void *arg)
{
    while (running)
    {
        sleep(1); // espera 1 segundo
        verificaHoraServico();
        if (tempo_simulado % 10 == 0)
        {
            if (isDebug())
                printf("[Controlador] - 10s \n");
            clienteAtivo();
        }
    }
    return NULL;
}

void *contador_thread(void *arg)
{
    while (running)
    {
        sleep(1);         // espera 1 segundo
        tempo_simulado++; // incrementa o contador
        if (DEBUG)
            printf("[contador Thread] %d\n", tempo_simulado);
    }
    return NULL;
}

int possivelPCliente(char nome[200], servico servico)
{

    for (int i = 0; i < tamClientes; ++i)
    {
        if (strcmp(nome, clientesStruct[i].nome) == 0)
        {
            int p1 = servico.hora;
            int p2 = servico.hora + servico.distancia;
            int veiculosOcupados = 0;

            for (int j = 0; j < clientesStruct[i].nServicos; ++j)
            {
                int serIni = clientesStruct[i].servicos[j].hora;
                int serFim = clientesStruct[i].servicos[j].hora + clientesStruct[i].servicos[j].distancia;
                if (DEBUG)
                    printf("Dentro do ciclo %d iniSer, %d fimSer, %d novoServi %d fimNovoSer\n", serIni, serFim, p1, p2);
                if (p1 >= serIni && p1 <= serFim)
                {
                    return 0;
                }
                else if (p2 >= serIni && p2 <= serFim)
                {
                    return 0;
                }
                else if (serIni >= p1 && serIni <= p2)
                {
                    return 0;
                }
                else if (serFim >= p1 && serFim <= p2)
                {
                    return 0;
                }
            }
        }
    }
    return 1;
}

void removeServicoCliente(char nome[200], int nrServico)
{
    pthread_mutex_lock(&mutexClientes);

    for (int i = 0; i < tamClientes; i++)
    {
        if (strcmp(nome, clientesStruct[i].nome) == 0)
        {
            for (int j = 0; j < clientesStruct[i].nServicos; ++j)
            {
                if (nrServico == clientesStruct[i].servicos[j].nroServico)
                {
                    strcpy(clientesStruct[i].servicos[j].local, clientesStruct[i].servicos[clientesStruct[i].nServicos - 1].local);
                    clientesStruct[i].servicos[j].hora = clientesStruct[i].servicos[clientesStruct[i].nServicos - 1].hora;
                    clientesStruct[i].servicos[j].distancia = clientesStruct[i].servicos[clientesStruct[i].nServicos - 1].distancia;
                    clientesStruct[i].servicos[j].nroServico = clientesStruct[i].servicos[clientesStruct[i].nServicos - 1].nroServico;
                    clientesStruct[i].servicos[j].estado = clientesStruct[i].servicos[clientesStruct[i].nServicos - 1].estado;
                    clientesStruct[i].nServicos--;
                    break;
                }
            }
            break;
        }
    }
    pthread_mutex_unlock(&mutexClientes);
}

void pagaServicos(char nome[200])
{
    for (int i = 0; i < tamClientes; ++i)
    {
        if (strcmp(nome, clientesStruct[i].nome) == 0)
        {
            for (int j = 0; j < clientesStruct[i].nServicos; ++j)
            {
                if (clientesStruct[i].servicos[j].estado == FINALIZADO)
                {
                    atualizaEstadoServico(nome, clientesStruct[i].servicos[j].nroServico, PAGO); // agendamento e no cliente
                    removeServicoCliente(nome, clientesStruct[i].servicos[j].nroServico);
                    --j;
                }
            }
        }
    }
}

int verificaEmViagem(pedido *ped)
{
    int indice = verificaCliente(ped);
    for (int j = 0; j < clientesStruct[indice].nServicos; ++j)
    {
        if (clientesStruct[indice].servicos[j].estado == ENCURSO)
        {
            return 0;
        }
    }
    return 1;
}

int verificaPendentes(pedido *ped)
{
    int indice = verificaCliente(ped);
    for (int j = 0; j < clientesStruct[indice].nServicos; ++j)
    {
        if (clientesStruct[indice].servicos[j].estado == AGENDADO)
        {
            return 0;
        }
    }
    return 1;
}

int verificaPagamentos(pedido *ped)
{
    int indice = verificaCliente(ped);
    for (int j = 0; j < clientesStruct[indice].nServicos; ++j)
    {
        if (clientesStruct[indice].servicos[j].estado == FINALIZADO)
        {
            return 0;
        }
    }
    return 1;
}

int jaPassou(char nome[200], servico servico)
{
    if (servico.hora < tempo_simulado)
        return 0;
    return 1;
}

int analisaComando(pedido *ped)
{
    char parametros[5][50];

    int i = 0; // índice da frase
    int p = 0; // índice da palavra atual
    int j = 0; // índice da letra atual
    for (int k = 0; k < 5; ++k)
        strcpy(parametros[k], stringVazia);

    while (ped->mensagem[i] != '\0' && p < 5)
    {
        // ignorar espaços
        while (ped->mensagem[i] == ' ')
            i++;

        // fim da string
        if (ped->mensagem[i] == '\0')
            break;

        // copiar palavra
        j = 0;
        while (ped->mensagem[i] != ' ' && ped->mensagem[i] != '\0' && j < 49)
            parametros[p][j++] = ped->mensagem[i++];

        parametros[p][j] = '\0'; // terminar string

        p++; // próxima palavra
    }
    if (strcmp("pagar", parametros[0]) == 0)
    {
        if (strcmp(parametros[1], stringVazia) != 0)
            return 0;

        pagaServicos(ped->identificador);
        return 1;
    }
    else if (strcmp("agendar", parametros[0]) == 0)
    {
        if (strcmp(parametros[4], stringVazia) == 0)
        {
            for (int i = 1; i < 4; ++i)
            {
                if (strcmp(parametros[i], stringVazia) == 0)
                {
                    return 0;
                }
                else
                {
                    // verifica se e numero
                    if (i == 1 || i == 3)
                    {
                        if (verificaInteiro(parametros[i]) == 1)
                            ;
                        else
                        {
                            if (isDebug())
                                printf("[Controlador] - Nao e numero: %s \n", parametros[i]);
                        }
                    }
                }
            }

            servico servico;
            servico.nroServico = numGlobalServico;
            ++numGlobalServico;
            servico.hora = atoi(parametros[1]);
            sprintf(servico.local, "%s\0", parametros[2]);
            servico.distancia = atoi(parametros[3]);
            // printf("Hora: %d, Local: %s, Distancia: %d", servico.hora, servico.local, servico.distancia);

            int indice = verificaCliente(ped);    // indice do cliente
            int check = possivelAgendar(servico); // verifica se nao ha conflito
            int check2 = possivelPCliente(ped->identificador, servico);
            int check3 = jaPassou(ped->identificador, servico);
            if (!check || !check2 || !check3)
            {
                if (!check3)
                {
                    char respostaNaoHaVaga[4096] = "Nao temos disponibilidade de viajar no passado\n";
                    enviaResposta(ped->identificador, respostaNaoHaVaga); // identificador é o nome do cliente? sim
                }
                else if (!check2)
                {
                    char respostaNaoHaVaga[4096] = "O seu pedido sobre poem se a outro seu servico\n";
                    enviaResposta(ped->identificador, respostaNaoHaVaga); // identificador é o nome do cliente? sim
                }
                else if (!check)
                {
                    char respostaNaoHaVaga[4096] = "Nao temos disponibilidade para esse altura\n";
                    enviaResposta(ped->identificador, respostaNaoHaVaga); // identificador é o nome do cliente? sim
                }
            }
            else
            {
                if (isDebug())
                    printf("Cliente numero: %d\n", indice);

                sprintf(clientesStruct[indice].servicos[clientesStruct[indice].nServicos].local, "%s\0", parametros[2]);
                clientesStruct[indice].servicos[clientesStruct[indice].nServicos].hora = atoi(parametros[1]);
                clientesStruct[indice].servicos[clientesStruct[indice].nServicos].distancia = atoi(parametros[3]);
                clientesStruct[indice].servicos[clientesStruct[indice].nServicos].nroServico = servico.nroServico;
                clientesStruct[indice].servicos[clientesStruct[indice].nServicos].estado = AGENDADO;
                clientesStruct[indice].nServicos++;

                // colocar tambem nos agendamentos do controlador
                strcpy(agendamentos[numAgendamentos].nome, clientesStruct[indice].nome);
                // printf("PID: %d", (*ped).pidCliente);
                agendamentos[numAgendamentos].pid = ped->pidCliente;
                agendamentos[numAgendamentos].servico.distancia = servico.distancia;
                agendamentos[numAgendamentos].servico.hora = servico.hora;
                agendamentos[numAgendamentos].servico.nroServico = servico.nroServico;
                strcpy(agendamentos[numAgendamentos].servico.local, servico.local);
                agendamentos[numAgendamentos].servico.estado = AGENDADO;
                numAgendamentos++;

                if (isDebug())
                    printf("\nChegou ao fim do comando agendar\n");
            }
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else if (strcmp("cancelar", parametros[0]) == 0)
    {
        if (strcmp(parametros[2], stringVazia) == 0)
        {
            if (verificaInteiro(parametros[1]))
            {
                int servicoCancelar = atoi(parametros[1]);
                cancelaServico(ped->identificador, servicoCancelar);
            }
        }
        return 1;
    }
    else if (strcmp("consultar", parametros[0]) == 0)
    {
        int indice = verificaCliente(ped);
        consultaServicos(indice);
        // consultaAgendamentos(); //para debug no controlador
        return 2; // para nao reenviar uma segunda mensagem a informar o comando executado
    }
    else if (strcmp("terminar", parametros[0]) == 0)
    {
        int check1 = verificaPagamentos(ped);
        int check2 = verificaPendentes(ped);
        int check3 = verificaEmViagem(ped);
        char mensagem[4096] = "";
        if (!check1 || !check2 || !check3)
        {
            if (!check1)
                strcat(mensagem, "Paga as viagens caloteiro\n");
            if (!check2)
                strcat(mensagem, "Ainda existem agendamentos, cancela ou usufrui\n");
            if (!check3)
                strcat(mensagem, "So se saires pela janela\n");

            enviaResposta(ped->identificador, mensagem);
            return 1;
        }
        strcat(mensagem, "shutdown");
        enviaResposta(ped->identificador, mensagem);
        removeCliente(verificaCliente(ped));
        return 1;
    }

    return 0;
}

void analisaMensagem(pedido *ped, int indice)
{
    if (strcmp("", ped->mensagem) == 0) // primeira mensagem do cliente
    {
        // mensagem inicial
        char respostaInicial[4096];
        sprintf(respostaInicial, "Bem-vindo ao UberISEC, sao agora %d momentos, insira o seu comando \n", tempo_simulado);
        enviaResposta(ped->identificador, respostaInicial);
    }
    else
    {
        int analisa = analisaComando(ped);
        char resposta[4096] = "Comando Executado\n";
        if (analisa == 1)
            enviaResposta(ped->identificador, resposta);
        char respotaNula[4096] = "Comando invalido\n";
        if (analisa == 0)
            enviaResposta(ped->identificador, respotaNula);
    }
}

void adicionaCliente(pedido *ped)
{
    if (tamClientes < tamMaxClientes) // max 2 para simular o maximo de clientes, alterar para 30
    {
        strcpy(clientesStruct[tamClientes].nome, ped->identificador);
        clientesStruct[tamClientes].nServicos = 0;
        clientesStruct[tamClientes].pid = ped->pidCliente;
        ++tamClientes;
        if (isDebug())
            printf("[Controlador] - Adiciona cliente %s \n", ped->identificador);
    }
    else
    {
        int indice = verificaCliente(ped);
        char respostaInicial[4096];
        sprintf(respostaInicial, "Bem-vindo ao UberISEC, numero maximo de clientes, tente mais tarde \n", tempo_simulado);
        enviaResposta(ped->identificador, respostaInicial);
        char respostaShutdown[4096];
        sprintf(respostaShutdown, "shutdown");
        enviaResposta(ped->identificador, respostaShutdown);
    }
}

void *atendeCliente(void *args)
{
    int *ponteiro = (int *)args;
    int fd = *ponteiro;
    pedido ped;
    while (running)
    {
        // Ler do named pipe
        int nbytes = read(fd, &ped, sizeof(pedido));
        if (nbytes == -1)
        {
            if (errno != EPIPE)
            {
                perror("[Controlador] - Erro na leitura do named pipe");
            }
            break;
        }
        if (strcmp(ped.mensagem, "t") == 0)
            break;
        if (verificaCliente(&ped) == -1 && strcmp("terminar", ped.mensagem) != 0) // verifica se o cliente ja existe e se nao eh terminar
            adicionaCliente(&ped);                                                // Guarda o novo cliente no array
        // analisa mensagem do cliente
        int indice = verificaCliente(&ped);
        if (indice != -1)                  // verifica se o cliente existe
            analisaMensagem(&ped, indice); // analisa a mensagem e responde
    }
}

int main()
{
    // char variavel[200] = "varAmbVeiculo";

    if (getenv("NVEICULOS") != NULL)
        if (atoi(getenv("NVEICULOS")) != 0)
            if (atoi(getenv("NVEICULOS")) <= 10)
                varAmbienteVeiculos = atoi(getenv("NVEICULOS"));
    if (isDebug)
        printf("Variavel de ambiente: %d\n", varAmbienteVeiculos);
    // inicia a thread do tempo
    pthread_t tid;
    // criar a thread
    if (pthread_create(&tid, NULL, contador_thread, NULL) != 0)
    {
        perror("Erro ao criar thread");
        return 1;
    }

    pthread_t analisaTempo;
    if (pthread_create(&analisaTempo, NULL, analisaTempoFuncao, NULL))
    {
        perror("Erro ao criar thread");
        return 1;
    }

    printf("Thread de tempo iniciada!\n");
    setbuf(stdout, NULL);
    resposta res;
    // msgControlador.pid = getpid();
    res.tipo_msg = 's';
    // estrutura sinal;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); // limpa a estrutura
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handle_signal;
    sigaction(SIGINT, &sa, NULL);  // CTRL + C
    sigaction(SIGPIPE, &sa, NULL); // Pipe sinal
    sigaction(SIGUSR1, &sa, NULL);

    // verifica se o named pipe ja existe
    if (access(PIPE_CONTROLADOR, F_OK) == 0)
    {
        printf("[Controlador] - O named pipe já existe");
        // unlink(PIPE_CONTROLADOR); // Caso ja existe mas nao seja para existir
        //  descomentar a linha
        exit(EXIT_SUCCESS);
    }
    // Cria o named pipe
    if (mkfifo(PIPE_CONTROLADOR, 0666) == -1)
    {
        perror("[Controlador] - Erro na criacao do named pipe");
        exit(EXIT_FAILURE);
    }
    // Abrir o named pipe para leitura

    int fd = open(PIPE_CONTROLADOR, O_RDWR); // Leitura e escrita para nao ficar bloqueado
    if (fd == -1)
    {
        perror("[Controlador] - Erro na abertura do named pipe");
        unlink(PIPE_CONTROLADOR);
        exit(EXIT_FAILURE);
    }

    pthread_t threadAtendeCliente;
    // criar a thread
    if (pthread_create(&threadAtendeCliente, NULL, atendeCliente, &fd) != 0)
    {
        perror("Erro ao criar thread");
        return 1;
    }

    pedido ped;
    ped.tipo_msg = 's';

    while (running)
    {
        fflush(stdout);
        setbuf(stdin, NULL);
        char teclado[4096];
        if (fgets(teclado, sizeof(teclado), stdin) != NULL)
        {
            teclado[strcspn(teclado, "\n")] = '\0';
            analisaComandoTeclado(teclado);
        }
        if (DEBUG)
            printf("Imprimiu no teclado [%s]\n", teclado);
    };

    strcpy(ped.mensagem, "t");
    write(fd, &ped, sizeof(pedido));

    printf("[Controlador] - Termina\n");
    close(fd);                // Fechar o named pipe
    unlink(PIPE_CONTROLADOR); // Apagar o named pipe

    // para a thread do tempo

    running = 0; // parar thread

    pthread_join(tid, NULL);
    pthread_join(analisaTempo, NULL);
    // pthread_join(tidTeclado, NULL);
    pthread_join(threadAtendeCliente, NULL);
    for (int i = 0; i < tamVeiculos; ++i)
    {
        pthread_join(veiculos[i].pidThread, NULL);
    }

    printf("Thread terminada!\n");
    pthread_mutex_destroy(&mutexKmsGlobais);
    pthread_mutex_destroy(&mutexAgendamentos);
    pthread_mutex_destroy(&mutexClientes);
    pthread_mutex_destroy(&mutexVeiculos);
    return 0;
}
