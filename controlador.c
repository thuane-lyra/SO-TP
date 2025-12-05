#include "comum.h"
#include <pthread.h> 
#include <time.h>    

// --- STRUCT ATUALIZADA ---
typedef struct {
    pid_t pid;       
    int pipe_fd;     
    int ocupado;     
    int distancia;   
    int id_servico;  // [NOVO] Para podermos cancelar pelo ID
} VeiculoFicha;

// --- DADOS PARTILHADOS ---
VeiculoFicha frota[MAX_VEICULOS];
UtilizadorAtivo users_ativos[MAX_USERS];
int num_users_ativos = 0;
int running = 1;
int n_max_veiculos = MAX_VEICULOS;

// Estatísticas
long total_kms_percorridos = 0; 
time_t hora_inicio_sistema;     

pthread_mutex_t trinco = PTHREAD_MUTEX_INITIALIZER;

// --- FUNÇÕES AUXILIARES ---

// [NOVO] Handler para o CTRL+C
void trata_sinal_ctrl_c(int sig) {
    (void)sig;
    printf("\n[SISTEMA] Recebi CTRL+C. A encerrar com seguranca...\n");
    running = 0; 
    // O main vai apanhar o running=0 e tratar da limpeza
}

void inicializar_frota() {
    for (int i = 0; i < MAX_VEICULOS; i++) {
        frota[i].pid = 0; frota[i].pipe_fd = -1; frota[i].ocupado = 0;
    }
}

void enviar_resposta(pid_t pid_cliente, TipoResposta tipo, const char *msg) {
    char fifo_cli[50];
    sprintf(fifo_cli, "%s%d", FIFO_CLIENTE_PREFIX, pid_cliente);
    int fd = open(fifo_cli, O_WRONLY);
    if (fd == -1) return;
    
    MsgControlador resp;
    resp.tipo = tipo;
    strncpy(resp.mensagem, msg, sizeof(resp.mensagem)-1);
    write(fd, &resp, sizeof(resp));
    close(fd);
}

// [NOVO] Função para cancelar servico
void cancelar_servico(int id_alvo) {
    pthread_mutex_lock(&trinco);
    int encontrou = 0;
    for(int i=0; i<MAX_VEICULOS; i++){
        if(frota[i].ocupado && frota[i].id_servico == id_alvo){
            // Envia SINAL ao processo veículo
            kill(frota[i].pid, SIGUSR1);
            printf("[SISTEMA] Sinal de cancelamento enviado para Veiculo PID %d (Servico %d)\n", frota[i].pid, id_alvo);
            encontrou = 1;
            break; 
        }
    }
    pthread_mutex_unlock(&trinco);
    if(!encontrou) printf("[ERRO] Servico #%d nao encontrado ou ja terminou.\n", id_alvo);
}

void cria_veiculo(int servico_id, int dist, char *fifo_cli) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) return;

    pthread_mutex_lock(&trinco);
    int slot = -1;
    for(int i=0; i<MAX_VEICULOS; i++) {
        if (!frota[i].ocupado) { slot = i; break; }
    }
    
    if (slot == -1) {
        pthread_mutex_unlock(&trinco); 
        return;
    }
    frota[slot].ocupado = 1; 
    pthread_mutex_unlock(&trinco);

    pid_t pid = fork();
    if (pid == 0) { // FILHO
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO); 
        close(pipe_fd[1]);
        
        char s_id[10], s_dist[10];
        sprintf(s_id, "%d", servico_id);
        sprintf(s_dist, "%d", dist);
        char *args[] = { "./veiculo", s_id, s_dist, fifo_cli, NULL };
        execvp("./veiculo", args);
        exit(1);
    } else { // PAI
        close(pipe_fd[1]);
        int f = fcntl(pipe_fd[0], F_GETFL, 0);
        fcntl(pipe_fd[0], F_SETFL, f | O_NONBLOCK);
        
        pthread_mutex_lock(&trinco);
        frota[slot].pid = pid;
        frota[slot].pipe_fd = pipe_fd[0];
        frota[slot].distancia = dist; 
        frota[slot].id_servico = servico_id; // [NOVO] Guardar ID
        pthread_mutex_unlock(&trinco);
        printf("[SISTEMA] Veiculo PID %d lancado (Servico %d).\n", pid, servico_id);
    }
}

