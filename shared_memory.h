#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

#define SHM_NAME "mem"
#define MAX_BUFFER_SIZE 256
#define MAX_RECEIVERS 50
#define MAX_FILE_SIZE 4096

// ✅ CORREGIDO: Agregado 'static' para evitar múltiples definiciones
static volatile sig_atomic_t keep_running = 1;

// Estructura para cada entrada del búfer
typedef struct {
    char ascii_val;
    int index;
    time_t timestamp; // ✅ CORREGIDO: Restaurado - REQUERIDO por el PDF
    volatile int read_count; 
} buffer_entry;

// Estructura para registrar a cada receptor
typedef struct {
    pid_t pid;
    int read_index;
    int is_manual;
    sem_t data_available;
} receiver_info;

// Estructura principal de la memoria compartida
typedef struct {
    sem_t empty_slots;
    sem_t producer_mutex;
    sem_t receiver_registry_mutex;
    sem_t process_finished;


    int buffer_size;
    int write_index;
    volatile sig_atomic_t shutdown_requested;
    char source_content[MAX_FILE_SIZE];
    int source_size;
    int source_read_index;
    receiver_info receivers[MAX_RECEIVERS];
    int total_emitters;
    int active_emitters;
    int total_receivers;
    int active_receivers;
    long total_chars_transferred;
    buffer_entry buffer[];
} shared_data;

static inline sem_t* get_slot_mutexes(shared_data* data) {
    return (sem_t*) &data->buffer[data->buffer_size];
}

// ✅ CORREGIDO: Restaurada función centralizada para impresión elegante
static inline void print_char_info(const char* role, pid_t pid, char c, int index, time_t ts) {
    char time_str[10];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&ts));

    char display_char[5];
    switch (c) {
        case '\n': strcpy(display_char, "\\n");  break;
        case '\r': strcpy(display_char, "\\r");  break;
        case '\t': strcpy(display_char, "\\t");  break;
        default:
            if (c >= 32 && c <= 126) {
                sprintf(display_char, "%c", c);
            } else {
                sprintf(display_char, "?");
            }
            break;
    }
    
    const char* color = (strcmp(role, "Emisor") == 0) ? "\033[0;32m" : "\033[0;34m";
    const char* reset_color = "\033[0m";
    
    printf("%s%-8s (PID %-5d) | Carácter: '%-2s' | Búfer[%-3d] | Hora: %s%s\n",
           color, role, pid, display_char, index, time_str, reset_color);
}

// ✅ CORREGIDO: Agregado 'static' y '__attribute__((unused))' para evitar warnings
__attribute__((unused))
static void sigterm_handler(int signum) {
    (void)signum;
    keep_running = 0;
}

#endif // SHARED_MEMORY_H