#include "comum.h"

// Variável global para controlar o loop (permite sair do while de forma limpa)
int running = 1;

int main(int argc, char *argv[]) {
    // --- 1. CONFIGURAÇÃO DA FROTA ---
    // Lê a variável de ambiente NVEICULOS [cite: 76]
    int n_veiculos = MAX_VEICULOS; 
    char *env_n_veiculos = getenv("NVEICULOS");
    if (env_n_veiculos != NULL) {
        n_veiculos = atoi(env_n_veiculos);
        // Garante que respeita os limites (Max 10) [cite: 88]
        if (n_veiculos <= 0 || n_veiculos > MAX_VEICULOS) n_veiculos = MAX_VEICULOS;
    }
    printf("CONTROLADOR: A iniciar com max %d veiculos (PID %d)\n", n_veiculos, getpid());

    // --- 2. CRIAÇÃO DO CANAL DE COMUNICAÇÃO ---
    // Cria o Named Pipe principal onde todos os clientes vêm bater à porta
    if (mkfifo(FIFO_CONTROLADOR, 0666) == -1) {
        // Se der erro "File exists", ignoramos. Outros erros são graves.
        perror("Aviso: mkfifo (se ja existe, ignora)");
    }

    // --- 3. ABERTURA NÃO-BLOQUEANTE (O SECREDO DA TAREFA 2) ---
    // Abrimos o FIFO com O_NONBLOCK.
    // O que isto faz: Se tentarmos ler (read) e não estiver lá ninguém, o programa 
    // NÃO bloqueia. O read retorna -1 e o programa continua.
    // Isto substitui a necessidade do 'select'.
    int fd_fifo = open(FIFO_CONTROLADOR, O_RDWR | O_NONBLOCK);
    if (fd_fifo == -1) {
        perror("Erro open FIFO");
        return 1;
    }

    // Também configuramos o TECLADO (Stdin) como Não-Bloqueante
    // Assim podemos ler comandos do Admin sem impedir a leitura de Clientes
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    printf("CONTROLADOR: A aguardar comandos ou clientes...\n");

    MsgCliente msg;
    char cmd_buffer[100];

    // --- 4. LOOP PRINCIPAL (POLLING) ---
    while (running) {
        
        // A. Verificar se há Clientes a falar no FIFO
        int n_lidos = read(fd_fifo, &msg, sizeof(msg));
        if (n_lidos == sizeof(msg)) {
            // Se lemos o tamanho exato de uma struct, é uma mensagem válida
            printf("[FIFO] Recebi mensagem do PID %d (Tipo: %d)\n", msg.pid_cliente, msg.tipo);
            
            if (msg.tipo == MSG_LOGIN) {
                // (Próximo passo: Validar se cabe na lista e responder)
                printf("       > Pedido de LOGIN de %s\n", msg.username);
            }
        }

        // B. Verificar se o Administrador escreveu algo no teclado
        int n_teclado = read(STDIN_FILENO, cmd_buffer, sizeof(cmd_buffer)-1);
        if (n_teclado > 0) {
            cmd_buffer[n_teclado] = '\0'; // Transforma em string válida
            
            // Remove o "Enter" (\n) do final da string
            if (cmd_buffer[strlen(cmd_buffer)-1] == '\n') 
                cmd_buffer[strlen(cmd_buffer)-1] = '\0';

            printf("[ADMIN] Comando: %s\n", cmd_buffer);

            // Processar comandos de gestão [cite: 110]
            if (strcmp(cmd_buffer, "terminar") == 0) {
                running = 0; // Vai quebrar o loop e fechar tudo
            } else {
                printf("Comando desconhecido. Tente 'terminar'.\n");
            }
        }

        // C. PAUSA INTELIGENTE
        // Como o loop roda muito depressa, usamos usleep para "dormir" 100ms.
        // Isto evita que o CPU fique a 100% (Busy Waiting).
        usleep(100000); 
    }

    // --- 5. LIMPEZA FINAL ---
    close(fd_fifo);
    unlink(FIFO_CONTROLADOR); // Apaga o ficheiro do pipe do disco
    printf("CONTROLADOR: Encerrado com sucesso.\n");
    return 0;
}