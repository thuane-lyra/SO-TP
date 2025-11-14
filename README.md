🛠️ Fase 1: Infraestrutura e Base (O "esqueleto")
Tarefa 0: Configuração do Ambiente e Makefile

Objetivo: Criar a estrutura de ficheiros e garantir que tudo compila.

A fazer:

Criar ficheiros vazios: controlador.c, cliente.c, veiculo.c, common.h (ou so_utils.h).

Criar o Makefile com as regras pedidas (all, controlador, cliente, veiculo, clean).

Nota: O common.h será usado para definir estruturas partilhadas (mensagens, constantes) entre os programas.

Tarefa 1: Definição de Dados (Protocolo)

Objetivo: Definir "como" os processos falam uns com os outros.

A fazer no common.h:

Definir constantes (ex: NVEICULOS, nomes dos FIFOs conhecidos).

Definir struct para comunicação Cliente <-> Controlador (deve incluir pid, tipo_mensagem, dados).

Definir struct para os dados do Veículo e do Serviço.

📡 Fase 2: O Controlador e o Cliente (Login)
Tarefa 2: O Controlador (Admin e Loop Principal)

Objetivo: O Controlador arranca e aceita comandos de administrador.

A fazer:

Criar o Named Pipe (FIFO) principal onde o controlador recebe pedidos de novos clientes (ex: fifo_entrada).

Implementar o loop principal que lê do stdin (comandos admin como utiliz, terminar).


Atenção: O enunciado proíbe select no controlador. Terás de usar leitura não bloqueante (O_NONBLOCK) ou outra estratégia para ler do teclado e do FIFO "ao mesmo tempo".

Tarefa 3: O Cliente e o Login

Objetivo: O Cliente liga-se e regista-se.

A fazer:

O Cliente cria o seu próprio FIFO exclusivo (ex: cli_1234.fifo) para receber respostas.

O Cliente envia pedido de login ao FIFO do Controlador.

O Controlador valida (verifica unicidade do nome e limite de 30 users ) e responde "OK" ou "Erro".

🚗 Fase 3: Gestão de Frota e Veículos
Tarefa 4: Lançar o Processo Veículo

Objetivo: O Controlador consegue criar um processo filho veiculo.

A fazer:

No Controlador, quando necessário (simular um pedido manual por agora), usar fork() e exec().

Passar informações via argumentos da linha de comandos (argv).

Criar um Unnamed Pipe antes do fork para redirecionar o stdout do veículo para o controlador.

Tarefa 5: Lógica do Veículo (Simulação)

Objetivo: O veículo "anda" e reporta estado.

A fazer:

Implementar timer/sleep para simular 1 unidade de tempo = 1 segundo.

O veículo escreve no stdout (que vai ter ao pipe do controlador) o estado a cada 10% da viagem.

O Controlador lê desse pipe e atualiza as suas estruturas de dados internas.

🔄 Fase 4: Serviços e Interação Completa
Tarefa 6: Agendamento de Serviços

Objetivo: Cliente pede viagem, Controlador agenda.

A fazer:

Comando agendar no Cliente envia pedido ao Controlador.

Controlador guarda o pedido numa fila de espera e atribui ID.

Controlador verifica se há veículos livres e lança o veículo (usando a lógica da Tarefa 4).

Tarefa 7: Comunicação Veículo <-> Cliente

Objetivo: O veículo fala diretamente com o cliente.

A fazer:

Veículo abre o FIFO do Cliente (recebido via argumentos do controlador).

Veículo avisa Cliente que "chegou".

Cliente envia comando entrar ou sair para o Veículo via FIFO.

🛡️ Fase 5: Robustez e Limpeza
Tarefa 8: Cancelamentos e Sinais

Objetivo: Lidar com imprevistos.

A fazer:

Implementar comando cancelar (Controlador envia sinal SIGUSR1 ao veículo ).

Tratar SIGINT (Ctrl+C) para fechar tudo "limpinho" (apagar FIFOs, matar processos filhos).

Tarefa 9: Relatório e Testes Finais

Objetivo: Garantir a nota máxima.

A fazer:

Verificar requisitos de memória e descritores de ficheiros.

Escrever o relatório conforme as regras.