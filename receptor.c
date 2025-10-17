#include "shared_memory.h"

int main(int argc, char *argv[]) {
    // Validar argumentos
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <'manual' | milisegundos> <llave_8bits>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Determinar modo de ejecución
    int is_manual = (strcmp(argv[1], "manual") == 0);
    long delay_ms = 0;
    if (!is_manual) {
        delay_ms = atol(argv[1]);
        if (delay_ms < 0) {
            fprintf(stderr, "El tiempo debe ser no negativo.\n");
            return EXIT_FAILURE;
        }
    }
    char key = (char)atoi(argv[2]);
    
    int my_slot = -1; // Posición del receptor en la tabla

    // Abrir memoria compartida existente
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Receptor: shm_open falló");
        return EXIT_FAILURE;
    }

    // Mapeo temporal para conocer tamaño total
    shared_data* temp_map = mmap(0, sizeof(shared_data), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (temp_map == MAP_FAILED) {
        perror("Receptor: mmap temporal falló");
        close(shm_fd);
        return EXIT_FAILURE;
    }

    size_t shm_size = sizeof(shared_data)
        + temp_map->buffer_size * sizeof(buffer_entry)
        + temp_map->buffer_size * sizeof(sem_t);
    munmap(temp_map, sizeof(shared_data));

    // Mapeo completo con permisos de lectura/escritura
    shared_data *data = mmap(0, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (data == MAP_FAILED) {
        perror("Receptor: mmap final falló");
        close(shm_fd);
        return EXIT_FAILURE;
    }
    close(shm_fd);

    // Manejar señal de terminación
    signal(SIGTERM, sigterm_handler);

    // Crear archivo de salida
    char filename[50];
    sprintf(filename, "output_receptor_%d.txt", getpid());
    FILE *output_file = fopen(filename, "w");
    if (!output_file) {
        perror("No se pudo crear el archivo de salida");
        munmap(data, shm_size);
        return EXIT_FAILURE;
    }

    // Registrar el receptor en la memoria compartida
    sem_wait(&data->receiver_registry_mutex);
    for (int i = 0; i < MAX_RECEIVERS; ++i) {
        if (data->receivers[i].pid == 0) {
            my_slot = i;
            data->receivers[i].pid = getpid();
            data->receivers[i].read_index = data->write_index;
            data->receivers[i].is_manual = is_manual;
            break;
        }
    }
    if (my_slot != -1) {
        data->total_receivers++;
        data->active_receivers++;
    }
    sem_post(&data->receiver_registry_mutex);

    // Se sale si noy espacio
    if (my_slot == -1) {
        fprintf(stderr, "No hay slots disponibles para receptores\n");
        fclose(output_file);
        munmap(data, shm_size);
        return EXIT_FAILURE;
    }

    printf("Receptor (PID %d) en modo %s. Escribiendo a %s\n",
           getpid(), is_manual ? "Manual" : "Automático", filename);

    // Bucle principal
    while (keep_running && !data->shutdown_requested) {
        // Espera manual
        if (is_manual) {
            printf("Presione Enter para leer el siguiente caracter...\n");
            if (getchar() == EOF || !keep_running || data->shutdown_requested) break;
        }

        // Esperar dato disponible
        if (sem_wait(&data->receivers[my_slot].data_available) == -1) {
            if (keep_running && !data->shutdown_requested) perror("sem_wait data_available");
            break;
        }
        if (!keep_running || data->shutdown_requested) break;

        // Leer posición actual
        int my_read_idx = data->receivers[my_slot].read_index;
        
        // Bloquear acceso al slot
        sem_t* slot_mutexes = get_slot_mutexes(data);
        sem_wait(&slot_mutexes[my_read_idx]);
        
        // Descifrar carácter
        char decrypted_char = data->buffer[my_read_idx].ascii_val ^ key;
        time_t insertion_time = data->buffer[my_read_idx].timestamp;
                
        // Decrementar cantidad de lectores restantes
        int reads_after = __sync_sub_and_fetch(&data->buffer[my_read_idx].read_count, 1);

        // Mostrar información
        print_char_info("Receptor", getpid(), decrypted_char, my_read_idx, insertion_time);

        // Si es el último lector, liberar el espacio
        if (reads_after == 0) {
            printf("     └─ ¡Último lector! Búfer[%d] liberado.\n", my_read_idx);
            sem_post(&data->empty_slots);
        } else {
            printf("     └─ Quedan %d lectores para el búfer[%d].\n", reads_after, my_read_idx);
        }

        // Liberar el slot
        sem_post(&slot_mutexes[my_read_idx]);
        
        // Avanzar al siguiente índice
        data->receivers[my_slot].read_index = (my_read_idx + 1) % data->buffer_size;

        // Escribir en el archivo
        fputc(decrypted_char, output_file);
        fflush(output_file);

        // Espera automática
        if (!is_manual) {
            struct timespec ts;
            ts.tv_sec = delay_ms / 1000;
            ts.tv_nsec = (delay_ms % 1000) * 1000000;
            nanosleep(&ts, NULL);
        }
    }

    // Eliminar receptor del registro
    sem_wait(&data->receiver_registry_mutex);
    if(my_slot != -1) {
        data->receivers[my_slot].pid = 0;
        data->active_receivers--;
    }
    sem_post(&data->receiver_registry_mutex);

    // Notificar finalización
    sem_post(&data->process_finished);
    
    printf("Receptor (PID %d) finalizando.\n", getpid());
    fclose(output_file);
    munmap(data, shm_size);

    return EXIT_SUCCESS;
}
