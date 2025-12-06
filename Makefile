# Compiler e Flags
CC = gcc
# Adicionámos -pthread para as threads funcionarem
CFLAGS = -Wall -Wextra -g -pthread

# Targets obrigatórios
all: controlador cliente veiculo

controlador: controlador.o
	$(CC) $(CFLAGS) -o controlador controlador.o

cliente: cliente.o
	$(CC) $(CFLAGS) -o cliente cliente.o

veiculo: veiculo.o
	$(CC) $(CFLAGS) -o veiculo veiculo.o

# Compilação genérica dos objetos
%.o: %.c comum.h
	$(CC) $(CFLAGS) -c $<

# Limpeza obrigatória
clean:
	rm -f *.o controlador cliente veiculo fifo_*