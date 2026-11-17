/* ==========================================================================
 * tests/nativo/hola.c - Prueba del backend nativo, etapa 1
 *
 * Usa el codificador (x86enc.c) y el escritor ELF (elf.c) para producir,
 * sin gcc ni ensamblador, un ejecutable equivalente a este programa:
 *
 *     _start:
 *         mov rax, 1          ; write(1, mensaje, largo)
 *         mov rdi, 1
 *         mov rsi, mensaje
 *         mov rdx, largo
 *         syscall
 *         mov rax, 60         ; exit(0)
 *         mov rdi, 0
 *         syscall
 *
 *     ./hola_gen salida      escribe el ejecutable en el archivo salida
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "../../x86.h"

int main(int argc, char **argv) {
    static const char msg[] = "Hola desde un ejecutable generado sin gcc\n";
    Asm a;
    uint64_t msg_addr;
    int entry;

    if (argc != 2) { fprintf(stderr, "Usage: %s output\n", argv[0]); return 1; }
    asm_init(&a);
    msg_addr = asm_data(&a, msg, (int)strlen(msg));

    entry = a.code.len;
    x_mov_ri(&a, RAX, SYS_WRITE);
    x_mov_ri(&a, RDI, 1);                   /* 1 = salida estándar */
    x_mov_ri(&a, RSI, msg_addr);
    x_mov_ri(&a, RDX, strlen(msg));
    x_syscall(&a);
    x_mov_ri(&a, RAX, SYS_EXIT);
    x_mov_ri(&a, RDI, 0);                   /* código de salida */
    x_syscall(&a);

    if (elf_write(argv[1], &a, entry) != 0) { perror(argv[1]); return 1; }
    return 0;
}
