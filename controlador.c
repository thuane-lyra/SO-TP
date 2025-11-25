#include "comum.h"
#include <unistd.h> 
#include <stdlib.h> 
#include <string.h> 

// --- ESTRUTURAS DE DADOS INTERNAS ---

// Ficha de cada veículo na frota (Painel de Controlo)
typedef struct {
    pid_t pid;       // PID do processo veículo
    int pipe_fd;     // Descritor para ler a telemetria (pipe anónimo)
    int ocupado;     // 0 = Slot livre, 1 = Veículo a andar
} VeiculoFicha;

// --- VARIÁVEIS GLOBAIS ---
int running = 1;                  // Controla o loop principal
VeiculoFicha frota[MAX_VEICULOS]; // Array para gerir os veículos

// --- FUNÇÕES AUXILIARES ---

// Limpa o array da frota no arranque
void inicializar_frota() {
    for (int i = 0; i < MAX_VEICULOS; i++) {
        frota[i].pid = 0;
        frota[i].pipe_fd = -1;
        frota[i].ocupado = 0;
    }
}

// Envia resposta para o FIFO exclusivo do cliente
void enviar_resposta(pid_t pid_cliente, TipoResposta tipo, const char *msg_detalhada){
    char fifo_cliente[50];
    sprintf(fifo_cliente, "%s%d", FIFO_CLIENTE_PREFIX, pid_cliente);
    
    int fd_cli = open(fifo_cliente, O_WRONLY); // Abre para escrita
    if (fd_cli == -1) return; // Se falhar, o cliente já foi embora
    
    MsgControlador resposta;
    resposta.tipo = tipo;
    resposta.id_servico = -1; // Poderia ser usado para confirmar ID
    strncpy(resposta.mensagem, msg_detalhada, sizeof(resposta.mensagem)-1);
    resposta.mensagem[sizeof(resposta.mensagem)-1] = '\0'; // Segurança

    write(fd_cli, &resposta, sizeof(resposta));
    close(fd_cli);
}

