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

// Codigos de color ANSI
#define COLOR_RESET     "\033[0m"
#define COLOR_BOLD      "\033[1m"

// Colores para el emisor
#define COLOR_EMISOR_HEADER   "\033[1;32m"  // Verde brillante
#define COLOR_EMISOR_CHAR     "\033[0;36m"  // Cian
#define COLOR_EMISOR_BUFFER   "\033[0;33m"  // Amarillo
#define COLOR_EMISOR_TIME     "\033[0;35m"  // Magenta

// Colores para el receptor
#define COLOR_RECEPTOR_HEADER "\033[1;34m"  // Azul brillante
#define COLOR_RECEPTOR_CHAR   "\033[0;36m"  // Cian
#define COLOR_RECEPTOR_BUFFER "\033[0;33m"  // Amarillo
#define COLOR_RECEPTOR_TIME   "\033[0;35m"  // Magenta

// Colores para info adicional
#define COLOR_INFO      "\033[0;90m"  // Gris
#define COLOR_SUCCESS   "\033[0;32m"  // Verde
#define COLOR_WARNING   "\033[0;33m"  // Amarillo
#define COLOR_ERROR     "\033[0;31m"  // Rojo

// Bandera usada para finalizar procesos
static volatile sig_atomic_t keep_running = 1;

// Estructura que representa una entrada del búfer compartido
typedef struct {
    char ascii_val;       // Carácter almacenado
    int index;            // Posición en el búfer
    time_t timestamp;     // Momento en que se guardó
    volatile int read_count; // Cantidad de receptores que lo han leído
} buffer_entry;

// Información de cada receptor registrado
typedef struct {
    pid_t pid;            // PID del receptorDame este codigo con comentarios, no los hagas muy elaborados
    int read_index;       // Posición actual de lectura
    int is_manual;        // Indica si el receptor es manual
    sem_t data_available; // Semáforo de datos disponibles
} receiver_info;

// Estructura principal de la memoria compartida
typedef struct {
    // Semáforos globales
    sem_t empty_slots;              // Controla espacios vacíos en el búfer
    sem_t producer_mutex;           // Exclusión mutua para el emisor
    sem_t receiver_registry_mutex;  // Protege el registro de receptores
    sem_t process_finished;         // Indica cuando un proceso termina

    // Datos de control
    int buffer_size;                // Tamaño del búfer
    int write_index;                // Índice de escritura
    volatile sig_atomic_t shutdown_requested; // Señal de apagado

    // Fuente de datos (para el emisor)
    char source_content[MAX_FILE_SIZE];
    int source_size;
    int source_read_index;

    // Información de receptores
    receiver_info receivers[MAX_RECEIVERS];
    int total_emitters;
    int active_emitters;
    int total_receivers;
    int active_receivers;
    long total_chars_transferred;   // Estadística de caracteres enviados

    // Búfer de datos dinámico
    buffer_entry buffer[];
} shared_data;

// Calcula la dirección donde inician los semáforos por slot
static inline sem_t* get_slot_mutexes(shared_data* data) {
    return (sem_t*) &data->buffer[data->buffer_size];
}

// Función para imprimir información de cada carácter procesado
static inline void print_char_info(const char* role, pid_t pid, char c, int index, time_t ts) {
    char time_str[10];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&ts));

    char display_char[5];
    switch (c) {
        case '\n': strcpy(display_char, "\\n"); break;
        case '\r': strcpy(display_char, "\\r"); break;
        case '\t': strcpy(display_char, "\\t"); break;
        case ' ':  strcpy(display_char, "⎵");  break;
        default:
            if (c >= 32 && c <= 126) sprintf(display_char, "%c", c);
            else sprintf(display_char, "?");
            break;
    }

    if (strcmp(role, "Emisor") == 0) {
        printf("%s%-8s%s (PID %s%d%s) │ Carácter: %s'%s'%s │ Búfer%s[%d]%s │ Hora: %s%s%s\n",
               COLOR_EMISOR_HEADER, role, COLOR_RESET,
               COLOR_INFO, pid, COLOR_RESET,
               COLOR_EMISOR_CHAR, display_char, COLOR_RESET,
               COLOR_EMISOR_BUFFER, index, COLOR_RESET,
               COLOR_EMISOR_TIME, time_str, COLOR_RESET);
    } else {
        printf("%s%-8s%s (PID %s%d%s) │ Carácter: %s'%s'%s │ Búfer%s[%d]%s │ Hora: %s%s%s\n",
               COLOR_RECEPTOR_HEADER, role, COLOR_RESET,
               COLOR_INFO, pid, COLOR_RESET,
               COLOR_RECEPTOR_CHAR, display_char, COLOR_RESET,
               COLOR_RECEPTOR_BUFFER, index, COLOR_RESET,
               COLOR_RECEPTOR_TIME, time_str, COLOR_RESET);
    }
}

// Manejador de señal para apagar los procesos limpiamente
__attribute__((unused))
static void sigterm_handler(int signum) {
    (void)signum;
    keep_running = 0;
}

#endif // SHARED_MEMORY_H
