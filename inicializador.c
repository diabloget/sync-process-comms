#include "shared_memory.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <identificador_memoria> <cantidad_espacios> <archivo_origen>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int buffer_size = atoi(argv[2]);
    const char* source_file = argv[3];

    if (buffer_size <= 0 || buffer_size > MAX_BUFFER_SIZE) {
        fprintf(stderr, "La cantidad de espacios debe ser un entero positivo (max %d).\n", MAX_BUFFER_SIZE);
        return EXIT_FAILURE;
    }

    shm_unlink(SHM_NAME);

    // CAMBIO: El tamaño ahora incluye espacio para los mutex de cada slot del búfer.
    size_t shm_size = sizeof(shared_data) + 
                      buffer_size * sizeof(buffer_entry) + 
                      buffer_size * sizeof(sem_t);

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("No se pudo crear la memoria compartida");
        return EXIT_FAILURE;
    }
    
    if (ftruncate(shm_fd, shm_size) == -1) {
        perror("No se pudo establecer el tamaño de la memoria compartida");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }
    
    shared_data *data = mmap(0, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (data == MAP_FAILED) {
        perror("No se pudo mapear la memoria compartida");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }
    close(shm_fd);

    FILE* file = fopen(source_file, "r");
    if (!file) {
        perror("No se pudo abrir el archivo de origen");
        munmap(data, shm_size);
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }
    data->source_size = fread(data->source_content, 1, MAX_FILE_SIZE - 1, file);
    data->source_content[data->source_size] = '\0';
    fclose(file);

    data->buffer_size = buffer_size;
    data->write_index = 0;
    data->source_read_index = 0;
    data->active_emitters = 0;
    data->total_emitters = 0;
    data->active_receivers = 0;
    data->total_receivers = 0;
    data->total_chars_transferred = 0;
    data->shutdown_requested = 0;
    
    // Inicializar el registro de receptores y sus semáforos individuales
    for (int i = 0; i < MAX_RECEIVERS; ++i) {
        data->receivers[i].pid = 0;
        data->receivers[i].read_index = 0;
        data->receivers[i].is_manual = 0;
        // NUEVO: Inicializar el semáforo personal de cada receptor.
        if (sem_init(&data->receivers[i].data_available, 1, 0) == -1) {
            perror("Error al inicializar semáforo de receptor");
            // Aquí faltaría destruir los ya creados antes de salir
            return EXIT_FAILURE;
        }
    }
    
    for (int i = 0; i < buffer_size; i++) {
        data->buffer[i].ascii_val = 0;
        data->buffer[i].index = 0;
        data->buffer[i].read_count = 0;
    }
    
    // NUEVO: Obtener puntero a los mutex de slot e inicializarlos
    sem_t *slot_mutexes = get_slot_mutexes(data);
    for (int i = 0; i < buffer_size; i++) {
        if (sem_init(&slot_mutexes[i], 1, 1) == -1) { // 1 = unlocked
            perror("Error al inicializar un mutex de slot");
            return EXIT_FAILURE;
        }
    }

    // Inicializar los semáforos principales
    if (sem_init(&data->empty_slots, 1, buffer_size) == -1) { // Inicia con N espacios libres
        perror("Error al inicializar empty_slots");
        return EXIT_FAILURE;
    }
    if (sem_init(&data->producer_mutex, 1, 1) == -1) {
        perror("Error al inicializar producer_mutex");
        return EXIT_FAILURE;
    }
    if (sem_init(&data->receiver_registry_mutex, 1, 1) == -1) {
        perror("Error al inicializar receiver_registry_mutex");
        return EXIT_FAILURE;
    }
    if (sem_init(&data->process_finished, 1, 0) == -1) {
    perror("Error al inicializar process_finished");
    return EXIT_FAILURE;
    }
    
    printf("\033[1;32m✓ Memoria compartida inicializada correctamente (v2).\033[0m\n");
    printf("  • Búfer: %d espacios\n", buffer_size);
    printf("  • Contenido del archivo '%s' cargado (%d bytes)\n", source_file, data->source_size);
    printf("  • Tamaño total de memoria compartida: %zu bytes\n", shm_size);

    munmap(data, shm_size);
    return EXIT_SUCCESS;
}