// Lança um processo veículo (fork + exec + pipe)
void cria_veiculo(int servico_id, int dist_km, const char *cliente_fifo_path) { 
    // 1. Encontrar um slot livre na frota
    int slot = -1;
    for(int i=0; i<MAX_VEICULOS; i++){
        if(frota[i].ocupado == 0){
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        printf("ERRO: Frota cheia! Impossivel lancar veiculo.\n");
        return;
    }

    // 2. Criar o Pipe Anónimo
    int pipe_fd[2]; 
    if (pipe(pipe_fd) == -1) {
        perror("Erro pipe");
        return;
    }

    // 3. Fork (Clonar processo)
    pid_t pid = fork();

    if (pid == 0) { // --- PROCESSO FILHO (VEICULO) ---
        close(pipe_fd[0]); // Filho não lê do pai
        
        // Redirecionar STDOUT para o Pipe
        dup2(pipe_fd[1], STDOUT_FILENO); 
        close(pipe_fd[1]); 

        // Preparar argumentos
        char s_id[10], s_dist[10];
        sprintf(s_id, "%d", servico_id);
        sprintf(s_dist, "%d", dist_km); 

        // Executar o programa do veículo
        char *args[] = { "./veiculo", s_id, s_dist, (char*)cliente_fifo_path, NULL };
        execvp("./veiculo", args);
        
        // Se chegar aqui, correu mal
        perror("Erro execvp");
        exit(EXIT_FAILURE); 
    } 
    else { // --- PROCESSO PAI (CONTROLADOR) ---
        close(pipe_fd[1]); // Pai não escreve no filho

        // Configurar Pipe para NON-BLOCKING (Crucial para a Tarefa 5)
        int flags = fcntl(pipe_fd[0], F_GETFL, 0);
        fcntl(pipe_fd[0], F_SETFL, flags | O_NONBLOCK);

        // Registar na frota
        frota[slot].pid = pid;
        frota[slot].pipe_fd = pipe_fd[0];
        frota[slot].ocupado = 1;

        printf("CONTROLADOR: Veiculo lancado (PID %d) no slot %d for Servico #%d.\n", pid, slot, servico_id);
    }
}

// --- MAIN ---

int main(int argc, char *argv[]) {
    inicializar_frota();

    // 1. Configuração Inicial
    int n_max_veiculos = MAX_VEICULOS; 
    char *env = getenv("NVEICULOS");
    if (env) {
        n_max_veiculos = atoi(env);
        if (n_max_veiculos > MAX_VEICULOS) n_max_veiculos = MAX_VEICULOS;
    }

    UtilizadorAtivo users_ativos[MAX_USERS];
    int num_users_ativos = 0;

    printf("CONTROLADOR: A iniciar (PID %d) | Max Veiculos: %d\n", getpid(), n_max_veiculos);

    // 2. Criar e Abrir FIFO Principal
    if (mkfifo(FIFO_CONTROLADOR, 0666) == -1) perror("Aviso mkfifo");
    
    int fd_fifo = open(FIFO_CONTROLADOR, O_RDWR | O_NONBLOCK);
    if (fd_fifo == -1) { perror("Erro fatal FIFO"); return 1; }

    // 3. Configurar Teclado (Stdin) Non-Blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    MsgCliente msg;
    char cmd_buffer[100];
    EstadoVeiculo telemetria; 

    // --- LOOP PRINCIPAL ---
    while (running) {
        
        // A. MONITORIZAR A FROTA (TELEMETRIA)
        for (int i = 0; i < MAX_VEICULOS; i++) {
            if (frota[i].ocupado) {
                // Tenta ler do pipe deste veículo
                int n = read(frota[i].pipe_fd, &telemetria, sizeof(telemetria));
                
                if (n == sizeof(telemetria)) {
                    printf("[TELEMETRIA] Veiculo PID %d | Servico #%d | Progresso: %d%%\n", 
                           frota[i].pid, telemetria.servico_id, telemetria.percentagem_viagem);
                    
                    if (telemetria.estado == A_TERMINAR) {
                        printf("             >>> Servico CONCLUIDO. Slot %d livre.\n", i);
                        close(frota[i].pipe_fd);
                        frota[i].ocupado = 0; 
                    }
                }
                else if (n == 0) { 
                    // Veículo morreu inesperadamente
                    close(frota[i].pipe_fd);
                    frota[i].ocupado = 0;
                }
            }
        }

        // B. LER PEDIDOS DOS CLIENTES (FIFO)
        int n_lidos = read(fd_fifo, &msg, sizeof(msg));
        if (n_lidos == sizeof(msg)) {
            
            // --- LOGIN ---
            if (msg.tipo == MSG_LOGIN) {
                printf("[CLIENTE] Pedido LOGIN: %s\n", msg.username);
                
                if(num_users_ativos >= MAX_USERS) {
                    enviar_resposta(msg.pid_cliente, RES_ERRO, "Sistema cheio.");
                } else {
                    int existe = 0;
                    for(int i=0; i<num_users_ativos; i++)
                        if(strcmp(users_ativos[i].username, msg.username) == 0) existe = 1;

                    if (existe) {
                        enviar_resposta(msg.pid_cliente, RES_ERRO, "Username em uso.");
                    } else {
                        users_ativos[num_users_ativos].pid = msg.pid_cliente;
                        strncpy(users_ativos[num_users_ativos].username, msg.username, 20);
                        num_users_ativos++;
                        enviar_resposta(msg.pid_cliente, RES_OK, "Login com sucesso!");
                    }
                }
            }
            // --- AGENDAR VIAGEM ---
            else if (msg.tipo == MSG_PEDIDO_VIAGEM) {
                printf("[CLIENTE] Pedido VIAGEM: %s -> %s (%d km)\n", 
                       msg.dados.partida, msg.dados.destino, msg.dados.distancia);

                // Verificar vagas (respeitando o limite da env var)
                int vagas = 0;
                for(int i=0; i<n_max_veiculos; i++) if(!frota[i].ocupado) vagas++;

                if (vagas > 0) {
                    static int id_c = 1;
                    char fifo_cli[50];
                    sprintf(fifo_cli, "%s%d", FIFO_CLIENTE_PREFIX, msg.pid_cliente);
                    
                    cria_veiculo(id_c++, msg.dados.distancia, fifo_cli);
                    enviar_resposta(msg.pid_cliente, RES_INFO, "Veiculo enviado!");
                } else {
                    enviar_resposta(msg.pid_cliente, RES_ERRO, "Sem veiculos disponiveis.");
                }
            }
        }

        // C. LER COMANDOS DO ADMINISTRADOR (TECLADO)
        // Correção aplicada aqui: Capturamos 'n' para colocar o terminador nulo corretamente
        int n = read(STDIN_FILENO, cmd_buffer, sizeof(cmd_buffer)-1);
        if (n > 0) {
            cmd_buffer[n] = '\0'; // Garante o fim da string
            if (n > 0 && cmd_buffer[n-1] == '\n') 
                cmd_buffer[n-1] = '\0'; // Remove o ENTER

            // Comando UTILIZ
            if (strcmp(cmd_buffer, "utiliz") == 0) {
                printf("\n--- UTILIZADORES (%d) ---\n", num_users_ativos);
                for(int i=0; i<num_users_ativos; i++)
                    printf("User: %-15s | PID: %d\n", users_ativos[i].username, users_ativos[i].pid);
                printf("-------------------------\n");
            }
            // Comando FROTA
            else if (strcmp(cmd_buffer, "frota") == 0) {
                printf("\n--- FROTA (%d slots max) ---\n", n_max_veiculos);
                int cnt = 0;
                for(int i=0; i<MAX_VEICULOS; i++){
                    if(frota[i].ocupado){
                        printf("Slot %d: [OCUPADO] PID %d | PipeFD: %d\n", i, frota[i].pid, frota[i].pipe_fd);
                        cnt++;
                    }
                }
                if(cnt == 0) printf("Nenhum veiculo a circular.\n");
                printf("----------------------------\n");
            }
            // Comando TERMINAR
            else if (strcmp(cmd_buffer, "terminar") == 0) {
                running = 0;
            }
            // Comando AJUDA
            else {
                printf("Comandos disponiveis: utiliz, frota, terminar\n");
            }
        }

        usleep(100000); // Pausa de 100ms
    }

    // --- LIMPEZA ---
    close(fd_fifo);
    unlink(FIFO_CONTROLADOR);
    printf("Sistema encerrado.\n");
    return 0;
}