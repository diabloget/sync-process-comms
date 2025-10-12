#include "shared_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <id_compartido>\n", argv[0]);
        return 1;
    }

    const char* shm_name = argv[1];

    printf("=== FINALIZADOR ===\n");
    printf("Finalizando procesos...\n");

    // Abrir memoria compartida
    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error abriendo memoria compartida");
        return 1;
    }

    // Obtener tamaño
    struct stat shm_stat;
    fstat(shm_fd, &shm_stat);
    size_t shm_size = shm_stat.st_size;

    // Mapear memoria
    SharedMemory* shm = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("Error mapeando memoria");
        close(shm_fd);
        return 1;
    }

    // Abrir semáforo mutex
    sem_t* sem_mutex = sem_open(shm->sem_mutex_name, 0);
    if (sem_mutex == SEM_FAILED) {
        perror("Error abriendo semáforo mutex");
        munmap(shm, shm_size);
        close(shm_fd);
        return 1;
    }

    // Establecer flag de shutdown
    sem_wait(sem_mutex);
    shm->shutdown_flag = 1;
    int active_senders = shm->active_senders;
    int active_receivers = shm->active_receivers;
    sem_post(sem_mutex);

    // Esperar que terminen los procesos
    while (1) {
        sem_wait(sem_mutex);
        int still_active = shm->active_senders + shm->active_receivers;
        sem_post(sem_mutex);

        if (still_active == 0) break;
        usleep(500000); // 0.5 segundos
    }

    // Limpiar semáforos y memoria
    sem_close(sem_mutex);
    sem_unlink(shm->sem_empty_name);
    sem_unlink(shm->sem_full_name);
    sem_unlink(shm->sem_mutex_name);
    munmap(shm, shm_size);
    close(shm_fd);
    shm_unlink(shm_name);

    printf("Finalización completa. Recursos liberados.\n");

    return 0;
}
