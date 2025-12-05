#include "comum.h"

// Variáveis globais para o Signal Handler
int g_id_servico;
char *g_fifo_cliente;

// --- HANDLER PARA SINAL SIGUSR1 (CANCELAR) ---
void handle_cancel(int sig) {
    (void)sig;
    
    // Avisar o cliente que foi cancelado
    int fd = open(g_fifo_cliente, O_WRONLY);
    if (fd != -1) {
        MsgControlador msg;
        msg.tipo = RES_INFO;
        sprintf(msg.mensagem, "Viagem #%d cancelada pelo sistema!", g_id_servico);
        write(fd, &msg, sizeof(msg));
        close(fd);
    }
    
    // Escreve mensagem especial para o Controlador detetar que não chegou ao fim
    EstadoVeiculo estado;
    estado.veiculo_pid = getpid();
    estado.servico_id = g_id_servico;
    estado.percentagem_viagem = -1; // Código especial para cancelado
    estado.estado = A_TERMINAR;
    write(STDOUT_FILENO, &estado, sizeof(estado));

    exit(0); // Morre imediatamente
}

int main(int argc, char *argv[]) {
    if (argc != 4) return 1;
    
    g_id_servico = atoi(argv[1]);
    int dist_km = atoi(argv[2]);
    g_fifo_cliente = argv[3];

    // Armar o sinal: Se receber SIGUSR1, executa handle_cancel
    signal(SIGUSR1, handle_cancel);

    setbuf(stdout, NULL);

    // --- FASE 1 ---
    int fd_cli = open(g_fifo_cliente, O_WRONLY);
    if (fd_cli != -1) {
        MsgControlador msg;
        msg.tipo = RES_INFO;
        sprintf(msg.mensagem, "Taxi chegou! Viagem de %dkm...", dist_km);
        write(fd_cli, &msg, sizeof(msg));
        close(fd_cli);
    }

    sleep(2);

    // --- FASE 2: LOOP ---
    EstadoVeiculo estado;
    estado.veiculo_pid = getpid();
    estado.servico_id = g_id_servico;
    estado.estado = OCUPADO;

    for (int p = 0; p <= 100; p += 10) {
        estado.percentagem_viagem = p;
        write(STDOUT_FILENO, &estado, sizeof(estado));
        sleep(1); 
    }

    // --- FASE 3: FIM NORMAL ---
    estado.percentagem_viagem = 100;
    estado.estado = A_TERMINAR;
    write(STDOUT_FILENO, &estado, sizeof(estado));
    
    fd_cli = open(g_fifo_cliente, O_WRONLY);
    if (fd_cli != -1) {
        MsgControlador msg;
        msg.tipo = RES_INFO;
        sprintf(msg.mensagem, "Viagem concluida! Pagar: %d eur.", dist_km);
        write(fd_cli, &msg, sizeof(msg));
        close(fd_cli);
    }

    return 0;
}