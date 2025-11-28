#include "comum.h"
#include <sys/select.h> // Necessário para o select

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <username>\n", argv[0]);
        return 1;
    }

    // --- SETUP INICIAL IGUAL ---
    if (access(FIFO_CONTROLADOR, F_OK) != 0) {
        printf("ERRO: Controlador nao acessivel.\n");
        return 1;
    }

    char fifo_meu[50];
    sprintf(fifo_meu, "%s%d", FIFO_CLIENTE_PREFIX, getpid()); 
    if (mkfifo(fifo_meu, 0666) == -1) {
        perror("Erro mkfifo");
        return 1;
    }

    // Envia Login
    MsgCliente msg;
    msg.pid_cliente = getpid();
    msg.tipo = MSG_LOGIN;
    strncpy(msg.username, argv[1], sizeof(msg.username)-1);
    
    int fd_server = open(FIFO_CONTROLADOR, O_WRONLY);
    if(fd_server == -1) { unlink(fifo_meu); return 1; }
    write(fd_server, &msg, sizeof(msg));
    close(fd_server);

    // Abre o seu FIFO para leitura (Bloqueante por agora)
    int fd_meu = open(fifo_meu, O_RDWR); // RDWR evita que o select dispare EOF constantemente

    printf("CLIENTE: Login enviado. A aguardar...\n");

    // --- LOOP COM SELECT ---
    // Em vez de esperar resposta bloqueante, entramos logo no loop principal
    // O select vai gerir se é resposta de login ou input do user
    
    fd_set read_fds;
    char linha[100];
    int logado = 0; // Flag para saber se já fomos aceites

    while(1) {
        printf("Comando > "); 
        fflush(stdout); // Forçar o print da prompt

        // 1. Preparar o conjunto de descritores
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); // Monitorizar Teclado (0)
        FD_SET(fd_meu, &read_fds);       // Monitorizar FIFO do Servidor

        int max_fd = (STDIN_FILENO > fd_meu) ? STDIN_FILENO : fd_meu;

        // 2. Chamar o select (Bloqueia aqui até haver ação)
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if ((activity < 0)) {
            perror("Erro no select");
            break;
        }

        // --- CASO A: MENSAGEM DO SERVIDOR ---
        if (FD_ISSET(fd_meu, &read_fds)) {
            MsgControlador resposta;
            int n = read(fd_meu, &resposta, sizeof(resposta));
            
            // Limpar a linha atual para não estragar a interface visual
            printf("\r                          \r"); 
            
            if (n == sizeof(resposta)) {
                if (resposta.tipo == RES_OK) {
                    printf(">> SISTEMA: Login Aceite! %s\n", resposta.mensagem);
                    logado = 1;
                }
                else if (resposta.tipo == RES_ERRO) {
                    printf(">> SISTEMA: Erro: %s\n", resposta.mensagem);
                    if (!logado) break; // Se falhou login, sai
                }
                else if (resposta.tipo == RES_INFO) {
                    printf(">> NOTIFICACAO: %s\n", resposta.mensagem);
                }
            }
        }

        // --- CASO B: INPUT DO UTILIZADOR ---
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(linha, sizeof(linha), stdin) == NULL) break;
            
            // Só processa comandos se estiver logado
            if (!logado) {
                printf("Ainda nao estas logado...\n");
                continue;
            }

            char cmd[20], arg1[20], arg2[20], arg3[20];
            arg1[0]='\0'; arg2[0]='\0'; arg3[0]='\0';
            int n_args = sscanf(linha, "%s %s %s %s", cmd, arg1, arg2, arg3);

            if (n_args > 0) {
                if (strcmp(cmd, "sair") == 0) break;
                
                else if (strcmp(cmd, "agendar") == 0) {
                     if (n_args < 4) {
                        printf("Uso: agendar <origem> <destino> <distancia>\n");
                     } else {
                        MsgCliente p;
                        p.pid_cliente = getpid();
                        p.tipo = MSG_PEDIDO_VIAGEM;
                        strncpy(p.dados.partida, arg1, 49);
                        strncpy(p.dados.destino, arg2, 49);
                        p.dados.distancia = atoi(arg3);

                        int fd = open(FIFO_CONTROLADOR, O_WRONLY);
                        if(fd != -1) {
                            write(fd, &p, sizeof(p));
                            close(fd);
                            printf(">> Pedido enviado.\n");
                        }
                     }
                }
                else {
                    printf("Comando desconhecido.\n");
                }
            }
        }
    }

    close(fd_meu);
    unlink(fifo_meu);
    return 0;
}