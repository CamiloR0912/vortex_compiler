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
 * ========================================================================== */
#ifndef RUNTIME_H
#define RUNTIME_H

#include "x86.h"

#define OUT_BUF_SIZE 4096

typedef struct {
    Label write;        /* agrega rdx bytes desde la dirección rsi a la salida */
    Label write_all;    /* write(1, rsi, rdx) repetido hasta escribir todo     */
    Label flush;        /* vacía el buffer de salida                           */
    Label print_int;    /* imprime rax como entero decimal con signo           */
    Label print_bool;   /* imprime "true" si rax != 0, si no "false"           */
    Label exit;         /* vacía el buffer y termina con código rdi            */
    Label error;        /* imprime "[Runtime error] line rax: " y el mensaje
                           (rsi = dirección, rdx = largo); termina con código 1 */
    uint64_t out_buf;   /* dirección del buffer de salida (bss)                */
    uint64_t out_len;   /* dirección del contador de bytes en el buffer (bss)  */
    uint64_t depth;     /* dirección del contador de llamadas en curso (bss)   */
    uint64_t text_true, text_false;
} Runtime;

/* Crea las etiquetas y reserva la memoria; las rutinas pueden llamarse
 * desde ese momento aunque su código se emita después. */
void rt_init(Asm *a, Runtime *rt);

/* Emite el código de todas las rutinas en la posición actual */
void rt_emit(Asm *a, Runtime *rt);

/* Emite el código que imprime un texto fijo (se guarda en los datos) */
void rt_print_text(Asm *a, Runtime *rt, const char *text);

/* Emite el código que reporta un error de ejecución en la línea indicada
 * del programa fuente y termina */
void rt_fail(Asm *a, Runtime *rt, int line, const char *msg);

#endif
