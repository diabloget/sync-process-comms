#include "shared_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>

void print_error(const char* message) {
    fprintf(stderr, "[ERROR] %s\n", message);
}

void format_timestamp(struct timespec* ts, char* buffer, size_t size) {
    struct tm* tm_info = localtime(&ts->tv_sec);
    char tmp[64];
    strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", tm_info);
    snprintf(buffer, size, "%s.%03ld", tmp, ts->tv_nsec / 1000000);
}

unsigned char xor_encode(char c, unsigned char key) {
    return c ^ key;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <id_compartido> <tamaño_buffer> <clave> <archivo>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s /mi_memoria 10 123 input.txt\n", argv[0]);
        return 1;
    }

    const char* shm_name = argv[1];
    int buffer_size = atoi(argv[2]);
    unsigned char key = (unsigned char)atoi(argv[3]);
    const char* filename = argv[4];

    if (buffer_size <= 0 || buffer_size > 10000) {
        print_error("Tamaño de buffer inválido (1-10000)");
        return 1;
    }

    // Verificar que el archivo existe
    FILE* file = fopen(filename, "r");
    if (!file) {
        print_error("No se puede abrir el archivo fuente");
        return 1;
    }
    fclose(file);

    printf("=== INICIALIZADOR ===\n");
    printf("ID Compartido: %s\n", shm_name);
    printf("Tamaño Buffer: %d\n", buffer_size);
    printf("Clave: %u\n", key);
    printf("Archivo: %s\n", filename);

    // Calcular tamaño total de memoria compartida
    size_t shm_size = sizeof(SharedMemory) + buffer_size * sizeof(BufferElement);

    // Crear memoria compartida
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR | O_EXCL, 0666);
    if (shm_fd == -1) {
        if (errno == EEXIST) {
            print_error("La memoria compartida ya existe. Use el finalizador primero.");
        } else {
            perror("shm_open");
        }
        return 1;
    }

    // Establecer tamaño
    if (ftruncate(shm_fd, shm_size) == -1) {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(shm_name);
        return 1;
    }

    // Mapear memoria
    SharedMemory* shm = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        shm_unlink(shm_name);
        return 1;
    }

    // Inicializar estructura
    memset(shm, 0, shm_size);
    shm->buffer_size = buffer_size;
    shm->write_pos = 0;
    shm->read_pos = 0;
    shm->count = 0;
    shm->key = key;
    strncpy(shm->filename, filename, MAX_FILENAME - 1);
    shm->total_chars_transferred = 0;
    shm->active_senders = 0;
    shm->total_senders = 0;
    shm->active_receivers = 0;
    shm->total_receivers = 0;
    shm->shutdown_flag = 0;

    // Inicializar buffer
    for (int i = 0; i < buffer_size; i++) {
        shm->buffer[i].index = i;
        shm->buffer[i].read = 1;
        shm->buffer[i].ascii_value = 0;
    }

    // Crear nombres de semáforos únicos
    snprintf(shm->sem_empty_name, 64, "%s_empty", shm_name);
    snprintf(shm->sem_full_name, 64, "%s_full", shm_name);
    snprintf(shm->sem_mutex_name, 64, "%s_mutex", shm_name);

    // Limpiar semáforos existentes
    sem_unlink(shm->sem_empty_name);
    sem_unlink(shm->sem_full_name);
    sem_unlink(shm->sem_mutex_name);

    // Crear semáforos
    sem_t* sem_empty = sem_open(shm->sem_empty_name, O_CREAT, 0666, buffer_size);
    sem_t* sem_full = sem_open(shm->sem_full_name, O_CREAT, 0666, 0);
    sem_t* sem_mutex = sem_open(shm->sem_mutex_name, O_CREAT, 0666, 1);

    if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED || sem_mutex == SEM_FAILED) {
        perror("sem_open");
        munmap(shm, shm_size);
        close(shm_fd);
        shm_unlink(shm_name);
        return 1;
    }

    sem_close(sem_empty);
    sem_close(sem_full);
    sem_close(sem_mutex);

    printf("Memoria compartida inicializada exitosamente\n");
    printf("Tamaño total: %zu bytes\n", shm_size);

    // Limpiar
    munmap(shm, shm_size);
    close(shm_fd);

    return 0;
}