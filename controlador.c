#include "comum.h"
// EM controlador.c (antes do main)
/**
 * Constroi e envia a mensagem de resposta (OK/ERRO) ao FIFO exclusivo do Cliente.
 * @param pid_cliente PID do processo cliente (usado para construir o nome do FIFO).
 * @param tipo Tipo de resposta (RES_OK ou RES_ERRO).
 * @param msg_detalhada Mensagem de texto para o utilizador.
 */
void enviar_resposta(pid_t pid_cliente, TipoResposta tipo, const char *msg_detalhada){
    char fifo_cliente[50];
// 1. Constrói o nome do FIFO exclusivo do cliente
    sprintf(fifo_cliente, "%s%d", FIFO_CLIENTE_PREFIX, pid_cliente);
    // 2. Tenta abrir o FIFO para escrita (Controlador escreve)
    int fd_cli = open(fifo_cliente, O_WRONLY);

    if ( fd_cli == -1){
       // Se este erro ocorrer, é um problema de sincronização ou o cliente morreu.
       perror("ERRO: Controlador nao conseguiu abrir FIFO do cliente para responder"); 
       return; 
    }
    // 3. Prepara a struct de resposta
    MsgControlador resposta;
    resposta.tipo = tipo;
    resposta.id_servico = -1; // -1 para login
    strncpy(resposta.mensagem, msg_detalhada, sizeof(resposta.mensagem)-1);
    resposta.mensagem[sizeof(resposta.mensagem)-1] = '\0';

    // 4. Envia a resposta (desbloqueia o Cliente)
    write(fd_cli, &resposta, sizeof(resposta));

    // 5. Limpeza
    close(fd_cli);
    printf("Controlador enviou resposta [%d] para PID %d.\n", tipo, pid_cliente);

}


// Variável global para controlar o loop (permite sair do while de forma limpa)
int running = 1;

int main(int argc, char *argv[]) {
    // --- 1. CONFIGURAÇÃO DA FROTA ---
    // Lê a variável de ambiente NVEICULOS
    int n_veiculos = MAX_VEICULOS; 
    UtilizadorAtivo users_ativos[MAX_USERS];
    int num_users_ativos = 0;
    char *env_n_veiculos = getenv("NVEICULOS");
    if (env_n_veiculos != NULL) {
        n_veiculos = atoi(env_n_veiculos);
        // Garante que respeita os limites (Max 10)
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
    // Isto substitui a necessidade do 'select'.
    int fd_fifo = open(FIFO_CONTROLADOR, O_RDWR | O_NONBLOCK);
    if (fd_fifo == -1) {
        perror("Erro open FIFO");
        return 1;
    }

    // Também configuramos o TECLADO (Stdin) como Não-Bloqueante
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
            // Se lemos o tamanho exato de uma struct, é uma mensagem válida
            printf("[FIFO] Recebi mensagem do PID %d (Tipo: %d)\n", msg.pid_cliente, msg.tipo);
            
            // Tratamento do LOGIN (Tarefa 3)
            if (msg.tipo == MSG_LOGIN) {
                printf("   > Pedido de LOGIN de %s\n", msg.username);

                // --- LÓGICA DE VALIDAÇÃO ---

                // 1. VERIFICAR LIMITE DE UTILIZADORES
                if(num_users_ativos >= MAX_USERS){
                    enviar_resposta(msg.pid_cliente, RES_ERRO, "Limite maximo de utilizadores atingido.");
                    printf("   > Login RECUSADO: Limite atingido.\n");
                }else{
                    // 2. VERIFICAR SE USERNAME REPETE
                    int user_existe = 0;
                    for(int i=0; i<num_users_ativos;i++){
                        if(strcmp(users_ativos[i].username,msg.username)==0){
                            user_existe = 1;
                            break;
                        }
                    }
                    if (user_existe){
                        enviar_resposta(msg.pid_cliente, RES_ERRO, "Username ja em uso. Escolha outro.");
                        printf("  > Login RECUSADO: Username ja em uso.\n");
                    }else{
                        // 3. LOGIN ACEITE: ADICIONAR À LISTA
                        users_ativos[num_users_ativos].pid = msg.pid_cliente;
                        strncpy(users_ativos[num_users_ativos].username, msg.username, sizeof(users_ativos[0].username)-1);
                        num_users_ativos++;

                        //envia resposta OK
                        enviar_resposta(msg.pid_cliente, RES_OK, "Login aceite. Bem-vindo!");
                        printf("  > Login ACEITE. Total de users: %d\n", num_users_ativos);
                    }
                } // Fim do else de if(num_users_ativos >= MAX_USERS)

            } // FIM DO if (msg.tipo == MSG_LOGIN)
            // Futuramente, aqui trataremos outras mensagens (PEDIDO_VIAGEM, LOGOUT)
            
        } // FIM DO if (n_lidos == sizeof(msg))


        // B. Verificar se o Administrador escreveu algo no teclado
        int n_teclado = read(STDIN_FILENO, cmd_buffer, sizeof(cmd_buffer)-1);
        if (n_teclado > 0) {
            cmd_buffer[n_teclado] = '\0'; // Transforma em string válida
            
            // Remove o "Enter" (\n) do final da string
            if (cmd_buffer[strlen(cmd_buffer)-1] == '\n') 
                cmd_buffer[strlen(cmd_buffer)-1] = '\0';

            printf("[ADMIN] Comando: %s\n", cmd_buffer);

            // Processar comandos de gestão
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
    } // <--- ESTA CHAVETA FECHA o while (running)

    // --- 5. LIMPEZA FINAL ---
    // Estavam fora do main(), causando erro. Estão agora no sítio correto.
    close(fd_fifo);
    unlink(FIFO_CONTROLADOR); // Apaga o ficheiro do pipe do disco
    printf("CONTROLADOR: Encerrado com sucesso.\n");
    return 0;
} // <--- ESTA CHAVETA FECHA a função main()