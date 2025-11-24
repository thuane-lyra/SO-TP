#include "comum.h"

int main(int argc, char *argv[]) {
    // O utilizador tem de indicar o username ao arrancar [cite: 123]
    if (argc != 2) {
        printf("Uso: %s <username>\n", argv[0]);
        return 1;
    }

    printf("CLIENTE: A iniciar como %s (PID %d)\n", argv[1], getpid());

    // 1. VERIFICAR CONTROLADOR
    // Antes de tudo, vemos se o FIFO do controlador existe.
    if (access(FIFO_CONTROLADOR, F_OK) != 0) {
        printf("ERRO: O Controlador não está ligado! (FIFO não encontrado)\n");
        return 1;
    }

    // 2. CRIAR O MEU "CORREIO" (FIFO DE RESPOSTA)
    // Cada cliente cria um FIFO único baseado no seu PID (ex: /tmp/cli_1234)
    // É aqui que o controlador vai depositar a resposta "Login Aceite" ou "Recusado".
    char fifo_meu[50];
    sprintf(fifo_meu, "%s%d", FIFO_CLIENTE_PREFIX, getpid()); 
    
    if (mkfifo(fifo_meu, 0666) == -1) {
        perror("Erro ao criar FIFO exclusivo do cliente");
        return 1;
    }

    // 3. ENVIAR PEDIDO DE LOGIN
    MsgCliente msg;
    msg.pid_cliente = getpid(); // Mando o meu PID para ele saber onde responder
    msg.tipo = MSG_LOGIN;
    strncpy(msg.username, argv[1], sizeof(msg.username)-1);
    
    // Abre o FIFO do controlador apenas para escrita
    int fd_server = open(FIFO_CONTROLADOR, O_WRONLY);
    if (fd_server == -1) {
        perror("Erro ao contactar controlador");
        unlink(fifo_meu);
        return 1;
    }
    write(fd_server, &msg, sizeof(msg));
    close(fd_server); // Fecha logo para libertar o recurso

    printf("CLIENTE: Pedido enviado. A aguardar resposta em %s...\n", fifo_meu);

    // 4. AGUARDAR RESPOSTA (BLOQUEANTE)
    // Aqui abrimos o nosso FIFO em modo normal (sem O_NONBLOCK).
    // O programa vai PARAR nesta linha (read) até que o Controlador nos responda.
    int fd_meu = open(fifo_meu, O_RDONLY);
    
    MsgControlador resposta;
    int n = read(fd_meu, &resposta, sizeof(resposta));
    
    if (n == sizeof(resposta)) {
        // (Futuro: Aqui vamos verificar se resposta.tipo == RES_OK)
        printf("CLIENTE: Recebi resposta do servidor!\n");
        printf("       > Tipo: %d\n", resposta.tipo);
        printf("       > Msg: %s\n", resposta.mensagem);
    }

    // 5. LIMPEZA
    close(fd_meu);
    unlink(fifo_meu); // Muito importante: Apagar o meu FIFO antes de sair!
    return 0;
}