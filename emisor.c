#include "shared_memory.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <'manual' | milisegundos> <llave_8bits>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // ✅ Lógica para parsear el modo de ejecución
    int is_manual = (strcmp(argv[1], "manual") == 0);
    long delay_ms = 0;
    if (!is_manual) {
        delay_ms = atol(argv[1]);
        if (delay_ms < 0) {
            fprintf(stderr, "El tiempo de espera debe ser un entero no negativo.\n");
            return EXIT_FAILURE;
        }
    }
    char key = (char)atoi(argv[2]);

    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Emisor: shm_open falló");
        return EXIT_FAILURE;
    }

    shared_data* temp_map = mmap(0, sizeof(shared_data), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (temp_map == MAP_FAILED) {
        perror("Emisor: mmap temporal falló");
        close(shm_fd);
        return EXIT_FAILURE;
    }
    size_t shm_size = sizeof(shared_data) + temp_map->buffer_size * sizeof(buffer_entry) + temp_map->buffer_size * sizeof(sem_t);
    munmap(temp_map, sizeof(shared_data));

    shared_data *data = mmap(0, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (data == MAP_FAILED) {
        perror("Emisor: mmap final falló");
        close(shm_fd);
        return EXIT_FAILURE;
    }
    close(shm_fd);

    signal(SIGTERM, sigterm_handler);

    sem_wait(&data->producer_mutex);
    data->total_emitters++;
    data->active_emitters++;
    sem_post(&data->producer_mutex);

    // ✅ NUEVO: Cada emisor tiene su propio índice de lectura (independiente)
    int my_source_index = 0;

    printf("Emisor (PID %d) iniciado en modo: %s\n", getpid(), is_manual ? "Manual" : "Automático");
    printf("  └─ Leerá el archivo completo desde el inicio de forma independiente.\n");

    while (keep_running && !data->shutdown_requested) {
        int slot_reservado = 0;

        if (is_manual) {
            printf("Presione Enter para enviar un caracter...\n");
            if (getchar() == EOF || !keep_running || data->shutdown_requested) {
                break;
            }
        }
        
        if (sem_wait(&data->empty_slots) == -1) {
            if (keep_running && !data->shutdown_requested) perror("sem_wait empty_slots");
            break;
        }
        slot_reservado = 1;

        if (!keep_running || data->shutdown_requested) {
            if(slot_reservado) sem_post(&data->empty_slots);
            break;
        }

        char original_char;
        // ✅ CAMBIO: Ahora usa el índice LOCAL en lugar del compartido
        if (my_source_index >= data->source_size) {
            printf("Emisor (PID %d): Fin del archivo de origen.\n", getpid());
            if(slot_reservado) sem_post(&data->empty_slots);
            break;
        }
        original_char = data->source_content[my_source_index];
        my_source_index++;

        sem_wait(&data->receiver_registry_mutex);

        sem_wait(&data->producer_mutex);
        int write_idx = data->write_index;
        data->write_index = (data->write_index + 1) % data->buffer_size;
        sem_post(&data->producer_mutex);

        sem_t* slot_mutexes = get_slot_mutexes(data);
        sem_wait(&slot_mutexes[write_idx]);

        data->buffer[write_idx].ascii_val = original_char ^ key;
        data->buffer[write_idx].index = write_idx;
        data->buffer[write_idx].read_count = data->active_receivers;

        // ✅ CORREGIDO: Se captura el tiempo al momento de la inserción
        time(&data->buffer[write_idx].timestamp);

        // ✅ CORREGIDO: Se usa la función centralizada con timestamp
        print_char_info("Emisor", getpid(), original_char, write_idx, data->buffer[write_idx].timestamp);

        sem_post(&slot_mutexes[write_idx]);

        data->total_chars_transferred++;

        if (data->active_receivers == 0) {
            sem_post(&data->empty_slots);
        } else {
            for (int i = 0; i < MAX_RECEIVERS; i++) {
                if (data->receivers[i].pid != 0) {
                    sem_post(&data->receivers[i].data_available);
                }
            }
        }
        sem_post(&data->receiver_registry_mutex);

        // ✅ Lógica de espera dinámica según modo
        if (!is_manual) {
            struct timespec ts;
            ts.tv_sec = delay_ms / 1000;
            ts.tv_nsec = (delay_ms % 1000) * 1000000;
            nanosleep(&ts, NULL);
        }
    }

    sem_wait(&data->producer_mutex);
    data->active_emitters--;
    sem_post(&data->producer_mutex);

    sem_post(&data->process_finished);

    printf("Emisor (PID %d) finalizando.\n", getpid());
    munmap(data, shm_size);

    return EXIT_SUCCESS;
}