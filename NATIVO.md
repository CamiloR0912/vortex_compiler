# Backend nativo de vortexc

Este documento sigue la construcción del backend que reemplaza a GCC. El objetivo es que `vortexc` escriba por sí mismo un ejecutable ELF para Linux x86-64, sin compilador, ensamblador ni enlazador externos, y sin la biblioteca estándar de C en el programa generado.

El compilador se reconstruye por etapas: cada rama contiene solo lo que existe hasta esa etapa, de modo que cada avance se puede compilar y probar por separado. La versión anterior, que traducía a C y usaba GCC, está en la rama `main`; de ella se recuperan el analizador léxico, el sintáctico y el semántico cuando una etapa los necesita, y sus 33 programas de prueba son la referencia de la salida correcta.

## Etapas

| Etapa | Rama | Alcance | Estado |
|---|---|---|---|
| 1 | `etapa-1/ejecutable-minimo` | Codificador de instrucciones y escritor ELF | Terminada |
| 2 | `etapa-2/enteros-y-print` | Enteros, booleanos, variables, aritmética y `print` | Pendiente |
| 3 | `etapa-3/control-de-flujo` | `if`, `while`, `for`, `switch`, bloques y shadowing | Pendiente |
| 4 | `etapa-4/funciones` | Funciones, recursión, sobrecarga y funciones anidadas | Pendiente |
| 5 | `etapa-5/cadenas-y-reales` | `string` y `float` | Pendiente |
| 6 | `etapa-6/excepciones` | `try`-`catch`, `throw`, `assert` y errores de ejecución | Pendiente |
| 7 | `etapa-7/vectores-y-matrices` | Arreglos, operaciones elemento a elemento y reducciones | Pendiente |

## Etapa 1: ejecutable mínimo

### Archivos

| Archivo | Hace el trabajo de | Contenido |
|---|---|---|
| `x86.h` | | Registros, condiciones, la estructura `Asm` y la interfaz del codificador |
| `x86enc.c` | Ensamblador | Convierte cada instrucción en bytes y resuelve las etiquetas |
| `elf.c` | Enlazador | Escribe la cabecera ELF, las cabeceras de programa y los segmentos |
| `tests/nativo/` | | Generadores de prueba y `run.sh` |

### Cómo se codifica una instrucción

Una instrucción x86-64 tiene la forma `[REX] opcode [ModR/M] [SIB] [desplazamiento] [inmediato]`. Ejemplos tomados de los ejecutables de prueba (desensamblados con `objdump`):

| Instrucción | Bytes | Explicación |
|---|---|---|
| `mov eax, 1` | `b8 01 00 00 00` | `B8 + 0` (rax es el registro 0) y el valor en 32 bits little endian |
| `mov esi, 0x10000000` | `be 00 00 00 10` | `B8 + 6` (rsi) y la dirección del mensaje en el segmento de datos |
| `syscall` | `0f 05` | |
| `dec rbx` | `48 ff cb` | REX.W (64 bits), opcode `FF`, ModR/M `11 001 011`: registro, operación 1 (dec), rbx |
| `jne ciclo` | `0f 85 e8 ff ff ff` | Salto condicional; `0xffffffe8` = -24 bytes, hacia atrás |
| `call imprimir` | `e8 29 00 00 00` | Llamada 41 bytes hacia adelante |

Escribir `mov eax, 1` en lugar de `mov rax, 1` es correcto: al escribir un registro de 32 bits, el procesador pone en cero los 32 bits altos, y la instrucción ocupa 5 bytes en lugar de 10.

### Etiquetas y relleno posterior

Un salto hacia una etiqueta que todavía no tiene posición (por ejemplo, la llamada a una rutina definida más abajo) se emite con un desplazamiento provisional de cero, y su posición queda anotada en la lista `fixups`. Al terminar, `asm_finish` recorre la lista y escribe en cada salto el desplazamiento real:

```
rel32 = posición de la etiqueta - (posición del campo rel32 + 4)
```

El desplazamiento se mide desde el final de la instrucción, que es donde termina el campo de 4 bytes.

### Estructura del ejecutable

```
desplazamiento 0      cabecera ELF (64 bytes)
               64     2 cabeceras de programa (56 bytes cada una)
               176    código               cargado en 0x4000B0: punto de entrada
               ...    relleno hasta 4096
               4096   datos                cargados en 0x10000000
```

El segmento de código tiene permisos de lectura y ejecución y el de datos de lectura y escritura. Como el segmento de datos siempre empieza en `0x10000000`, la dirección de un dato se conoce en cuanto se crea y se escribe directamente en la instrucción que lo usa, sin un paso de reubicación.

### Pruebas

Desde WSL, en la carpeta del proyecto:

```
bash run
```

| Prueba | Qué demuestra | Salida esperada |
|---|---|---|
| `hola.c` | Un ejecutable escrito sin GCC que el núcleo carga y ejecuta | Un mensaje y código de salida 0 |
| `saltos.c` | Saltos hacia atrás (ciclo), hacia adelante (`jmp` que omite código) y `call`/`ret` | Tres líneas y código de salida 7 |

`file` reporta los ejecutables generados como `ELF 64-bit LSB executable, x86-64, version 1 (SYSV), statically linked, no section header`.

Los generadores de prueba se compilan con GCC porque son programas en C que usan el codificador, igual que `vortexc`. Los ejecutables que producen no pasan por GCC.
