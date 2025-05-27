/*
 * =============================================================================
 * Proyecto: Sistema de Préstamo de Libros - Proceso Receptor
 * Archivo: receptor.c
 * Autor: [Tu Nombre]
 * Fecha: 23/05/2025
 * Descripción: Implementa el proceso receptor que maneja la base de datos de
 *              libros y procesa solicitudes de préstamo, devolución y renovación
 *              usando múltiples hilos y sincronización con mutexes.
 * Funcionalidades:
 *   - Gestión de base de datos de libros
 *   - Procesamiento concurrente de solicitudes
 *   - Buffer circular para renovaciones/devoluciones
 *   - Generación de reportes
 *   - Comunicación por pipes nombrados
 * =============================================================================
 */
#include "estructuras.h"

// Variables globales
libro_t biblioteca[MAX_BOOKS];
int num_libros = 0;
circular_buffer_t buffer_renovaciones;
reporte_entry_t reportes[1000];
int num_reportes = 0;
pthread_mutex_t bd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t reporte_mutex = PTHREAD_MUTEX_INITIALIZER;
int verbose_mode = 0;
int terminar_programa = 0;

char pipe_name[MAX_STRING];
char archivo_datos[MAX_STRING];
char archivo_salida[MAX_STRING];
int usar_archivo_salida = 0;

// Implementación de funciones comunes
void obtener_fecha_actual(char *fecha) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(fecha, 12, "%d-%m-%Y", tm_info);
}

void agregar_dias_fecha(char *fecha, int dias) {
    struct tm tm_fecha = {0};
    sscanf(fecha, "%d-%d-%d", &tm_fecha.tm_mday, &tm_fecha.tm_mon, &tm_fecha.tm_year);
    tm_fecha.tm_mon--; // tm_mon es 0-11
    tm_fecha.tm_year -= 1900; // tm_year desde 1900

    time_t tiempo = mktime(&tm_fecha);
    tiempo += dias * 24 * 60 * 60; // agregar días

    struct tm *nueva_fecha = localtime(&tiempo);
    strftime(fecha, 12, "%d-%m-%Y", nueva_fecha);
}

int validar_isbn(int isbn) {
    return isbn > 0;
}

void imprimir_verbose(const char *mensaje, solicitud_t *sol) {
    if (verbose_mode) {
        printf("[VERBOSE] %s: %c, %s, %d (PID: %d)\n",
               mensaje, sol->operacion, sol->nombre_libro, sol->isbn, sol->pid_solicitante);
    }
}

// Función para inicializar el buffer circular
void init_buffer() {
    buffer_renovaciones.in = 0;
    buffer_renovaciones.out = 0;
    buffer_renovaciones.count = 0;
    pthread_mutex_init(&buffer_renovaciones.mutex, NULL);
    pthread_cond_init(&buffer_renovaciones.not_full, NULL);
    pthread_cond_init(&buffer_renovaciones.not_empty, NULL);
}

// Función para agregar al buffer (productor)
void buffer_put(solicitud_t *sol) {
    pthread_mutex_lock(&buffer_renovaciones.mutex);

    while (buffer_renovaciones.count == BUFFER_SIZE) {
        pthread_cond_wait(&buffer_renovaciones.not_full, &buffer_renovaciones.mutex);
    }

    buffer_renovaciones.buffer[buffer_renovaciones.in] = *sol;
    buffer_renovaciones.in = (buffer_renovaciones.in + 1) % BUFFER_SIZE;
    buffer_renovaciones.count++;

    pthread_cond_signal(&buffer_renovaciones.not_empty);
    pthread_mutex_unlock(&buffer_renovaciones.mutex);
}

// Función para obtener del buffer (consumidor)
int buffer_get(solicitud_t *sol) {
    pthread_mutex_lock(&buffer_renovaciones.mutex);

    while (buffer_renovaciones.count == 0 && !terminar_programa) {
        pthread_cond_wait(&buffer_renovaciones.not_empty, &buffer_renovaciones.mutex);
    }

    if (terminar_programa && buffer_renovaciones.count == 0) {
        pthread_mutex_unlock(&buffer_renovaciones.mutex);
        return 0; // No hay más elementos
    }

    *sol = buffer_renovaciones.buffer[buffer_renovaciones.out];
    buffer_renovaciones.out = (buffer_renovaciones.out + 1) % BUFFER_SIZE;
    buffer_renovaciones.count--;

    pthread_cond_signal(&buffer_renovaciones.not_full);
    pthread_mutex_unlock(&buffer_renovaciones.mutex);

    return 1; // Elemento obtenido
}

