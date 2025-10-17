#include "shared_memory.h"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    // Mensaje inicial e instalación de señal Ctrl+C
    printf("Para iniciar el proceso de finalización presione Ctrl+C.\n");
    signal(SIGINT, sigterm_handler);
    
    // Abrir memoria compartida existente
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Finalizador: shm_open falló");
        return EXIT_FAILURE;
    }
    
    // Mapeo temporal para calcular tamaño real
    shared_data* temp_map = mmap(0, sizeof(shared_data), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (temp_map == MAP_FAILED) {
        perror("Finalizador: mmap temporal falló");
        close(shm_fd);
        return EXIT_FAILURE;
    }
    size_t shm_size = sizeof(shared_data) 
                    + temp_map->buffer_size * sizeof(buffer_entry)
                    + temp_map->buffer_size * sizeof(sem_t);
    munmap(temp_map, sizeof(shared_data));
    
    // Mapeo completo de la memoria
    shared_data *data = mmap(0, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (data == MAP_FAILED) {
        perror("Finalizador: mmap final falló");
        close(shm_fd);
        return EXIT_FAILURE;
    }
    close(shm_fd);
    
    // Esperar señal de interrupción 
    while (keep_running) {
        pause();
    }
    
    printf("\n\033[1;33mCtrl+C presionado, procedo con la finalización de procesos.\033[0m\n");

    data->shutdown_requested = 1; // Avisar a todos que deben cerrar

    // Despertar receptores dormidos y enviarles SIGTERM
    sem_wait(&data->receiver_registry_mutex);
    int total_processes = data->active_emitters + data->active_receivers;
    for (int i = 0; i < MAX_RECEIVERS; i++) {
        if (data->receivers[i].pid != 0) {
            sem_post(&data->receivers[i].data_available);
            kill(data->receivers[i].pid, SIGTERM);
        }
    }
    sem_post(&data->receiver_registry_mutex);
    
    // Despertar emisores bloqueados por falta de espacio
    int buffer_size = data->buffer_size;
    for (int i = 0; i < buffer_size; ++i) {
        sem_post(&data->empty_slots);
    }
    
    // Enviar SIGTERM a los procesos emisor
    system("pkill -SIGTERM emisor 2>/dev/null");
    
    printf("Esperando a que todos los procesos finalicen (%d procesos activos)\n", total_processes);
    
    // Esperar que cada proceso confirme su finalización
    for (int i = 0; i < total_processes; i++) {
        if (sem_wait(&data->process_finished) == -1) {
            perror("Error esperando finalización de procesos");
            break;
        }
        int remaining = total_processes - i - 1;
        if (remaining > 0) {
            printf("  %d proceso(s) restante(s)\n", remaining);
        }
    }
    
    printf("\033[1;32mProcesos finalizados con exito.\033[0m\n");

    // Estadisticas
    // Mostrar estadísticas
    printf("\n\033[1;36m⸻⸻⸻⸻   Estadísticas de Ejecución ⸻⸻⸻⸻\033[0m\n");
    printf("  * Total de emisores conectados: \033[0;33m%d\033[0m\n", data->total_emitters);
    printf("  * Total de receptores conectados: \033[0;33m%d\033[0m\n", data->total_receivers);
    printf("  * Caracteres transferidos: \033[0;32m%ld\033[0m\n", data->total_chars_transferred);
    printf("  * Emisores activos al finalizar: \033[0;31m%d\033[0m\n", data->active_emitters);
    printf("  * Receptores activos al finalizar: \033[0;31m%d\033[0m\n", data->active_receivers);
    printf("\033[1;36m⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻⸻\033[0m\n");

    printf("\nProcedo a liberar recursos del sistema.\n");
    
    // Destruir semáforos 
    sem_destroy(&data->empty_slots);
    sem_destroy(&data->producer_mutex);
    sem_destroy(&data->receiver_registry_mutex);
    sem_destroy(&data->process_finished);
    
    // Destruir semáforos de receptores
    for (int i = 0; i < MAX_RECEIVERS; ++i) {
        sem_destroy(&data->receivers[i].data_available);
    }

    // Destruir semáforos por cada slot del búfer
    sem_t* slot_mutexes = get_slot_mutexes(data);
    for (int i = 0; i < buffer_size; ++i) {
        sem_destroy(&slot_mutexes[i]);
    }
    
    // Liberar memoria compartida
    munmap(data, shm_size);
    shm_unlink(SHM_NAME);
    
    printf("\033[1;32mLimpieza completa.\033[0m\n");
    return EXIT_SUCCESS;
}
