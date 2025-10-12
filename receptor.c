#include "shared_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <id_compartido> <clave>\n", argv[0]);
        return 1;
    }

    const char* shm_name = argv[1];
    unsigned char key = (unsigned char)atoi(argv[2]);

    // 1. Abrir memoria compartida
    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }

    // 2. Mapear memoria
    SharedMemory* shm = mmap(NULL, sizeof(SharedMemory) + 10 * sizeof(BufferElement),
                           PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // 3. Abrir semáforos
    sem_t* sem_empty = sem_open(shm->sem_empty_name, 0);
    sem_t* sem_full = sem_open(shm->sem_full_name, 0);
    sem_t* sem_mutex = sem_open(shm->sem_mutex_name, 0);

    if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED || sem_mutex == SEM_FAILED) {
        perror("sem_open");
        return 1;
    }

    printf("=== RECEPTOR SIMPLE ===\n");
    printf("Esperando caracteres...\n");

    while (1) { // Bucle infinito simple
        // Esperar por un elemento
        sem_wait(sem_full);

        // Entrar a sección crítica
        sem_wait(sem_mutex);

        // Leer del buffer
        int pos = shm->read_pos;
        char decoded_char = '?';

        // Asegurarse que no ha sido leído por otro receptor simple
        if (shm->buffer[pos].read == 0) {
            unsigned char encoded_val = shm->buffer[pos].ascii_value;
            decoded_char = xor_encode(encoded_val, key);
            shm->buffer[pos].read = 1; // Marcar como leído
            shm->read_pos = (shm->read_pos + 1) % shm->buffer_size;
            printf("Recibido: '%c'\n", decoded_char);
            
            // Salir de sección crítica
            sem_post(sem_mutex);
            // Notificar que hay un espacio libre
            sem_post(sem_empty);
        } else {
             // Ya fue leído, liberar mutex y volver a esperar
            sem_post(sem_mutex);
            sem_post(sem_full); // Devolver el "ticket" para que otro intente
        }
    }

    // Este código nunca se alcanza en este diseño simple,
    // pero se incluye por completitud.
    printf("Receptor simple ha finalizado.\n");
    sem_close(sem_empty);
    sem_close(sem_full);
    sem_close(sem_mutex);
    munmap(shm, sizeof(SharedMemory));
    close(shm_fd);

    return 0;
}