CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
LDFLAGS = -lrt -pthread

TARGETS = inicializador emisor receptor finalizador

all: $(TARGETS)

inicializador: inicializador.c shared_memory.h
	$(CC) $(CFLAGS) -o inicializador inicializador.c $(LDFLAGS)

emisor: emisor.c shared_memory.h
	$(CC) $(CFLAGS) -o emisor emisor.c $(LDFLAGS)

receptor: receptor.c shared_memory.h
	$(CC) $(CFLAGS) -o receptor receptor.c $(LDFLAGS)

finalizador: finalizador.c shared_memory.h
	$(CC) $(CFLAGS) -o finalizador finalizador.c $(LDFLAGS)

clean:
	rm -f $(TARGETS)
	rm -f output_receptor_*.txt
	rm -f core

clean-all: clean
	# Limpiar memoria compartida si existe
	@echo "Limpiando recursos del sistema..."
	@rm -f /dev/shm/proyecto_so_shm 2>/dev/null || true
	@pkill -9 emisor 2>/dev/null || true
	@pkill -9 receptor 2>/dev/null || true
	@pkill -9 finalizador 2>/dev/null || true
	@echo "Limpieza completa."

.PHONY: all clean clean-all