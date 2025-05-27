/*
 * =============================================================================
 * Proyecto: Sistema de Préstamo de Libros - Proceso Solicitante
 * Archivo: solicitante.c
 * Autor: Samuel Emperador
 * Fecha: 23/05/2025
 * Descripción: Implementa el proceso solicitante que puede enviar solicitudes
 *              de préstamo, devolución y renovación de libros a través de pipes
 *              nombrados. Soporta modo interactivo y procesamiento por lotes.
 * =============================================================================
 */

#include "estructuras.h"

// Variables globales
char pipe_name[MAX_STRING];
char input_file[MAX_STRING];
int usar_archivo = 0;
int pipe_fd;

// Función para mostrar el menú
void mostrar_menu() {
    printf("\n=== SISTEMA DE PRÉSTAMO DE LIBROS ===\n");
    printf("1. Devolver libro (D)\n");
    printf("2. Renovar libro (R)\n");
    printf("3. Solicitar préstamo (P)\n");
    printf("4. Salir (Q)\n");
    printf("Seleccione una opción: ");
}

// Función para obtener operación del menú
operation_t obtener_operacion_menu() {
    int opcion;
    scanf("%d", &opcion);

    switch(opcion) {
        case 1: return OP_DEVOLVER;
        case 2: return OP_RENOVAR;
        case 3: return OP_PRESTAR;
        case 4: return OP_SALIR;
        default:
            printf("Opción inválida\n");
            return obtener_operacion_menu();
    }
}

// Función para enviar solicitud
int enviar_solicitud(solicitud_t *sol) {
    sol->pid_solicitante = getpid();

    if (write(pipe_fd, sol, sizeof(solicitud_t)) == -1) {
        perror("Error escribiendo en pipe");
        return -1;
    }

    return 0;
}

// Función para recibir respuesta
int recibir_respuesta(respuesta_t *resp) {
    char pipe_respuesta[MAX_STRING];
    snprintf(pipe_respuesta, sizeof(pipe_respuesta), "/tmp/resp_%d", getpid());

    // Crear pipe nombrado para respuesta
    if (mkfifo(pipe_respuesta, 0666) == -1) {
        if (errno != EEXIST) {
            perror("Error creando pipe de respuesta");
            return -1;
        }
    }

    int resp_fd = open(pipe_respuesta, O_RDONLY);
    if (resp_fd == -1) {
        perror("Error abriendo pipe de respuesta");
        return -1;
    }

    if (read(resp_fd, resp, sizeof(respuesta_t)) == -1) {
        perror("Error leyendo respuesta");
        close(resp_fd);
        return -1;
    }

    close(resp_fd);
    unlink(pipe_respuesta);

}

