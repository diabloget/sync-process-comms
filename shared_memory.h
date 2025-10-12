#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <time.h>
#include <semaphore.h>

#define MAX_FILENAME 256

// Estructura para cada elemento en el buffer circular
typedef struct {
    char ascii_value;           // Valor del carácter (codificado)
    int index;                  // Posición en el buffer
    struct timespec timestamp;  // Momento de inserción
    int read;                   // Flag para indicar si fue leído
} BufferElement;

// Estructura principal de memoria compartida
typedef struct {
    int buffer_size;            // Tamaño del buffer
    int write_pos;              // Posición de escritura
    int read_pos;               // Posición de lectura
    int count;                  // Elementos en buffer
    unsigned char key;          // Clave de encriptación
    char filename[MAX_FILENAME]; // Archivo fuente
    
    // Estadísticas
    long total_chars_transferred;
    int active_senders;
    int total_senders;
    int active_receivers;
    int total_receivers;
    int shutdown_flag;          // Flag para terminación
    
    // Semáforos nombrados 
    char sem_empty_name[64];
    char sem_full_name[64];
    char sem_mutex_name[64];
    
    // Buffer circular 
    BufferElement buffer[];     // Flexible array member
} SharedMemory;



// Funciones necesarias
void print_error(const char* message);
void format_timestamp(struct timespec* ts, char* buffer, size_t size);
unsigned char xor_encode(char c, unsigned char key);

#endif // SHARED_MEMORY_H