// Función para cargar la base de datos
int cargar_base_datos() {
    FILE *file = fopen(archivo_datos, "r");
    if (!file) {
        perror("Error abriendo archivo de datos");
        return -1;
    }

    char linea[MAX_LINE];
    num_libros = 0;

    while (fgets(linea, sizeof(linea), file) && num_libros < MAX_BOOKS) {
        // Remover salto de línea
        linea[strcspn(linea, "\n")] = 0;

        // Parsear información del libro
        if (sscanf(linea, "%[^,], %d, %d",
                   biblioteca[num_libros].nombre,
                   &biblioteca[num_libros].isbn,
                   &biblioteca[num_libros].num_ejemplares) != 3) {
            continue;
        }

        // Leer información de cada ejemplar
        for (int i = 0; i < biblioteca[num_libros].num_ejemplares; i++) {
            if (!fgets(linea, sizeof(linea), file)) break;
            linea[strcspn(linea, "\n")] = 0;

            char status_char;
            if (sscanf(linea, "%d, %c, %s",
                       &biblioteca[num_libros].ejemplares[i].numero,
                       &status_char,
                       biblioteca[num_libros].ejemplares[i].fecha) == 3) {
                biblioteca[num_libros].ejemplares[i].status = (status_t)status_char;
            }
        }

        num_libros++;
    }

    fclose(file);
    printf("Base de datos cargada: %d libros\n", num_libros);
    return 0;
}

// Función para encontrar un libro por ISBN
int encontrar_libro(int isbn) {
    for (int i = 0; i < num_libros; i++) {
        if (biblioteca[i].isbn == isbn) {
            return i;
        }
    }
    return -1;
}

// Función para agregar entrada al reporte
void agregar_reporte(char status, const char *nombre, int isbn, int ejemplar, const char *fecha) {
    pthread_mutex_lock(&reporte_mutex);

    if (num_reportes < 1000) {
        reportes[num_reportes].status = status;
        strcpy(reportes[num_reportes].nombre, nombre);
        reportes[num_reportes].isbn = isbn;
        reportes[num_reportes].ejemplar = ejemplar;
        strcpy(reportes[num_reportes].fecha, fecha);
        num_reportes++;
    }

    pthread_mutex_unlock(&reporte_mutex);
}

// Función para procesar devolución
void procesar_devolucion(solicitud_t *sol) {
    pthread_mutex_lock(&bd_mutex);

    int libro_idx = encontrar_libro(sol->isbn);
    if (libro_idx != -1) {
        // Buscar ejemplar prestado
        for (int i = 0; i < biblioteca[libro_idx].num_ejemplares; i++) {
            if (biblioteca[libro_idx].ejemplares[i].status == STATUS_PRESTADO) {
                biblioteca[libro_idx].ejemplares[i].status = STATUS_DISPONIBLE;
                obtener_fecha_actual(biblioteca[libro_idx].ejemplares[i].fecha);

                agregar_reporte('D', biblioteca[libro_idx].nombre, sol->isbn,
                               biblioteca[libro_idx].ejemplares[i].numero,
                               biblioteca[libro_idx].ejemplares[i].fecha);
                break;
            }
        }
    }

    pthread_mutex_unlock(&bd_mutex);
}

// Función para procesar renovación
void procesar_renovacion(solicitud_t *sol) {
    respuesta_t resp_renovacion;
    pthread_mutex_lock(&bd_mutex);
    int libro_idx = encontrar_libro(sol->isbn);
    if (libro_idx == -1) {
        resp_renovacion.exito = 0;
        strcpy(resp_renovacion.mensaje, "Libro no encontrado");
        strcpy(resp_renovacion.fecha_devolucion, "");
    } else {
        // Buscar ejemplar prestado
        int encontrado = 0;
        for (int i = 0; i < biblioteca[libro_idx].num_ejemplares; i++) {
            if (biblioteca[libro_idx].ejemplares[i].status == STATUS_PRESTADO) {
                // Renovar por 7 días más
                agregar_dias_fecha(biblioteca[libro_idx].ejemplares[i].fecha, 7);

                resp_renovacion.exito = 1;
                // MENSAJE MÁS CORTO PARA EVITAR TRUNCACIÓN
                strcpy(resp_renovacion.mensaje, "Renovación exitosa");
                strcpy(resp_renovacion.fecha_devolucion,
                       biblioteca[libro_idx].ejemplares[i].fecha);
                agregar_reporte('R', biblioteca[libro_idx].nombre, sol->isbn,
                               biblioteca[libro_idx].ejemplares[i].numero,
                               biblioteca[libro_idx].ejemplares[i].fecha);
                encontrado = 1;
                break;
            }
        }
        if (!encontrado) {
            resp_renovacion.exito = 0;
            strcpy(resp_renovacion.mensaje, "No hay ejemplares prestados para renovar");
            strcpy(resp_renovacion.fecha_devolucion, "");
        }
    }
    pthread_mutex_unlock(&bd_mutex);
    // Enviar respuesta específica
    enviar_respuesta(sol->pid_solicitante, &resp_renovacion);
    buffer_put(sol);
}

