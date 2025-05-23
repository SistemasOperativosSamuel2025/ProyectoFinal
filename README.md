# Sistema de Préstamo de Libros

Este proyecto implementa un sistema de préstamo de libros usando procesos e hilos en C con la biblioteca POSIX.

## Descripción

El sistema consta de dos componentes principales:

1. **Proceso Solicitante (PS)**: Genera solicitudes de préstamo, renovación y devolución de libros
2. **Proceso Receptor (RP)**: Procesa las solicitudes y maneja la base de datos de libros

## Compilación

```bash
make
```

Para limpiar archivos compilados:
```bash
make clean
```

## Uso

### Proceso Receptor

```bash
./receptor -p pipeReceptor -f filedatos [-v] [-s filesalida]
```

Parámetros:
- `-p pipeReceptor`: Nombre del pipe para comunicación
- `-f filedatos`: Archivo con la base de datos inicial de libros
- `-v`: Modo verbose (opcional)
- `-s filesalida`: Archivo de salida para el estado final (opcional)

Ejemplo:
```bash
./receptor -p /tmp/biblioteca_pipe -f libros.txt -v -s estado_final.txt
```

### Proceso Solicitante

```bash
./solicitante [-i archivo] -p pipeReceptor
```

Parámetros:
- `-i archivo`: Archivo con solicitudes (opcional, si no se especifica usa menú interactivo)
- `-p pipeReceptor`: Nombre del pipe para comunicación

Ejemplos:
```bash
# Modo interactivo
./solicitante -p /tmp/biblioteca_pipe

# Con archivo de solicitudes
./solicitante -i solicitudes.txt -p /tmp/biblioteca_pipe
```

## Formato de Archivos

### Base de Datos (libros.txt)

```
nombre del libro, ISBN, numero ejemplares
ejemplar1, status, fecha
ejemplar2, status, fecha
...
```

Ejemplo:
```
Operating Systems, 2233, 4
1, D, 01-03-2025
2, D, 01-03-2025
3, P, 01-03-2025
4, P, 01-03-2025
```

### Archivo de Solicitudes

```
Operación, nombre del libro, ISBN
```

Operaciones:
- `D`: Devolver libro
- `R`: Renovar libro
- `P`: Prestar libro
- `Q`: Salir

Ejemplo:
```
P, Operating Systems, 2233
R, Data Bases, 2234
D, Programming Languages, 2240
Q, Salir, 0
```

## Comandos del Receptor

Mientras el receptor está ejecutándose, acepta los siguientes comandos:

- `s`: Terminar el programa
- `r`: Generar reporte de operaciones

## Funcionalidades Implementadas

### Proceso Solicitante
- [x] Lectura de solicitudes desde archivo
- [x] Menú interactivo para solicitudes
- [x] Comunicación con receptor via pipes nombrados
- [x] Manejo de respuestas del receptor
- [x] Comando de terminación (Q)

### Proceso Receptor
- [x] Recepción de solicitudes via pipe nombrado
- [x] Procesamiento inmediato de devoluciones y renovaciones
- [x] Verificación de disponibilidad para préstamos
- [x] Hilo auxiliar 1 para actualización de BD (productor-consumidor)
- [x] Hilo auxiliar 2 para comandos de consola
- [x] Modo verbose para mostrar operaciones
- [x] Generación de reportes
- [x] Guardado de estado final
- [x] Sincronización con mutex y variables de condición

### Características Adicionales
- [x] Manejo concurrente de múltiples solicitantes
- [x] Sincronización thread-safe de la base de datos
- [x] Buffer circular para operaciones de renovación/devolución
- [x] Manejo de fechas (renovación por 7 días)
- [x] Validación de ISBN y disponibilidad de libros
- [x] Reportes detallados de operaciones

## Arquitectura

### Comunicación entre Procesos
- **Pipe principal**: `/tmp/biblioteca_pipe` para solicitudes PS → RP
- **Pipes de respuesta**: `/tmp/resp_{PID}` para respuestas RP → PS

### Hilos del Proceso Receptor
1. **Hilo principal**: Recibe solicitudes y procesa préstamos
2. **Hilo auxiliar 1**: Procesa devoluciones y renovaciones (consumidor)
3. **Hilo auxiliar 2**: Maneja comandos de consola

### Sincronización
- Mutex para proteger la base de datos (`bd_mutex`)
- Mutex para proteger el array de reportes (`reporte_mutex`)
- Buffer circular con mutex y variables de condición para productor-consumidor

## Pruebas

Ejecutar el script de prueba:
```bash
chmod +x test.sh
./test.sh
```

## Archivos del Proyecto

- `common.h`: Definiciones y estructuras compartidas
- `solicitante.c`: Implementación del proceso solicitante
- `receptor.c`: Implementación del proceso receptor
- `Makefile`: Archivo de compilación
- `libros.txt`: Ejemplo de base de datos inicial
- `solicitudes.txt`: Ejemplo de archivo de solicitudes
- `test.sh`: Script de prueba automatizada

## Limitaciones Conocidas

1. El sistema asume que los archivos de entrada están bien formateados
2. La gestión de fechas es simplificada (no maneja años bisiestos completamente)
3. El tamaño máximo del buffer circular está limitado a 10 elementos
4. Los ISBN pueden ser más cortos que los reales para simplificar

## Notas de Implementación

- Se utilizan pipes nombrados (FIFOs) para la comunicación entre procesos
- La sincronización se implementa con mutexes y variables de condición POSIX
- El patrón productor-consumidor se usa para las operaciones de renovación/devolución
- Se implementa manejo de señales para terminación limpia de procesos