// Función para procesar archivo de entrada
// Función corregida para procesar archivo de entrada
void procesar_archivo() {
    FILE *file = fopen(input_file, "r");
    if (!file) {
        perror("Error abriendo archivo de entrada");
        exit(1);
    }

    char linea[MAX_LINE];
    while (fgets(linea, sizeof(linea), file)) {
        // Remover salto de línea
        linea[strcspn(linea, "\n")] = 0;
        
        // Ignorar líneas vacías
        if (strlen(linea) == 0) {
            continue;
        }

        // Parsear línea con formato: "OPERACION, NOMBRE_LIBRO, ISBN"
        char op_char;
        char nombre[MAX_STRING];
        int isbn;

        // Usar un parsing más robusto
        char *token1 = strtok(linea, ",");
        char *token2 = strtok(NULL, ",");
        char *token3 = strtok(NULL, ",");
        
        if (token1 == NULL || token2 == NULL || token3 == NULL) {
            printf("Error parseando línea: %s\n", linea);
            continue;
        }
        
        // Limpiar espacios en blanco
        while (*token1 == ' ') token1++;  // Quitar espacios del inicio
        while (*token2 == ' ') token2++;
        while (*token3 == ' ') token3++;
        
        op_char = token1[0];
        strcpy(nombre, token2);
        isbn = atoi(token3);

        // Crear solicitud
        solicitud_t sol;
        
        // Convertir carácter a operation_t
        switch(op_char) {
            case 'P':
                sol.operacion = OP_PRESTAR;
                break;
            case 'R':
                sol.operacion = OP_RENOVAR;
                break;
            case 'D':
                sol.operacion = OP_DEVOLVER;
                break;
            case 'Q':
                sol.operacion = OP_SALIR;
                break;
            default:
                printf("Operación desconocida: %c\n", op_char);
                continue;
        }
        
        strcpy(sol.nombre_libro, nombre);
        sol.isbn = isbn;

        // Si es comando de salir
        if (sol.operacion == OP_SALIR) {
            printf("Comando de salir detectado\n");
            enviar_solicitud(&sol);
            break;
        }

        // Enviar solicitud
        printf("Enviando: %c, %s, %d\n", op_char, nombre, isbn);
        if (enviar_solicitud(&sol) == 0) {
            respuesta_t resp;
            if (recibir_respuesta(&resp) == 0) {
                printf("Respuesta: %s\n", resp.mensaje);
                if (sol.operacion == OP_RENOVAR && resp.exito) {
                    printf("Nueva fecha de devolución: %s\n", resp.fecha_devolucion);
                }
            } else {
                printf("Error recibiendo respuesta\n");
            }
        } else {
            printf("Error enviando solicitud\n");
        }
        
        // Pequeña pausa para evitar saturar el sistema
        usleep(100000); // 100ms
    }

    fclose(file);
}

// Función para procesar menú interactivo
void procesar_menu() {
    solicitud_t sol;
    respuesta_t resp;

    while (1) {
        mostrar_menu();
        sol.operacion = obtener_operacion_menu();

        if (sol.operacion == OP_SALIR) {
            strcpy(sol.nombre_libro, "Salir");
            sol.isbn = 0;
            enviar_solicitud(&sol);
            printf("Saliendo del sistema...\n");
            break;
        }

        printf("Ingrese el nombre del libro: ");
        getchar(); // consumir newline
        fgets(sol.nombre_libro, sizeof(sol.nombre_libro), stdin);
        sol.nombre_libro[strcspn(sol.nombre_libro, "\n")] = 0;

        printf("Ingrese el ISBN: ");
        scanf("%d", &sol.isbn);

        // Enviar solicitud
        if (enviar_solicitud(&sol) == 0) {
            if (recibir_respuesta(&resp) == 0) {
                printf("\nRespuesta del sistema: %s\n", resp.mensaje);
                if (sol.operacion == OP_RENOVAR && resp.exito) {
                    printf("Nueva fecha de devolución: %s\n", resp.fecha_devolucion);
                }
            }
        }
    }
}

// Función para manejar señales
void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\nCerrando proceso solicitante...\n");
        if (pipe_fd > 0) {
            close(pipe_fd);
        }
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    // Configurar manejo de señales
    signal(SIGINT, signal_handler);

    // Parsear argumentos
    if (argc < 3) {
        printf("Uso: %s [-i archivo] -p pipeReceptor\n", argv[0]);
        exit(1);
    }

    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-i") == 0) {
            usar_archivo = 1;
            strcpy(input_file, argv[++i]);
        } else if (strcmp(argv[i], "-p") == 0) {
            strcpy(pipe_name, argv[++i]);
        }
        i++;
    }

    if (strlen(pipe_name) == 0) {
        printf("Error: Debe especificar el pipe receptor con -p\n");
        exit(1);
    }

    // Abrir pipe para comunicación
    pipe_fd = open(pipe_name, O_WRONLY);
    if (pipe_fd == -1) {
        perror("Error abriendo pipe");
        exit(1);
    }

    printf("Proceso solicitante iniciado (PID: %d)\n", getpid());

    if (usar_archivo) {
        printf("Procesando archivo: %s\n", input_file);
        procesar_archivo();
    } else {
        printf("Modo interactivo\n");
        procesar_menu();
    }

    close(pipe_fd);
    printf("Proceso solicitante terminado\n");

    return 0;
}
