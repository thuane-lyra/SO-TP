#include "comum.h"
#include <unistd.h> // Contém fork, exec, pipe, dup, sleep - ESSENCIAL
#include <stdlib.h> // Contém exit, EXIT_FAILURE, atoi - ESSENCIAL
#include <string.h> // Contém strlen, strcmp

// --- FUNÇÕES AUXILIARES ---

/**
 * Constroi e envia a mensagem de resposta (OK/ERRO) ao FIFO exclusivo do Cliente. (T3)
 */
void enviar_resposta(pid_t pid_cliente, TipoResposta tipo, const char *msg_detalhada){
    char fifo_cliente[50];
    sprintf(fifo_cliente, "%s%d", FIFO_CLIENTE_PREFIX, pid_cliente);
    int fd_cli = open(fifo_cliente, O_WRONLY);

    if ( fd_cli == -1){
       perror("ERRO: Controlador nao conseguiu abrir FIFO do cliente para responder"); 
       return; 
    }
    MsgControlador resposta;
    resposta.tipo = tipo;
    resposta.id_servico = -1;
    strncpy(resposta.mensagem, msg_detalhada, sizeof(resposta.mensagem)-1);
    resposta.mensagem[sizeof(resposta.mensagem)-1] = '\0';

    write(fd_cli, &resposta, sizeof(resposta));

    close(fd_cli);
    printf("Controlador enviou resposta [%d] para PID %d.\n", tipo, pid_cliente);
}


/**
 * [TAREFA 4] Lanca um processo filho 'veiculo' usando fork/exec/pipe.
 */
void cria_veiculo(int servico_id, int dist_km, const char *cliente_fifo_path) { 
    int pipe_fd[2]; 
    if (pipe(pipe_fd) == -1) {
        perror("Erro ao criar pipe anonimo");
        return;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("Erro no fork");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return;
    } 
    
    // --- LADO DO FILHO (VEÍCULO) ---
    if (pid == 0) { 
        close(pipe_fd[0]); 
        
        // REDIRECIONAR o stdout (FD 1) para a ponta de escrita do pipe (USANDO DUP2)
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) { 
            perror("Erro no dup2 (redirecionar stdout)");
            exit(EXIT_FAILURE);
        }
        close(pipe_fd[1]); 

        // EXEC: Substituir pela imagem do veiculo
        char s_id[10], s_dist[10];
        sprintf(s_id, "%d", servico_id);
        sprintf(s_dist, "%d", dist_km); 

        char *args[] = { "./veiculo", s_id, s_dist, (char*)cliente_fifo_path, NULL };
        execvp("./veiculo", args);

        perror("Erro no execvp - nao encontrou 'veiculo'");
        exit(EXIT_FAILURE); 
    } 
    
    // --- LADO DO PAI (CONTROLADOR) ---
    else { 
        close(pipe_fd[1]);

        printf("CONTROLADOR: Veiculo %d lancado (PID: %d). A ler telemetria do FD: %d\n", servico_id, pid, pipe_fd[0]);
        
        // [TAREFA 4] FECHA-SE A PONTA DE LEITURA (Pois não estamos a ler no loop)
        close(pipe_fd[0]); 
    }
}


// --- LOOP PRINCIPAL E MAIN ---

int running = 1;

