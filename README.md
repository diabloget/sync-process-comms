# Synchronized Process Communication System

## 📋 Abstract

This project implements a sophisticated synchronized inter-process communication system using shared memory in Linux. The system enables multiple producer and consumer processes to exchange encrypted character data through a circular buffer without busy waiting. Key features include XOR-based encryption/decryption, configurable automatic/manual operation modes, elegant real-time visualization with colored output, and graceful shutdown management. The implementation demonstrates advanced synchronization techniques using semaphores and signal handling to ensure efficient CPU utilization while maintaining data integrity across multiple concurrent processes.

---

# 🚀 Sistema de Comunicación de Procesos Sincronizada

## 📖 Descripción

Este proyecto implementa un sistema de comunicación entre procesos pesados (*Heavy Process*) utilizando memoria compartida en Linux. El sistema permite que múltiples procesos emisores y receptores intercambien datos de caracteres de manera sincronizada mediante un búfer circular, evitando completamente el *busy waiting*.

## 🏗️ Arquitectura del Sistema

### **4 Procesos Principales:**

#### 1. **Inicializador** (`inicializador.c`)
- Configura la memoria compartida y semáforos
- Carga el archivo fuente de caracteres
- Define el tamaño del búfer circular
- Parámetros: `<identificador_memoria> <cantidad_espacios> <archivo_origen>`

#### 2. **Emisor** (`emisor.c`)
- Codifica caracteres usando XOR con clave de 8 bits
- Inserta datos en el búfer circular de manera sincronizada
- Modos: Manual (espera Enter) o Automático (intervalo en ms)
- Parámetros: `<'manual' | milisegundos> <llave_8bits>`

#### 3. **Receptor** (`receptor.c`)
- Decodifica caracteres usando XOR con la misma clave
- Lee datos del búfer circular de manera sincronizada
- Crea archivo de salida individual por proceso
- Parámetros: `<'manual' | milisegundos> <llave_8bits>`

#### 4. **Finalizador** (`finalizador.c`)
- Gestiona apagado elegante de todos los procesos
- Espera señal Ctrl+C para iniciar shutdown
- Muestra estadísticas finales del sistema

## 🛠️ Instalación y Compilación

### **Requisitos:**
- Sistema Linux
- Compilador GCC
- Bibliotecas estándar de C

### **Compilación:**
```bash
make all
```

### **Limpieza:**
```bash
make clean
```

## 🎯 Uso del Sistema

### **1. Inicializar el sistema:**
```bash
./inicializador mem1 64 archivo_fuente.txt
```

### **2. Ejecutar emisores:**
```bash
# Modo automático (cada 500ms)
./emisor 500 42

# Modo manual
./emisor manual 42
```

### **3. Ejecutar receptores:**
```bash
# Modo automático (cada 300ms)
./receptor 300 42

# Modo manual  
./receptor manual 42
```

### **4. Finalizar el sistema:**
```bash
./finalizador
# Presionar Ctrl+C para iniciar apagado
```

## 🔒 Características de Sincronización

### **Mecanismos Implementados:**
- **Semáforos de slots vacíos**: Controlan espacio disponible en el búfer
- **Semáforos de datos disponibles**: Notifican a receptores sobre nuevos datos
- **Mutex de productor**: Protege índice de escritura
- **Mutex de registro**: Protege información de receptores
- **Mutex por slot**: Protege entradas individuales del búfer

### **Estructuras de Datos:**
```c
typedef struct {
    char ascii_val;        // Carácter codificado
    int index;             // Índice en el búfer
    time_t timestamp;      // Hora de inserción
    volatile int read_count; // Contador de lecturas pendientes
} buffer_entry;
```

## 🎨 Visualización en Tiempo Real

### **Salida Elegante con Colores:**
- **🟢 Emisores**: Verde
- **🔵 Receptores**: Azul  
- **Formato tabular alineado**
- **Timestamp en formato HH:MM:SS**
- **Caracteres especiales escapados** (\n, \t, \r)

### **Ejemplo de Salida:**
```
🟢 Emisor   (PID 1234)  | Carácter: 'H'  | Búfer[0]   | Hora: 14:30:25
🔵 Receptor (PID 1235)  | Carácter: 'H'  | Búfer[0]   | Hora: 14:30:25
```

## 📊 Estadísticas Finales

El finalizador muestra:
- Cantidad total de caracteres transferidos
- Capacidad del búfer
- Emisores activos/totales
- Receptores activos/totales  
- Memoria compartida utilizada

## 🔧 Características Técnicas

### **Sin Busy Waiting:**
- Uso de `sem_wait()` para bloqueo eficiente
- `pause()` en finalizador para espera de señales
- `nanosleep()` para retardos en modo automático
- `getchar()` para entrada manual bloqueante

### **Manejo Elegante de Errores:**
- Verificación de parámetros
- Manejo de errores de memoria compartida
- Limpieza apropiada de recursos
- Destrucción de semáforos

## 📁 Estructura de Archivos

```
proyecto/
├── common.h           # Definiciones y estructuras compartidas
├── inicializador.c    # Proceso inicializador
├── emisor.c          # Proceso emisor
├── receptor.c        # Proceso receptor  
├── finalizador.c     # Proceso finalizador
├── Makefile          # Script de compilación
└── README.md         # Este archivop
```

## ⚠️ Consideraciones Importantes

- No se permite sobreescritura de datos no leídos
- Todos los procesos acceden al búfer de manera circular
- Múltiples instancias de emisores y receptores pueden ejecutarse concurrentemente
- El sistema evita completamente el busy waiting
- Uso eficiente de memoria compartida

## 🎓 Atributos de Aprendizaje

Este proyecto refuerza:
- **Sincronización de procesos**
- **Comunicación interprocesos**
- **Programación concurrente**
- **Manejo de señales en Linux**
- **Gestión de memoria compartida**

---

**Desarrollado para el curso CE 4303 - Principios de Sistemas Operativos**  
**Tecnológico de Costa Rica - 2025**
