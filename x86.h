/* ==========================================================================
 * x86.h - Codificador de instrucciones x86-64 (el "ensamblador" de vortexc)
 *
 * vortexc no usa un ensamblador externo: cada instrucción se convierte aquí
 * mismo en los bytes que la representan. Los bytes se acumulan en dos
 * buffers que después elf.c escribe en el archivo ejecutable:
 *
 *   code   instrucciones del programa         (segmento de lectura y ejecución)
 *   data   literales: cadenas, mensajes, ...   (segmento de lectura y escritura)
 *
 * Direcciones fijas en memoria (el ejecutable no es reubicable):
 *
 *   CODE_BASE  0x400000     donde el sistema carga el archivo; el código
 *                           empieza justo después de las cabeceras ELF
 *   DATA_BASE  0x10000000   donde se carga el segmento de datos
 *
 * Como DATA_BASE no depende del tamaño del código, la dirección de un dato
 * se conoce en el momento de crearlo y puede escribirse directamente en una
 * instrucción.
 * ========================================================================== */
#ifndef X86_H
#define X86_H

#include <stdint.h>

#define CODE_BASE  0x400000ULL
#define DATA_BASE  0x10000000ULL

/* ---- Buffer de bytes que crece según se necesite ---- */
typedef struct {
    unsigned char *bytes;
    int len, cap;
} Buf;

/* ---- Registros de propósito general de 64 bits ----
 * El número es el que usa la codificación de las instrucciones. Los
 * registros 8 a 15 necesitan un bit extra en el prefijo REX. */
typedef enum {
    RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI,
    R8, R9, R10, R11, R12, R13, R14, R15
} Reg;

/* ---- El programa que se está ensamblando ---- */
typedef struct {
    Buf code;
    Buf data;
} Asm;

void asm_init(Asm *a);

/* Agrega bytes al final de un buffer */
void buf_byte(Buf *b, int byte);
void buf_u32(Buf *b, uint32_t v);
void buf_u64(Buf *b, uint64_t v);
void buf_bytes(Buf *b, const void *src, int len);

/* Copia len bytes al segmento de datos y devuelve su dirección en memoria */
uint64_t asm_data(Asm *a, const void *src, int len);

/* ---- Instrucciones ---- */
void x_mov_ri(Asm *a, Reg dst, uint64_t imm);   /* mov dst, imm   */
void x_syscall(Asm *a);                         /* syscall        */

/* ---- elf.c ----
 * Escribe el ejecutable; entry es la posición de la primera instrucción
 * dentro del código. Devuelve 0 si todo salió bien. */
int elf_write(const char *path, const Asm *a, int entry);

/* Números de las llamadas al sistema de Linux x86-64 que usa el compilador */
#define SYS_WRITE  1
#define SYS_EXIT   60

#endif
