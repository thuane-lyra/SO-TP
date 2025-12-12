#ifndef COMUM_H
#define COMUM_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h> // Adicionado para evitar erros no mkfifo (EEXIST)


#define MAX_USERS 30          
#define MAX_VEICULOS 10       
#define FIFO_CONTROLADOR "fifo_controlador" // O FIFO onde o admin recebe pedidos
// o fifo e pa criar os name pipes
// as aspas é meio que um nome para que o cliente saiba
// a "que porta bater"
#define TAM_MAX_BUFFER 256    // Defenir tamanho pas strings que seja seguro

// Adicionar prefixo para a criação dos Named Pipes de resposta dos Clientes
#define FIFO_CLIENTE_PREFIX "/tmp/cli_" //tarefa1





//0 significa "Login"

//1 significa "Logout"

//2 significa "Pedir Viagem"
typedef enum{
    MSG_LOGIN,
    MSG_LOGOUT,
    MSG_PEDIDO_VIAGEM,
    MSG_CANCELAR_VIAGEM,
    MSG_CONSULTAR_VIAGENS
} TipoMsg;

// struct para clinte para controlador enviado por namepipe
typedef struct{
    pid_t pid_cliente; //pid do cliente para o controlador saber a quem é que vai responder
    char username[20]; 
    TipoMsg tipo; 
    //opcionais depende da mensagem
    struct{
        int id_viagem;
        char partida[50]; 
        char destino[50]; 
        int distancia; 
        int hora;
    }dados;
} MsgCliente;

//struct para reportar ao controlador 
//via stdout ou pipe anonimo
//pacote de dados que o veículo envia para o controlador para dizer 
//"estou aqui e estou a fazer isto"

typedef struct{
    int veiculo_pid; 
    int percentagem_viagem; //nr de 0 a 100
    int servico_id;
    enum{LIVRE, OCUPADO, A_TERMINAR} estado;
    //se estado = 0 LIVRE se = 1 OCUPADO etc..
} EstadoVeiculo;

// 1. MENSAGEM DE RESPOSTA DO CONTROLADOR (Controlador -> Cliente)
typedef enum {
    RES_OK,       // Operação bem-sucedida (ex: Login aceite)
    RES_ERRO,     // Operação falhada (ex: Username já em uso)
    RES_INFO      // Apenas informação (ex: "Viagem #5 agendada")
} TipoResposta;

typedef struct {
    TipoResposta tipo;
    int id_servico; // Se for agendamento OK, inclui o ID atribuído
    char mensagem[TAM_MAX_BUFFER]; // Mensagem detalhada para o utilizador
} MsgControlador;


//controlador registar os utilizadores ativos
typedef struct {
    pid_t pid;
    char username[20];
    //nas proxs tarefas, acrescentar o estado
} UtilizadorAtivo;

#endif