// --- THREAD CLIENTES ---
void *tarefa_clientes(void *arg) {
    (void)arg; 
    int fd_fifo = open(FIFO_CONTROLADOR, O_RDWR); 
    MsgCliente msg;

    while(running) {
        int n = read(fd_fifo, &msg, sizeof(msg));
        if (n == sizeof(msg)) {
            
            if (msg.tipo == MSG_LOGIN) {
                // (Código Login Igual)
                pthread_mutex_lock(&trinco); 
                if (num_users_ativos >= MAX_USERS) {
                    pthread_mutex_unlock(&trinco);
                    enviar_resposta(msg.pid_cliente, RES_ERRO, "Cheio");
                } else {
                    int existe=0;
                    for(int i=0; i<num_users_ativos; i++)
                        if(strcmp(users_ativos[i].username, msg.username)==0) existe=1;
                    
                    if(existe) {
                        pthread_mutex_unlock(&trinco);
                        enviar_resposta(msg.pid_cliente, RES_ERRO, "User existe");
                    } else {
                        users_ativos[num_users_ativos].pid = msg.pid_cliente;
                        strcpy(users_ativos[num_users_ativos].username, msg.username);
                        num_users_ativos++;
                        pthread_mutex_unlock(&trinco);
                        enviar_resposta(msg.pid_cliente, RES_OK, "Bem-vindo");
                        printf("[THREAD CLI] Novo login: %s\n", msg.username);
                    }
                }
            }
            else if (msg.tipo == MSG_PEDIDO_VIAGEM) {
                // (Código Viagem Igual)
                pthread_mutex_lock(&trinco);
                int vagas = 0;
                for(int i=0; i<n_max_veiculos; i++) if(!frota[i].ocupado) vagas++;
                pthread_mutex_unlock(&trinco);

                if (vagas > 0) {
                    static int id_c = 1;
                    char fifo[50];
                    sprintf(fifo, "%s%d", FIFO_CLIENTE_PREFIX, msg.pid_cliente);
                    cria_veiculo(id_c++, msg.dados.distancia, fifo);
                    enviar_resposta(msg.pid_cliente, RES_INFO, "Taxi enviado");
                } else {
                    enviar_resposta(msg.pid_cliente, RES_ERRO, "Sem taxis");
                }
            }
            else if (msg.tipo == MSG_CANCELAR_VIAGEM) {
                // [NOVO] Cliente pediu para cancelar
                printf("[THREAD CLI] Pedido cancelamento servico ID %d\n", msg.dados.id_viagem);
                cancelar_servico(msg.dados.id_viagem);
            }
        }
    }
    close(fd_fifo);
    return NULL;
}