// Hilo auxiliar 1 para procesar devoluciones y renovaciones
void* hilo_auxiliar1(void *arg) {
    (void)arg; // Suprimir warning de parámetro no usado

    solicitud_t sol;

    while (buffer_get(&sol)) {
        if (sol.operacion == OP_DEVOLVER) {
            procesar_devolucion(&sol);
        } else if (sol.operacion == OP_RENOVAR) {
            procesar_renovacion(&sol);
        }
    }

    return NULL;
}

// Hilo auxiliar 2 para comandos de consola
void* hilo_auxiliar2(void *arg) {
    (void)arg;

    char comando;
    while (!terminar_programa) {
        printf("Ingrese comando (s=salir, r=reporte): ");
        if (scanf(" %c", &comando) != 1) {
            continue;
        }

        if (comando == 's') {
            printf("Terminando programa...\n");
            terminar_programa = 1;

            // Despertar al hilo auxiliar1
            pthread_mutex_lock(&buffer_renovaciones.mutex);
            pthread_cond_broadcast(&buffer_renovaciones.not_empty);
            pthread_mutex_unlock(&buffer_renovaciones.mutex);

            break;
        } else if (comando == 'r') {
            printf("\n=== REPORTE DE OPERACIONES ===\n");
            printf("Status, Nombre del Libro, ISBN, Ejemplar, Fecha\n");

            pthread_mutex_lock(&reporte_mutex);
            for (int i = 0; i < num_reportes; i++) {
                printf("%c, %s, %d, %d, %s\n",
                       reportes[i].status, reportes[i].nombre,
                       reportes[i].isbn, reportes[i].ejemplar,
                       reportes[i].fecha);
            }
            pthread_mutex_unlock(&reporte_mutex);

            printf("=== FIN REPORTE ===\n\n");
        }
    }

    return NULL;
}

// Función para enviar respuesta
void enviar_respuesta(int pid_solicitante, respuesta_t *resp) {
    char pipe_respuesta[MAX_STRING];
    snprintf(pipe_respuesta, sizeof(pipe_respuesta), "/tmp/resp_%d", pid_solicitante);

    int resp_fd = open(pipe_respuesta, O_WRONLY);
    if (resp_fd != -1) {
        write(resp_fd, resp, sizeof(respuesta_t));
        close(resp_fd);
    }
}

// Función para procesar préstamo
void procesar_prestamo(solicitud_t *sol) {
    respuesta_t resp;

    pthread_mutex_lock(&bd_mutex);

    int libro_idx = encontrar_libro(sol->isbn);
    if (libro_idx == -1) {
        resp.exito = 0;
        strcpy(resp.mensaje, "Libro no encontrado");
    } else {
        // Buscar ejemplar disponible
        int ejemplar_disponible = -1;
        for (int i = 0; i < biblioteca[libro_idx].num_ejemplares; i++) {
            if (biblioteca[libro_idx].ejemplares[i].status == STATUS_DISPONIBLE) {
                ejemplar_disponible = i;
                break;
            }
        }

        if (ejemplar_disponible == -1) {
            resp.exito = 0;
            strcpy(resp.mensaje, "No hay ejemplares disponibles");
        } else {
            // Prestar el libro
            biblioteca[libro_idx].ejemplares[ejemplar_disponible].status = STATUS_PRESTADO;
            obtener_fecha_actual(biblioteca[libro_idx].ejemplares[ejemplar_disponible].fecha);
            agregar_dias_fecha(biblioteca[libro_idx].ejemplares[ejemplar_disponible].fecha, 7);

            resp.exito = 1;
            snprintf(resp.mensaje, sizeof(resp.mensaje),
                    "Libro prestado exitosamente. Fecha de devolución: %s",
                    biblioteca[libro_idx].ejemplares[ejemplar_disponible].fecha);

            agregar_reporte('P', biblioteca[libro_idx].nombre, sol->isbn,
                           biblioteca[libro_idx].ejemplares[ejemplar_disponible].numero,
                           biblioteca[libro_idx].ejemplares[ejemplar_disponible].fecha);
        }
    }

    pthread_mutex_unlock(&bd_mutex);

    enviar_respuesta(sol->pid_solicitante, &resp);
}

