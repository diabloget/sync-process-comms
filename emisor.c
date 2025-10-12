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

    // Aquí se abre la memoria compartida 
    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }

    // Se mapear memoria (Asumiendo el tamaño máximo conocido)
    SharedMemory* shm = mmap(NULL, sizeof(SharedMemory) + 10 * sizeof(BufferElement), // Tamaño hardcodeado para simplicidad
                           PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // Se abren los semáforos
    sem_t* sem_empty = sem_open(shm->sem_empty_name, 0);
    sem_t* sem_full = sem_open(shm->sem_full_name, 0);
    sem_t* sem_mutex = sem_open(shm->sem_mutex_name, 0);

    if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED || sem_mutex == SEM_FAILED) {
        perror("sem_open");
        return 1;
    }

    // Abrir archivo fuente
    FILE* file = fopen(shm->filename, "r");
    if (!file) {
        fprintf(stderr, "Error: no se pudo abrir el archivo %s\n", shm->filename);
        return 1;
    }

    printf("=== EMISOR SIMPLE ===\n");
    printf("Enviando caracteres del archivo: %s\n", shm->filename);

    char ch;
    while ((ch = fgetc(file)) != EOF) {
        // Esperar por un espacio vacío
        sem_wait(sem_empty);
        
        // Entrar a la sección crítica
        sem_wait(sem_mutex);

        // Escribir en el buffer
        int pos = shm->write_pos;
        shm->buffer[pos].ascii_value = xor_encode(ch, key);
        shm->buffer[pos].read = 0; // Marcar como no leído
        shm->write_pos = (shm->write_pos + 1) % shm->buffer_size;
        
        printf("Enviado: '%c'\n", ch);

        // Salir de la sección crítica
        sem_post(sem_mutex);

        // Notificar que hay un elemento nuevo
        sem_post(sem_full);
        
        usleep(100000); // Es necesario hacer esta pausa para evitar errores de sincronización.
    }

    fclose(file);
    printf("Emisor simple ha finalizado.\n");

    // Aquí se cierran los semáforos y la memoria compartida
    sem_close(sem_empty);
    sem_close(sem_full);
    sem_close(sem_mutex);
    munmap(shm, sizeof(SharedMemory));
    close(shm_fd);

    return 0;
}