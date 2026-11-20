/* ==========================================================================
 * runtime.h - Rutinas de ejecución que vortexc incluye en cada ejecutable
 *
 * El programa generado no puede usar la biblioteca de C (printf, exit...).
 * Lo que necesita en tiempo de ejecución se genera como código máquina con
 * el mismo codificador, y el código del programa lo invoca con call.
 *
 * Convención de llamada de estas rutinas:
 *   - Los argumentos van en los registros indicados en cada una.
 *   - Pueden modificar rax, rcx, rdx, rsi, rdi, r8, r9, r10 y r11.
 *   - Conservan rbx, rbp, rsp y r12..r15.
 * El generador de código guarda sus valores en la pila o en el marco de la
 * función, así que no pierde nada al llamarlas.
 *
 * Salida con buffer: write es una llamada al sistema, que es costosa. Los
 * textos se acumulan en un buffer de 4096 bytes y se escriben de una sola
 * vez cuando el buffer se llena o cuando el programa termina.
 *
 * Cadenas: un string de Vortex es la dirección de sus bytes, terminados en
 * un byte 0 (como en C). Los literales están en el segmento de datos; las
 * cadenas creadas al ejecutar (concatenación, conversión a texto) se
 * reservan en el heap.
 *
 * Heap: alloc entrega memoria de un bloque obtenido con la llamada al
 * sistema mmap, avanzando un puntero; cuando el bloque se acaba pide otro.
 * La memoria no se libera: el sistema la recupera al terminar el programa,
 * igual que en la versión anterior.
 * ========================================================================== */
#ifndef RUNTIME_H
#define RUNTIME_H

#include "x86.h"

#define OUT_BUF_SIZE 4096
#define HEAP_CHUNK   (1 << 20)      /* tamaño mínimo de cada bloque pedido con mmap */
#define MAX_TRY      2048           /* bloques try activos al mismo tiempo, como antes */

/* Clases de excepción: deciden la etiqueta de un error no capturado */
#define EXC_RUNTIME  0              /* [Runtime error]: división entre cero, índices... */
#define EXC_ASSERT   1              /* [Assertion error]: assert que falla               */
#define EXC_THROW    2              /* [Uncaught exception]: throw                       */

/* Vectores y matrices (runtime_arrays.c) */
#define KIND_INT     0              /* clase de los elementos de un arreglo */
#define KIND_FLOAT   1
#define KIND_BOOL    2
#define KIND_STRING  3
#define ARR_ADD      0              /* operación elemento a elemento */
#define ARR_SUB      1
#define ARR_MUL      2
#define ARR_DIV      3
#define ARR_MOD      4
#define RED_SUM      0              /* reducción */
#define RED_PROD     1
#define RED_MAX      2
#define RED_MIN      3

typedef struct {
    Label write;        /* agrega rdx bytes desde la dirección rsi a la salida */
    Label write_all;    /* write(1, rsi, rdx) repetido hasta escribir todo     */
    Label flush;        /* vacía el buffer de salida                           */
    Label fmt_int;      /* escribe rax en decimal en un espacio temporal;
                           devuelve rsi = dirección, rdx = largo               */
    Label print_int;    /* imprime rax como entero decimal con signo           */
    Label print_bool;   /* imprime "true" si rax != 0, si no "false"           */
    Label print_str;    /* imprime la cadena rax                               */
    Label alloc;        /* reserva rdi bytes en el heap; rax = dirección       */
    Label strlen;       /* rdx = largo de la cadena rsi (conserva rsi)         */
    Label copy_str;     /* rax = nueva cadena con los rdx bytes desde rsi      */
    Label str_int;      /* rax = cadena con el entero rax                      */
    Label str_bool;     /* rax = "true" o "false" según rax                    */
    Label concat;       /* rax = nueva cadena: cadena rax seguida de cadena rcx */
    Label strcmp;       /* rax = -1, 0 o 1 al comparar las cadenas rax y rcx   */
    Label fmt_float;    /* escribe el double con bits rax como "%.15g" (más
                           ".0" si hace falta); rsi = dirección, rdx = largo   */
    Label print_float;  /* imprime el double con bits rax                      */
    Label str_float;    /* rax = cadena con el double con bits rax             */
    Label exit;         /* vacía el buffer y termina con código rdi            */
    Label raise;        /* lanza una excepción: rdi = clase, rax = línea,
                           rsi = mensaje; va al catch activo o, si no hay,
                           imprime el error y termina con código 1         */
    uint64_t out_buf;   /* dirección del buffer de salida (bss)                */
    uint64_t out_len;   /* dirección del contador de bytes en el buffer (bss)  */
    uint64_t depth;     /* dirección del contador de llamadas en curso (bss)   */
    uint64_t try_top;   /* cantidad de bloques try activos (bss)               */
    uint64_t handlers;  /* pila de manejadores de try, 32 bytes cada uno (bss) */
    /* Vectores y matrices: ver runtime_arrays.c */
    Label arr_new, arr_total, arr_copy, arr_to_float, mat_from_rows;
    Label vec_addr, vec_get, mat_row, mat_addr2, mat_set_row;
    Label elem_apply, arr_arr, arr_scalar, arr_neg, arr_eq, reduce;
    Label print_items, print_arr;
    uint64_t num_buf;   /* espacio temporal para escribir números (bss)        */
    uint64_t heap_ptr;  /* siguiente byte libre del bloque actual (bss)        */
    uint64_t heap_end;  /* fin del bloque actual (bss)                         */
    uint64_t text_true, text_false;     /* "true" y "false", terminadas en 0  */
    uint64_t pow10;     /* tabla de doubles 10^-324 ... 10^308 (datos)         */
} Runtime;

/* Crea las etiquetas y reserva la memoria; las rutinas pueden llamarse
 * desde ese momento aunque su código se emita después. */
void rt_init(Asm *a, Runtime *rt);

/* Emite el código de todas las rutinas en la posición actual */
void rt_emit(Asm *a, Runtime *rt);

/* Emite el código que imprime un texto fijo (se guarda en los datos) */
void rt_print_text(Asm *a, Runtime *rt, const char *text);

/* Emite las rutinas de vectores y matrices (runtime_arrays.c); la llama
 * rt_emit */
void rt_emit_arrays(Asm *a, Runtime *rt);

/* Guarda un texto en los datos, terminado en 0, y devuelve su dirección */
uint64_t rt_literal(Asm *a, const char *text, int len);

/* Emite el código que lanza un error de ejecución (EXC_RUNTIME) con el
 * mensaje y la línea del programa fuente indicados */
void rt_fail(Asm *a, Runtime *rt, int line, const char *msg);

#endif
