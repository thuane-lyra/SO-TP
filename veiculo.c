#include "comum.h"
#include <unistd.h> 
#include <stdlib.h> 

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <id_servico> <dist_km> <fifo_cliente_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int id_servico = atoi(argv[1]);
    int dist_km = atoi(argv[2]);
    const char *fifo_cliente_path = argv[3];

    // ESTA MENSAGEM VAI PARA O CONTROLADOR VIA PIPE ANÓNIMO
    printf("VEICULO (PID %d): Servico #%d iniciado. Dist: %dkm. Cliente FIFO: %s\n", 
            getpid(), id_servico, dist_km, fifo_cliente_path);
    
    printf("VEICULO: Concluido.\n");
    return 0;
}