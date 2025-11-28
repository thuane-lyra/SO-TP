#include "comum.h"

int main(int argc, char *argv[]) {
    // Validação de argumentos (recebidos do execvp do controlador)
    if (argc != 4) {
        fprintf(stderr, "ERRO VEICULO: Args invalidos.\n");
        return 1;
    }
    
    int id_servico = atoi(argv[1]);
    int dist_km = atoi(argv[2]); // Distancia total
    char *fifo_cliente = argv[3];

    // Configurar o comportamento do stdout para não ter buffer
    // (Isto garante que os dados saem IMEDIATAMENTE para o pipe)
    setbuf(stdout, NULL);

    // --- FASE 1: CHEGADA AO CLIENTE ---
    // Avisar o cliente que o táxi chegou (escreve no FIFO do cliente)
    // Nota: Numa versão final, aqui esperariamos pela resposta "CMD_ENTRAR"
    int fd_cli = open(fifo_cliente, O_WRONLY);
    if (fd_cli != -1) {
        MsgControlador msg;
        msg.tipo = RES_INFO;
        sprintf(msg.mensagem, "Taxi chegou! A iniciar viagem de %dkm...", dist_km);
        write(fd_cli, &msg, sizeof(msg));
        close(fd_cli);
    }

    // Simular tempo de entrada do passageiro
    sleep(2);

    // --- FASE 2: A VIAGEM (Loop de Telemetria) ---
    EstadoVeiculo estado;
    estado.veiculo_pid = getpid();
    estado.servico_id = id_servico;
    estado.estado = OCUPADO; // [cite: 43] Cliente entrou

    // O enunciado diz: reportar a cada 10% da distancia [cite: 43]
    // Velocidade = 1 unidade de tempo [cite: 170]
    for (int p = 0; p <= 100; p += 10) {
        estado.percentagem_viagem = p;
        
        // 1. Enviar telemetria para o Controlador (via STDOUT -> Pipe)
        // Escrevemos a struct inteira em binário
        write(STDOUT_FILENO, &estado, sizeof(estado));

        // 2. Simular o tempo a passar
        // Quanto maior a distância, mais tempo demora cada 10%? 
        // Para simplificar e testar rápido, usamos 1 segundo por cada 10%
        sleep(1); 
    }

    // --- FASE 3: FIM DA VIAGEM ---
    estado.percentagem_viagem = 100;
    estado.estado = A_TERMINAR; // [cite: 43] Cliente saiu
    write(STDOUT_FILENO, &estado, sizeof(estado));
    
    // Avisar o cliente que acabou (opcional para agora)
    fd_cli = open(fifo_cliente, O_WRONLY);
    if (fd_cli != -1) {
        MsgControlador msg;
        msg.tipo = RES_INFO;
        sprintf(msg.mensagem, "Viagem concluida! Valor a pagar: %d euros.", dist_km); // Exemplo
        write(fd_cli, &msg, sizeof(msg));
        close(fd_cli);
    }

    return 0;
}