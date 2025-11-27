#include "comum.h"

int main(int argc, char *argv[]) {
    // O utilizador tem de indicar o username ao arrancar [cite: 123]
    if (argc != 2) {
        printf("Uso: %s <username>\n", argv[0]);
        return 1;
    }

    printf("CLIENTE: A iniciar como %s (PID %d)\n", argv[1], getpid());

    // 1. VERIFICAR CONTROLADOR
    // Antes de tudo, vemos se o FIFO do controlador existe.
    if (access(FIFO_CONTROLADOR, F_OK) != 0) {
        printf("ERRO: O Controlador não está ligado! (FIFO não encontrado)\n");
        return 1;
    }

    // 2. CRIAR O MEU "CORREIO" (FIFO DE RESPOSTA)
    // Cada cliente cria um FIFO único baseado no seu PID (ex: /tmp/cli_1234)
    // É aqui que o controlador vai depositar a resposta "Login Aceite" ou "Recusado".
    char fifo_meu[50];
    sprintf(fifo_meu, "%s%d", FIFO_CLIENTE_PREFIX, getpid()); 
    
    if (mkfifo(fifo_meu, 0666) == -1) {
        perror("Erro ao criar FIFO exclusivo do cliente");
        return 1;
    }

    // 3. ENVIAR PEDIDO DE LOGIN
    MsgCliente msg;
    msg.pid_cliente = getpid(); // Mando o meu PID para ele saber onde responder
    msg.tipo = MSG_LOGIN;
    strncpy(msg.username, argv[1], sizeof(msg.username)-1);
    
    // Abre o FIFO do controlador apenas para escrita
    int fd_server = open(FIFO_CONTROLADOR, O_WRONLY);
    if (fd_server == -1) {
        perror("Erro ao contactar controlador");
        unlink(fifo_meu);
        return 1;
    }
    write(fd_server, &msg, sizeof(msg));
    close(fd_server); // Fecha logo para libertar o recurso

    printf("CLIENTE: Pedido enviado. A aguardar resposta em %s...\n", fifo_meu);

    // 4. AGUARDAR RESPOSTA (BLOQUEANTE)
    // Aqui abrimos o nosso FIFO em modo normal (sem O_NONBLOCK).
    // O programa vai PARAR nesta linha (read) até que o Controlador nos responda.
    int fd_meu = open(fifo_meu, O_RDWR);

    MsgControlador resposta;
    int n = read(fd_meu, &resposta, sizeof(resposta));

    int status_code = 0; // 0 para sucesso (termina OK), 1 para erro (termina com erro)

    if( n == sizeof(resposta)){
        // Verificar o tipo de resposta enviada pelo Controlador
        if (resposta.tipo == RES_OK){
            printf("\n LOGIN SUCESSO! %s\n", resposta.mensagem);
            
            // --- NOVO LOOP PRINCIPAL (TAREFA 7 - COM SELECT) ---
            fd_set read_fds;     // Estrutura que guarda a lista de "ficheiros" a vigiar
            int max_fd = fd_meu; // O select precisa saber qual é o ID mais alto para procurar
            
            while (1) {
                // Mostra a prompt para o utilizador escrever
                printf("Comando (agendar/cancelar/sair) > ");
                fflush(stdout); // [IMPORTANTE] Força o print aparecer antes de bloquearmos no select

                // 1. Limpar e configurar os "ouvidos" do select
                FD_ZERO(&read_fds);              // Limpa a lista
                FD_SET(STDIN_FILENO, &read_fds); // Adiciona o Teclado (0) à lista
                FD_SET(fd_meu, &read_fds);       // Adiciona o FIFO de mensagens à lista


                // 2. O Select bloqueia aqui até acontecer alguma coisa numa das portas
                // Não usamos timeout (NULL), fica a espera indefinidamente
                int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

                if (activity < 0 && errno != EINTR) {
                    perror("Erro no select"); // Erro grave de sistema
                    break;
                }

                // --- SITUAÇÃO A: RECEBEU MENSAGEM NO FIFO ---
                // (Vem do Controlador OU do Veículo - agora tratamos igual)
                if (FD_ISSET(fd_meu, &read_fds)) {
                    MsgControlador msg_in;
                    int n = read(fd_meu, &msg_in, sizeof(msg_in));

                    if (n > 0) {
                        // Truque visual: \r volta ao inicio da linha e espaços limpam o "Comando >"
                        // Isto evita que a mensagem fique misturada com o que estavas a escrever
                        printf("\r                                                  \r");
                        
                        if (msg_in.tipo == RES_INFO) {
                            // Aqui recebes "Veiculo enviado" e "Veiculo chegou"
                            printf(" NOTIFICAÇÃO: %s\n", msg_in.mensagem);
                        } 
                        else if (msg_in.tipo == RES_ERRO) {
                            printf(" ERRO: %s\n", msg_in.mensagem);
                        }
                        
                        // Reapresenta a prompt para o utilizador saber que pode escrever
                        printf("Comando (agendar/cancelar/ sair) > ");
                        fflush(stdout);
                    }
                }

                // --- SITUAÇÃO B: UTILIZADOR ESCREVEU NO TECLADO ---
                if ( FD_ISSET(STDIN_FILENO, &read_fds)){
                    char linha [100];
                    //Agora é seguro usar fgets porque sabemos que há dados para ler!

                    if (fgets(linha, sizeof(linha), stdin) == NULL) break; 
                    
                    // Limpeza rápida do \n no fim da string
                    linha[strcspn(linha, "\n")] = 0;

                    char cmd[20], arg1[20], arg2[20], arg3[20];
                    // Inicializar buffers a vazio
                    arg1[0] = arg2[0] = arg3[0] = '\0';

                    int n_args = sscanf(linha, "%s %s %s %s", cmd, arg1,arg2,arg3);
                    if(n_args < 1 ) continue; // Utilizador só deu ENTER


                    // --- COMANDO SAIR ---
                    if(strcmp(cmd, "sair") == 0){
                        printf("A sair do sistema...\n");
                        // Nota: Idealmente devias avisar o controlador que vais sair
                        break;
                    }
                    // --- COMANDO AGENDAR ---
                    else if (strcmp(cmd, "agendar")==0){
                         // Valida se escreveu: agendar origem destino distancia
                        if (n_args < 4) {
                            printf("Erro: Use 'agendar <origem> <destino> <distancia>'\n");
                            continue; // Volta ao início do while
                        } else {
                            // Prepara a mensagem para o Controlador
                            MsgCliente p;
                            p.pid_cliente = getpid();
                            p.tipo = MSG_PEDIDO_VIAGEM;
                            strncpy(p.dados.partida, arg1, sizeof(p.dados.partida)-1);
                            strncpy(p.dados.destino, arg2, sizeof(p.dados.destino)-1);
                            p.dados.distancia = atoi(arg3); // Converte string "100" para int 100

                            // Envia para o FIFO do Controlador
                            int fd = open(FIFO_CONTROLADOR, O_WRONLY);
                            if (fd != -1) {
                                write(fd, &p, sizeof(p));
                                close(fd);
                                printf("Pedido enviado! (Origem: %s, Destino: %s, Dist: %d)\n", 
                                       arg1, arg2, p.dados.distancia);
                            } else {
                                perror("Erro ao enviar pedido");
                            }
                        }
                    }
                    else {
                        printf("Comando desconhecido. Tente 'agendar' ou 'sair'.\n");
                    }
                }
            } 
        } 
    } 
    // 5. LIMPEZA
    close(fd_meu);
    unlink(fifo_meu); // Muito importante: Apagar o meu FIFO antes de sair!
    return status_code; // Retorna 0 para sucesso, 1 para erro
}