// --- THREAD ADMIN ---
void *tarefa_admin(void *arg) {
    (void)arg;
    char buffer[100];
    
    while(running) {
        int n = read(STDIN_FILENO, buffer, sizeof(buffer)-1);
        if (n > 0) {
            buffer[n] = '\0';
            if (buffer[n-1]=='\n') buffer[n-1]='\0';

            char cmd[20], arg1[20];
            sscanf(buffer, "%s %s", cmd, arg1); // Ler comando e argumento

            if (strcmp(cmd, "terminar") == 0) {
                running = 0; 
            }
            else if (strcmp(cmd, "utiliz") == 0) {
                pthread_mutex_lock(&trinco);
                printf("\n--- USERS (%d) ---\n", num_users_ativos);
                for(int i=0; i<num_users_ativos; i++)
                    printf("- %s (PID %d)\n", users_ativos[i].username, users_ativos[i].pid);
                pthread_mutex_unlock(&trinco);
            }
            else if (strcmp(cmd, "frota") == 0) {
                pthread_mutex_lock(&trinco);
                printf("\n--- FROTA ---\n");
                for(int i=0; i<MAX_VEICULOS; i++)
                    if(frota[i].ocupado) 
                        printf("Slot %d: Servico #%d (PID %d)\n", i, frota[i].id_servico, frota[i].pid);
                pthread_mutex_unlock(&trinco);
            }
            else if (strcmp(cmd, "km") == 0) {
                pthread_mutex_lock(&trinco);
                printf("Total KMs: %ld km\n", total_kms_percorridos);
                pthread_mutex_unlock(&trinco);
            }
            else if (strcmp(cmd, "hora") == 0) {
                printf("Ativo ha: %.0f s\n", difftime(time(NULL), hora_inicio_sistema));
            }
            else if (strcmp(cmd, "cancelar") == 0) {
                // [NOVO] Comando cancelar <id>
                int id_alvo = atoi(arg1);
                if (id_alvo > 0) cancelar_servico(id_alvo);
                else printf("Erro: Use 'cancelar <id_servico>'\n");
            }
        }
    }
    return NULL;
}

// --- MAIN ---
int main() {
    // [NOVO] Registar handler para CTRL+C
    signal(SIGINT, trata_sinal_ctrl_c);

    inicializar_frota();
    hora_inicio_sistema = time(NULL); 
    
    mkfifo(FIFO_CONTROLADOR, 0666);
    char *env = getenv("NVEICULOS");
    if(env) n_max_veiculos = atoi(env);
    if(n_max_veiculos > MAX_VEICULOS) n_max_veiculos = MAX_VEICULOS;

    printf("CONTROLADOR Iniciado (PID %d). CTRL+C para sair.\n", getpid());

    pthread_t t_cli, t_adm;
    pthread_create(&t_cli, NULL, tarefa_clientes, NULL);
    pthread_create(&t_adm, NULL, tarefa_admin, NULL);

    EstadoVeiculo st;
    while(running) {
        pthread_mutex_lock(&trinco);
        for(int i=0; i<MAX_VEICULOS; i++) {
            if(frota[i].ocupado && frota[i].pipe_fd != -1) {
                int n = read(frota[i].pipe_fd, &st, sizeof(st));
                
                if (n == sizeof(st)) {
                    // [NOVO] Detetar se foi cancelado (-1)
                    if (st.percentagem_viagem == -1) {
                        printf("[TELEMETRIA] Servico #%d CANCELADO a meio.\n", st.servico_id);
                    } else {
                        printf("[TELEMETRIA] Servico #%d: %d%%\n", st.servico_id, st.percentagem_viagem);
                    }
                    
                    if (st.estado == A_TERMINAR) {
                        // Só soma KMs se chegou ao fim (100%), se cancelado não soma (ou soma parcial se quiseres)
                        if (st.percentagem_viagem == 100)
                            total_kms_percorridos += frota[i].distancia;

                        close(frota[i].pipe_fd);
                        frota[i].ocupado = 0;
                        // waitpid não bloqueante para limpar zombie
                        // waitpid(frota[i].pid, NULL, WNOHANG); 
                    }
                }
                else if (n == 0) { 
                    close(frota[i].pipe_fd);
                    frota[i].ocupado = 0;
                }
            }
        }
        pthread_mutex_unlock(&trinco);
        usleep(100000); 
    }

    // --- LIMPEZA SEGURA ---
    printf("\nA encerrar sistema... Matando veiculos...\n");
    for(int i=0; i<MAX_VEICULOS; i++) {
        if(frota[i].ocupado) kill(frota[i].pid, SIGKILL);
    }
    unlink(FIFO_CONTROLADOR);
    return 0;
}