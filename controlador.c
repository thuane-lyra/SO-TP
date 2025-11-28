#include "comum.h"
#include <pthread.h> // [Indispensável para threads]

// --- DEFINIÇÃO DA STRUCT (Isto estava a faltar) ---
typedef struct {
    pid_t pid;       // PID do processo veículo
    int pipe_fd;     // Descritor para ler a telemetria
    int ocupado;     // 0 = Livre, 1 = Ocupado
} VeiculoFicha;

// --- DADOS PARTILHADOS ---
VeiculoFicha frota[MAX_VEICULOS];
UtilizadorAtivo users_ativos[MAX_USERS];
int num_users_ativos = 0;
int running = 1;
int n_max_veiculos = MAX_VEICULOS;

// MUTEX: O "Trinco" de segurança
pthread_mutex_t trinco = PTHREAD_MUTEX_INITIALIZER;

// --- FUNÇÕES DE LÓGICA (AUXILIARES) ---
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

// A função cria_veiculo precisa de lock quando mexe na frota!
void cria_veiculo(int servico_id, int dist, char *fifo_cli) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) return;

    // Procura slot com Mutex
    pthread_mutex_lock(&trinco);
    int slot = -1;
    for(int i=0; i<MAX_VEICULOS; i++) {
        if (!frota[i].ocupado) { slot = i; break; }
    }
    
    if (slot == -1) {
        pthread_mutex_unlock(&trinco); // Liberta antes de sair
        return;
    }
    // Reserva o slot mas ainda não preenche tudo
    frota[slot].ocupado = 1; 
    pthread_mutex_unlock(&trinco);

    pid_t pid = fork();
    if (pid == 0) { // Filho
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);
        char s_id[10], s_dist[10];
        sprintf(s_id, "%d", servico_id);
        sprintf(s_dist, "%d", dist);
        char *args[] = { "./veiculo", s_id, s_dist, fifo_cli, NULL };
        execvp("./veiculo", args);
        exit(1);
    } else { // Pai
        close(pipe_fd[1]);
        // Configura leitura Non-Block
        int f = fcntl(pipe_fd[0], F_GETFL, 0);
        fcntl(pipe_fd[0], F_SETFL, f | O_NONBLOCK);
        
        pthread_mutex_lock(&trinco);
        frota[slot].pid = pid;
        frota[slot].pipe_fd = pipe_fd[0];
        // ocupado já estava a 1
        pthread_mutex_unlock(&trinco);
        printf("[SISTEMA] Veiculo PID %d lancado.\n", pid);
    }
}

// --- THREAD 1: GESTÃO DE ENTRADAS (FIFO) ---
void *tarefa_clientes(void *arg) {
    (void)arg; // Ignora warning
    // Abre FIFO em modo bloqueante normal
    int fd_fifo = open(FIFO_CONTROLADOR, O_RDWR); 
    MsgCliente msg;

    while(running) {
        // Bloqueia aqui até chegar mensagem
        int n = read(fd_fifo, &msg, sizeof(msg));
        if (n == sizeof(msg)) {
            
            // LOGIN
            if (msg.tipo == MSG_LOGIN) {
                pthread_mutex_lock(&trinco); // Protege lista users
                if (num_users_ativos >= MAX_USERS) {
                    pthread_mutex_unlock(&trinco);
                    enviar_resposta(msg.pid_cliente, RES_ERRO, "Cheio");
                } else {
                    // Verificar duplicado
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
            // PEDIDO
            else if (msg.tipo == MSG_PEDIDO_VIAGEM) {
                // Verificar vagas
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
        }
    }
    close(fd_fifo);
    return NULL;
}

// --- THREAD 2: INTERFACE ADMIN (TECLADO) ---
void *tarefa_admin(void *arg) {
    (void)arg;
    char buffer[100];
    
    // Stdin é bloqueante por defeito, ótimo para threads
    while(running) {
        int n = read(STDIN_FILENO, buffer, sizeof(buffer)-1);
        if (n > 0) {
            buffer[n] = '\0';
            if (buffer[n-1]=='\n') buffer[n-1]='\0';

            if (strcmp(buffer, "terminar") == 0) {
                running = 0; // Vai fazer as outras threads pararem eventualmente
                // Pode ser preciso enviar um sinal para acordar a thread bloqueada no FIFO
            }
            else if (strcmp(buffer, "utiliz") == 0) {
                pthread_mutex_lock(&trinco);
                printf("\n--- USERS (%d) ---\n", num_users_ativos);
                for(int i=0; i<num_users_ativos; i++)
                    printf("- %s (PID %d)\n", users_ativos[i].username, users_ativos[i].pid);
                pthread_mutex_unlock(&trinco);
            }
            else if (strcmp(buffer, "frota") == 0) {
                pthread_mutex_lock(&trinco);
                printf("\n--- FROTA ---\n");
                for(int i=0; i<MAX_VEICULOS; i++)
                    if(frota[i].ocupado) printf("Slot %d: PID %d\n", i, frota[i].pid);
                pthread_mutex_unlock(&trinco);
            }
        }
    }
    return NULL;
}

// --- MAIN (Thread Principal) ---
// Fica responsável pela Telemetria
int main() {
    inicializar_frota();
    mkfifo(FIFO_CONTROLADOR, 0666);
    
    char *env = getenv("NVEICULOS");
    if(env) n_max_veiculos = atoi(env);
    if(n_max_veiculos > MAX_VEICULOS) n_max_veiculos = MAX_VEICULOS;

    printf("CONTROLADOR (Multithread) Iniciado. PID %d\n", getpid());

    // 1. Criar threads auxiliares
    pthread_t t_cli, t_adm;
    pthread_create(&t_cli, NULL, tarefa_clientes, NULL);
    pthread_create(&t_adm, NULL, tarefa_admin, NULL);

    // 2. Loop Principal (Telemetria)
    EstadoVeiculo st;
    while(running) {
        pthread_mutex_lock(&trinco);
        for(int i=0; i<MAX_VEICULOS; i++) {
            if(frota[i].ocupado && frota[i].pipe_fd != -1) {
                // Read é non-blocking aqui (configurado no cria_veiculo)
                int n = read(frota[i].pipe_fd, &st, sizeof(st));
                if (n == sizeof(st)) {
                    printf("[TELEMETRIA] PID %d: %d%%\n", st.veiculo_pid, st.percentagem_viagem);
                    if (st.estado == A_TERMINAR) {
                        close(frota[i].pipe_fd);
                        frota[i].ocupado = 0;
                        printf("[SISTEMA] Veiculo libertado slot %d\n", i);
                    }
                }
                else if (n == 0) { // EOF inesperado
                    close(frota[i].pipe_fd);
                    frota[i].ocupado = 0;
                }
            }
        }
        pthread_mutex_unlock(&trinco);
        usleep(100000); // Polling da frota a cada 0.1s
    }

    // Join threads antes de sair (num caso real seria necessário cancelar a leitura bloqueante)
    // pthread_join(t_cli, NULL); 
    // pthread_join(t_adm, NULL);
    unlink(FIFO_CONTROLADOR);
    return 0;
}