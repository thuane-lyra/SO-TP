# Compiler e Flags
CC = gcc
CFLAGS = -Wall -Wextra -g

# Targets obrigatórios
all: controlador cliente veiculo

controlador: controlador.o
	$(CC) $(CFLAGS) -o controlador controlador.o

cliente: cliente.o
	$(CC) $(CFLAGS) -o cliente cliente.o

veiculo: veiculo.o
	$(CC) $(CFLAGS) -o veiculo veiculo.o

# Compilação genérica dos objetos
%.o: %.c common.h
	$(CC) $(CFLAGS) -c $<

# Limpeza obrigatória
clean:
	rm -f *.o controlador cliente veiculo fifo_*