int main(int argc, char *argv[]) {
    // --- 1. CONFIGURAÇÃO DA FROTA ---
    int n_veiculos = MAX_VEICULOS; 
    UtilizadorAtivo users_ativos[MAX_USERS];
    int num_users_ativos = 0;
    
    char *env_n_veiculos = getenv("NVEICULOS");
    if (env_n_veiculos != NULL) {
        n_veiculos = atoi(env_n_veiculos);
        if (n_veiculos <= 0 || n_veiculos > MAX_VEICULOS) n_veiculos = MAX_VEICULOS;
    }
    printf("CONTROLADOR: A iniciar com max %d veiculos (PID %d)\n", n_veiculos, getpid());


    // [BLOCO DE TESTE DA TAREFA 4]
    printf("TESTE: A lancar veiculo de simulacao (Tarefa 4)...\n");
    cria_veiculo(99, 50, "/tmp/cli_1234"); 
    printf("TESTE: Veiculo lancado. A aguardar comandos ou clientes...\n"); 

    sleep(1); // Espera 1s para o veículo escrever e terminar.
    // [FIM DO BLOCO DE TESTE DA TAREFA 4]


    // --- 2. CRIAÇÃO DO CANAL DE COMUNICAÇÃO ---
    if (mkfifo(FIFO_CONTROLADOR, 0666) == -1) {
        perror("Aviso: mkfifo (se ja existe, ignora)");
    }

    // --- 3. ABERTURA NÃO-BLOQUEANTE (TAREFA 2) ---
    int fd_fifo = open(FIFO_CONTROLADOR, O_RDWR | O_NONBLOCK);
    if (fd_fifo == -1) {
        perror("Erro open FIFO");
        return 1;
    }

    // Configura o Teclado como Não-Bloqueante
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    printf("CONTROLADOR: A aguardar comandos ou clientes...\n");

    MsgCliente msg;
    char cmd_buffer[100];

    // --- 4. LOOP PRINCIPAL (POLLING) ---
    while (running) {
        
        // A. Verificar se há Clientes a falar no FIFO (TAREFA 3)
        int n_lidos = read(fd_fifo, &msg, sizeof(msg));
        if (n_lidos == sizeof(msg)) {
            printf("[FIFO] Recebi mensagem do PID %d (Tipo: %d)\n", msg.pid_cliente, msg.tipo);
            
            // Tratamento do LOGIN
            if (msg.tipo == MSG_LOGIN) {
                printf("   > Pedido de LOGIN de %s\n", msg.username);

                // --- LÓGICA DE VALIDAÇÃO ---
                if(num_users_ativos >= MAX_USERS){
                    enviar_resposta(msg.pid_cliente, RES_ERRO, "Limite maximo de utilizadores atingido.");
                    printf("   > Login RECUSADO: Limite atingido.\n");
                }else{
                    int user_existe = 0;
                    for(int i=0; i<num_users_ativos;i++){
                        if(strcmp(users_ativos[i].username,msg.username)==0){
                            user_existe = 1;
                            break;
                        }
                    }
                    if (user_existe){
                        enviar_resposta(msg.pid_cliente, RES_ERRO, "Username ja em uso. Escolha outro.");
                        printf("  > Login RECUSADO: Username ja em uso.\n");
                    }else{
                        users_ativos[num_users_ativos].pid = msg.pid_cliente;
                        strncpy(users_ativos[num_users_ativos].username, msg.username, sizeof(users_ativos[0].username)-1);
                        num_users_ativos++;
                        enviar_resposta(msg.pid_cliente, RES_OK, "Login aceite. Bem-vindo!");
                        printf("   > Login ACEITE. Total de users: %d\n", num_users_ativos);
                    }
                } 
            }
        } 


        // B. Verificar se o Administrador escreveu algo no teclado
        int n_teclado = read(STDIN_FILENO, cmd_buffer, sizeof(cmd_buffer)-1);
        if (n_teclado > 0) {
            cmd_buffer[n_teclado] = '\0'; 
            
            if (cmd_buffer[strlen(cmd_buffer)-1] == '\n') 
                cmd_buffer[strlen(cmd_buffer)-1] = '\0';

            printf("[ADMIN] Comando: %s\n", cmd_buffer);

            // Processar comandos de gestão
            if (strcmp(cmd_buffer, "terminar") == 0) {
                running = 0; 
            } else {
                printf("Comando desconhecido. Tente 'terminar'.\n");
            }
        }

        // C. PAUSA INTELIGENTE
        usleep(100000); 
    } 

    // --- 5. LIMPEZA FINAL ---
    close(fd_fifo);
    unlink(FIFO_CONTROLADOR); 
    printf("CONTROLADOR: Encerrado com sucesso.\n");
    return 0;
}