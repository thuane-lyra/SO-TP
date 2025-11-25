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
int fd_meu = open(fifo_meu, O_RDONLY);

MsgControlador resposta;
int n = read(fd_meu, &resposta, sizeof(resposta));

int status_code = 0; // 0 para sucesso (termina OK), 1 para erro (termina com erro)

if( n == sizeof(resposta)){
   // Verificar o tipo de resposta enviada pelo Controlador
   if (resposta.tipo == RES_OK){
    printf("\n LOGIN SUCESSO! %s\n", resposta.mensagem);
    char linha[100];
    char cmd[20], arg1[20], arg2[20], arg3[20];
    while (1) {
            // Mostra a prompt para o utilizador escrever
            printf("Comando (agendar/cancelar/sair) > ");
            
            // Lê a linha. Se der erro ou CTRL+D, sai do loop
            if (fgets(linha, sizeof(linha), stdin) == NULL) break;

            // Limpa variáveis para evitar lixo de memória
            arg1[0] = '\0'; arg2[0] = '\0'; arg3[0] = '\0';

            // Parte a string em palavras: comando + 3 argumentos
            int n_args = sscanf(linha, "%s %s %s %s", cmd, arg1, arg2, arg3);
            
            if (n_args < 1) continue; // Se o utilizador só deu Enter, volta ao início

            // --- COMANDO SAIR ---
            if (strcmp(cmd, "sair") == 0) {
                printf("A sair do sistema...\n");
                break; // Quebra o while e vai para a limpeza final
            }
            // --- COMANDO AGENDAR ---
            else if (strcmp(cmd, "agendar") == 0) {
                // Valida se escreveu: agendar origem destino distancia
                if (n_args < 4) {
                    printf("Erro: Use 'agendar <origem> <destino> <distancia>'\n");
                    continue;
                }
                
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
            else {
                printf("Comando desconhecido. Tente 'agendar' ou 'sair'.\n");
            }
        }
    // O Cliente fica num loop de comandos aqui, mas por agora, termina OK. 
   } else if(resposta.tipo == RES_ERRO){
    printf("\n LOGIN FALHADO! %s\n", resposta.mensagem);
        status_code = 1; // Falhou, vamos terminar com código de erro
   } else {
    printf("\n Resposta desconhecida do controlador (Tipo: %d). %s\n", resposta.tipo, resposta.mensagem);
        status_code = 1;
   }
} else {
    // Se o read falhar ou ler um tamanho inesperado (n != sizeof(resposta))
    printf("\nERRO de leitura no FIFO de resposta. Tamanho lido: %d\n", n);
    status_code = 1;
}
// 5. LIMPEZA
close(fd_meu);
unlink(fifo_meu); // Muito importante: Apagar o meu FIFO antes de sair!
return status_code; // Retorna 0 para sucesso, 1 para erro
}