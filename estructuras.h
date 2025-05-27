/*
 * =============================================================================
 * Proyecto: Sistema de Préstamo de Libros
 * Archivo: estructuras.h
 * Autor: Samuel Emperador
 * Fecha: 23/05/2025
 * Descripción: Archivo de cabecera que contiene todas las estructuras de datos,
 *              enums, constantes y declaraciones de funciones compartidas
 *              entre los procesos solicitante y receptor del sistema.
 * Incluye:
 *   - Definiciones de tipos de datos para libros y ejemplares
 *   - Estructuras para comunicación IPC (solicitudes y respuestas)
 *   - Buffer circular para sincronización entre hilos
 *   - Funciones utilitarias para manejo de fechas y validaciones
 * =============================================================================
 */

#ifndef ESTRUCTURAS_H
#define ESTRUCTURAS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

// Definiciones de constantes
#define MAX_STRING 256
#define MAX_BOOKS 100
#define MAX_COPIES 10
#define BUFFER_SIZE 10
#define MAX_LINE 512

// Tipos de operaciones
typedef enum {
    OP_DEVOLVER = 'D',
    OP_RENOVAR = 'R',
    OP_PRESTAR = 'P',
    OP_SALIR = 'Q'
} operation_t;

// Estados de los ejemplares
typedef enum {
    STATUS_DISPONIBLE = 'D',
    STATUS_PRESTADO = 'P'
} status_t;

// Estructura para un ejemplar de libro
typedef struct {
    int numero;
    status_t status;
    char fecha[12];  // formato: dd-mm-yyyy
} ejemplar_t;

// Estructura para un libro
typedef struct {
    char nombre[MAX_STRING];
    int isbn;
    int num_ejemplares;
    ejemplar_t ejemplares[MAX_COPIES];
} libro_t;

// Estructura para una solicitud
typedef struct {
    operation_t operacion;
    char nombre_libro[MAX_STRING];
    int isbn;
    int pid_solicitante;
} solicitud_t;

// Estructura para respuesta
typedef struct {
    int exito;  // 1 si éxito, 0 si fallo
    char mensaje[MAX_STRING];
    char fecha_devolucion[12];  // Para renovaciones
} respuesta_t;

// Estructura para el buffer productor-consumidor
typedef struct {
    solicitud_t buffer[BUFFER_SIZE];
    int in;
    int out;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} circular_buffer_t;

// Estructura para reporte
typedef struct {
    char status;
    char nombre[MAX_STRING];
    int isbn;
    int ejemplar;
    char fecha[12];
} reporte_entry_t;

// Variables globales compartidas
extern libro_t biblioteca[MAX_BOOKS];
extern int num_libros;
extern circular_buffer_t buffer_renovaciones;
extern reporte_entry_t reportes[1000];
extern int num_reportes;
extern pthread_mutex_t bd_mutex;
extern pthread_mutex_t reporte_mutex;
extern int verbose_mode;
extern int terminar_programa;

// Funciones comunes
void obtener_fecha_actual(char *fecha);
void agregar_dias_fecha(char *fecha, int dias);
int validar_isbn(int isbn);
void imprimir_verbose(const char *mensaje, solicitud_t *sol);

#endif // ESTRUCTURAS_H
