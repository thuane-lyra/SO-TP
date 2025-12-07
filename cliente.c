#include "comum.h"
#include <sys/select.h> // Necessário para o select

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <username>\n", argv[0]);
        return 1;
    }

    // --- 1. PREPARAÇÃO INICIAL ---
    printf("CLIENTE: A iniciar como %s (PID %d)\n", argv[1], getpid());

    if (access(FIFO_CONTROLADOR, F_OK) != 0) {
        printf("ERRO: O Controlador nao esta a correr.\n");
        return 1;
    }

    // Criar o FIFO exclusivo deste cliente
    char fifo_meu[50];
    sprintf(fifo_meu, "%s%d", FIFO_CLIENTE_PREFIX, getpid()); 
    if (mkfifo(fifo_meu, 0666) == -1) {
        perror("Erro mkfifo");
        return 1;
    }

    // --- 2. ENVIAR LOGIN ---
    MsgCliente msg;
    msg.pid_cliente = getpid();
    msg.tipo = MSG_LOGIN;
    strncpy(msg.username, argv[1], sizeof(msg.username)-1);
    
    int fd_server = open(FIFO_CONTROLADOR, O_WRONLY);
    if(fd_server == -1) { unlink(fifo_meu); return 1; }
    write(fd_server, &msg, sizeof(msg));
    close(fd_server);

    // Abrir o nosso FIFO para receber respostas (RDWR para não bloquear logo no select)
    int fd_meu = open(fifo_meu, O_RDWR); 

    printf("CLIENTE: Login enviado. A aguardar resposta...\n");

    // --- 3. LOOP PRINCIPAL (MULTIPLEXAGEM COM SELECT) ---
    fd_set read_fds;
    char linha[100];
    int logado = 0; // Flag: 0 = A espera de login, 1 = Logado

    while(1) {
        // Mostrar a prompt apenas se já estivermos logados
        if (logado) {
            printf("Comando (agendar/consultar/cancelar/sair) > "); 
            fflush(stdout); 
        }

        // A. Preparar o select
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); // Monitorizar Teclado (FD 0)
        FD_SET(fd_meu, &read_fds);       // Monitorizar FIFO de Resposta

        int max_fd = (STDIN_FILENO > fd_meu) ? STDIN_FILENO : fd_meu;

        // B. Esperar por ação (Bloqueia aqui)
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("Erro no select");
            break;
        }

        // --- CASO 1: RECEBEU MENSAGEM DO CONTROLADOR/TAXI ---
        if (FD_ISSET(fd_meu, &read_fds)) {
            MsgControlador resposta;
            int n = read(fd_meu, &resposta, sizeof(resposta));
            
            // Truque visual: Limpa a linha atual para a notificação não ficar misturada com a prompt
            printf("\r" "\x1b[K"); 

            if (n == sizeof(resposta)) {
                if (resposta.tipo == RES_OK) {
                    printf(">> SISTEMA: Login Aceite! %s\n", resposta.mensagem);
                    logado = 1;
                }
                else if (resposta.tipo == RES_ERRO) {
                    printf(">> SISTEMA: Erro: %s\n", resposta.mensagem);
                    if (!logado) break; // Se falhou o login inicial, sai.
                }
                else if (resposta.tipo == RES_INFO) {
                    printf(">> NOTIFICACAO: %s\n", resposta.mensagem);
                }
            }
        }

        // --- CASO 2: O UTILIZADOR ESCREVEU NO TECLADO ---
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(linha, sizeof(linha), stdin) == NULL) break;
            
            // Só aceita comandos se o login já foi aceite
            if (!logado) {
                printf("Ainda nao recebeste confirmacao de login...\n");
                continue;
            }

            // Parsing do comando
            char cmd[20], arg1[20], arg2[20], arg3[20];
            // Limpar buffers
            memset(cmd, 0, 20); memset(arg1, 0, 20); 
            memset(arg2, 0, 20); memset(arg3, 0, 20);

            int n_args = sscanf(linha, "%s %s %s %s", cmd, arg1, arg2, arg3);

            if (n_args > 0) {
                // > SAIR
                if (strcmp(cmd, "sair") == 0) {
                    printf("A sair...\n");
                    break;
                }
                
                // > AGENDAR
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
                
                // > CANCELAR (TAREFA 7)
                else if (strcmp(cmd, "cancelar") == 0) {
                     if (n_args < 2) {
                        printf("Uso: cancelar <id_servico>\n");
                     } else {
                        MsgCliente p;
                        p.pid_cliente = getpid();
                        p.tipo = MSG_CANCELAR_VIAGEM;
                        p.dados.id_viagem = atoi(arg1); // ID vem no primeiro argumento

                        int fd = open(FIFO_CONTROLADOR, O_WRONLY);
                        if(fd != -1) {
                            write(fd, &p, sizeof(p));
                            close(fd);
                            printf(">> Pedido de cancelamento para servico #%d enviado.\n", p.dados.id_viagem);
                        }
                     }
                }
                else if (strcmp(cmd, "consultar") == 0) {
                     MsgCliente p;
                     p.pid_cliente = getpid();
                     p.tipo = MSG_CONSULTAR_VIAGENS;
                     // Não precisa de dados extra, o PID chega
                     
                     int fd = open(FIFO_CONTROLADOR, O_WRONLY);
                     if(fd != -1) {
                         write(fd, &p, sizeof(p));
                         close(fd);
                         printf(">> Pedido de consulta enviado.\n");
                     } else {
                        printf(">> Erro ao contactar servidor.\n");
                     }
                 }


                else {
                    printf("Comando invalido.\n");
                }
            }
        }
    }

    // --- 4. LIMPEZA FINAL ---
    close(fd_meu);
    unlink(fifo_meu);
    return 0;
}