// Función para guardar estado final
void guardar_estado_final() {
    if (!usar_archivo_salida) return;

    FILE *file = fopen(archivo_salida, "w");
    if (!file) {
        perror("Error creando archivo de salida");
        return;
    }

    fprintf(file, "=== ESTADO FINAL DE LA BIBLIOTECA ===\n");
    for (int i = 0; i < num_libros; i++) {
        fprintf(file, "\nLibro: %s (ISBN: %d)\n", biblioteca[i].nombre, biblioteca[i].isbn);
        fprintf(file, "Ejemplares totales: %d\n", biblioteca[i].num_ejemplares);

        int disponibles = 0;
        for (int j = 0; j < biblioteca[i].num_ejemplares; j++) {
            fprintf(file, "  Ejemplar %d: %s",
                   biblioteca[i].ejemplares[j].numero,
                   (biblioteca[i].ejemplares[j].status == STATUS_DISPONIBLE) ? "Disponible" : "Prestado");

            if (biblioteca[i].ejemplares[j].status == STATUS_PRESTADO) {
                fprintf(file, " (Fecha devolución: %s)", biblioteca[i].ejemplares[j].fecha);
            } else {
                disponibles++;
            }
            fprintf(file, "\n");
        }
        fprintf(file, "Ejemplares disponibles: %d\n", disponibles);
    }

    fclose(file);
    printf("Estado final guardado en: %s\n", archivo_salida);
}

int main(int argc, char *argv[]) {
    // Parsear argumentos
    if (argc < 5) {
        printf("Uso: %s -p pipeReceptor -f filedatos [-v] [-s filesalida]\n", argv[0]);
        exit(1);
    }

    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-p") == 0) {
            strcpy(pipe_name, argv[++i]);
        } else if (strcmp(argv[i], "-f") == 0) {
            strcpy(archivo_datos, argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose_mode = 1;
        } else if (strcmp(argv[i], "-s") == 0) {
            usar_archivo_salida = 1;
            strcpy(archivo_salida, argv[++i]);
        }
        i++;
    }

    // Validar argumentos requeridos
    if (strlen(pipe_name) == 0 || strlen(archivo_datos) == 0) {
        printf("Error: Debe especificar pipe (-p) y archivo de datos (-f)\n");
        exit(1);
    }

    // Inicializar estructuras
    init_buffer();

    // Cargar base de datos
    if (cargar_base_datos() != 0) {
        exit(1);
    }

    // Crear pipe nombrado
    if (mkfifo(pipe_name, 0666) == -1) {
        if (errno != EEXIST) {
            perror("Error creando pipe");
            exit(1);
        }
    }

    printf("Proceso receptor iniciado\n");
    if (verbose_mode) {
        printf("Modo verbose activado\n");
    }

    // Crear hilos auxiliares
    pthread_t hilo1, hilo2;
    pthread_create(&hilo1, NULL, hilo_auxiliar1, NULL);
    pthread_create(&hilo2, NULL, hilo_auxiliar2, NULL);

    // Procesar solicitudes
    int pipe_fd = open(pipe_name, O_RDONLY);
    if (pipe_fd == -1) {
        perror("Error abriendo pipe");
        exit(1);
    }

    solicitud_t sol;
    respuesta_t resp;

    while (!terminar_programa) {
        if (read(pipe_fd, &sol, sizeof(solicitud_t)) > 0) {
            imprimir_verbose("Solicitud recibida", &sol);

            switch (sol.operacion) {
                case OP_DEVOLVER:
                    resp.exito = 1;
                    strcpy(resp.mensaje, "Libro recibido para devolución");
                    enviar_respuesta(sol.pid_solicitante, &resp);
                    buffer_put(&sol); // Enviar al hilo auxiliar
                    break;

                case OP_RENOVAR:
			procesar_renovacion(&sol);
			break;

                case OP_PRESTAR:
                    procesar_prestamo(&sol);
                    break;

                case OP_SALIR:
                    printf("Proceso solicitante %d terminó\n", sol.pid_solicitante);
                    break;

                default:
                    printf("Operación desconocida: %c\n", sol.operacion);
                    break;
            }
        }
    }

    close(pipe_fd);

    // Esperar que terminen los hilos
    pthread_join(hilo1, NULL);
    pthread_join(hilo2, NULL);

    // Guardar estado final
    guardar_estado_final();

    // Limpiar
    unlink(pipe_name);
    pthread_mutex_destroy(&bd_mutex);
    pthread_mutex_destroy(&reporte_mutex);
    pthread_mutex_destroy(&buffer_renovaciones.mutex);
    pthread_cond_destroy(&buffer_renovaciones.not_full);
    pthread_cond_destroy(&buffer_renovaciones.not_empty);

    printf("Proceso receptor terminado\n");

    return 0;

}
// Fin del archivo procesoRecepcion.c
