#include "comum.h"
//função de limpeza (a ser usada em caso de erro ou terminação)
void limpar_e_sair(int status);

int main(int argc, char *argv[]) {
    int fd_fifo_controlador;
    int n_veiculos = MAX_VEICULOS; // Valor default caso NVEICULOS não esteja definido
    
    // 1. Ler a variável de ambiente NVEICULOS
    char *env_n_veiculos = getenv("NVEICULOS");
    if (env_n_veiculos != NULL) {
        n_veiculos = atoi(env_n_veiculos);
        if (n_veiculos <= 0 || n_veiculos > MAX_VEICULOS) {
            fprintf(stderr, "Aviso: NVEICULOS inválido ou fora dos limites (%d). Usando o máximo (%d).\n", n_veiculos, MAX_VEICULOS);
            n_veiculos = MAX_VEICULOS;
        }
    }
    printf("CONTROLADOR: A arrancar com frota máxima de %d veículos.\n", n_veiculos);

    // 2. Criar o Named Pipe principal (FIFO_CONTROLADOR)
    // Se o FIFO já existir, mkfifo falhará, o que não é um problema grave (mas pode ser um erro se outro controlador estiver a correr)
    if (mkfifo(FIFO_CONTROLADOR, 0666) == -1) {
        perror("Erro ao criar FIFO_CONTROLADOR (pode já existir)");
        // Se houver erro, podemos tentar apagar e recriar, mas por agora saímos.
        // É responsabilidade do programa validar se já existe uma instância do Controlador[cite: 107].
    }

    // 3. Abrir o FIFO em modo de leitura (O_RDONLY)
    // Nota: O open() bloqueará até que um cliente o abra em O_WRONLY
    fd_fifo_controlador = open(FIFO_CONTROLADOR, O_RDONLY);
    if (fd_fifo_controlador == -1) {
        perror("Erro ao abrir FIFO_CONTROLADOR para leitura");
        limpar_e_sair(1);
    }
    
    printf("CONTROLADOR: Pronto para receber pedidos de login no FIFO: %s\n", FIFO_CONTROLADOR);
    
    // O próximo passo seria o Loop Principal (Tarefas 2.2 e 2.3)
    
    // Por enquanto, apenas esperamos um pouco e limpamos
    sleep(5); 

    limpar_e_sair(0);
    return 0;
}

void limpar_e_sair(int status) {
    // 4. Limpeza: Fechar descritores e apagar o FIFO
    printf("CONTROLADOR: A terminar e a limpar recursos.\n");
    // close(fd_fifo_controlador); // Nota: fechar aqui pode ser problemático, dependendo de como o fd é global ou passado
    unlink(FIFO_CONTROLADOR); // Apagar o ficheiro FIFO
    exit(status);
}