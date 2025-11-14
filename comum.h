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

// constantes que vao ser usadas em bue sitios
#define MAX_USERS 30          // 
#define MAX_VEHICLES 10       // 
#define FIFO_CONTROLADOR "fifo_controlador" // O FIFO onde o admin recebe pedidos
// o fifo e pa criar os name pipes
// as aspas é meio que um nome para que o cliente saiba
// a "que porta bater"
#define TAM_MAX_BUFFER 256    // Defenir tamanho pas strings que seja seguro


//vou fazer os structs já aqui, depois se quiseres
//podemos por outro ficheiro para aqui

//mnsgs cliente servidor 
//enum quer dizer enumeracao
/*Imagina que o Cliente quer enviar um pedido ao Controlador. O computador só entende números de forma eficiente. Tu poderias definir regras na tua cabeça:

0 significa "Login"

1 significa "Logout"

2 significa "Pedir Viagem"*/
typedef enum{
    MSG_LOGIN,
    MSG_LOGOUT,
    MSG_PEDIDO_VIAGEM,
    MSG_CANCELAR_VIAGEM
} TipoMsg;

// struct para clinte po controlador enviado por namepipe
typedef struct{
    pid_t pid_cliente; //pid do cliente para o controlador saber a quem é que vai responder
    char username[20]; //nome do user
    TipoMsg tipo; // O que é que ele quer?
    //opcionais depende da mensagem
    struct{
        int id_viagem;
        char partida[50]; //local 
        char destino[50]; //isto e so informativo
        int distancia; //Estimativa ou valor real ns
    }dados;
}MsgCliente;

//struct para reportar ao controlador 
//via stdout ou pipe anonimo
//pacote de dados que o veículo envia para o controlador para dizer 
//"estou aqui e estou a fazer isto"

typedef struct{
    int veiculo_pid; //pid é process id, este e o do veiculo para o controlador saber que veiculo está a mandar a informacao pelo pipe
    int percentagem_viagem; //nr de 0 a 100
    int servico_id;
    enum{LIVRE, OCUPADO, A_TERMINAR} estado;
    //se estado = 0 LIVRE se = 1 OCUPADO etc..
}EstadoVeiculo;


#endif