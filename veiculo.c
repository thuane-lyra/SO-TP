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

    // --- CÁLCULO DO TEMPO ---
    // Regra: Velocidade = 1km / 1seg
    // O loop corre 10 vezes (0, 10, 20... 100)
    // Tempo a dormir em cada volta = Distancia Total / 10
    
    int segundos_por_fatia = dist_km / 10;

    // Proteção: Se for muito perto (ex: 5km), 5/10 dá 0. 
    // Forçamos a demorar pelo menos 1 segundo por fatia para se ver algo.
    if (segundos_por_fatia < 1) segundos_por_fatia = 1;

    for (int p = 0; p <= 100; p += 10) {
        estado.percentagem_viagem = p;
        
        // Envia percentagem ao Controlador
        write(STDOUT_FILENO, &estado, sizeof(estado));
        
        // O tempo passa...
        sleep(segundos_por_fatia); 
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