/* ==========================================================================
 * tests/nativo/saltos.c - Prueba de etiquetas, saltos y llamadas
 *
 * Genera un ejecutable equivalente a:
 *
 *     _start:
 *         mov rbx, 3
 *     ciclo:                      ; etiqueta usada hacia atrás
 *         mov rsi, linea
 *         mov rdx, largo_linea
 *         call imprimir           ; imprimir está más abajo: salto hacia adelante
 *         dec rbx
 *         jne ciclo               ; repetir mientras rbx != 0
 *         jmp fin                 ; saltar el mensaje que no debe verse
 *         mov rsi, oculto
 *         mov rdx, largo_oculto
 *         call imprimir
 *     fin:
 *         mov rax, 60             ; exit(7)
 *         mov rdi, 7
 *         syscall
 *
 *     imprimir:                   ; write(1, rsi, rdx)
 *         mov rax, 1
 *         mov rdi, 1
 *         syscall
 *         ret
 *
 * rbx sirve de contador porque syscall no lo modifica (sí modifica rcx y r11).
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "../../x86.h"

int main(int argc, char **argv) {
    static const char linea[] = "una vuelta del ciclo\n";
    static const char oculto[] = "esto no debe imprimirse\n";
    Asm a;
    uint64_t linea_addr, oculto_addr;
    Label ciclo, fin, imprimir;
    int entry;

    if (argc != 2) { fprintf(stderr, "Usage: %s output\n", argv[0]); return 1; }
    asm_init(&a);
    linea_addr = asm_data(&a, linea, (int)strlen(linea));
    oculto_addr = asm_data(&a, oculto, (int)strlen(oculto));
    ciclo = asm_label(&a);
    fin = asm_label(&a);
    imprimir = asm_label(&a);

    entry = a.code.len;
    x_mov_ri(&a, RBX, 3);
    asm_bind(&a, ciclo);
    x_mov_ri(&a, RSI, linea_addr);
    x_mov_ri(&a, RDX, strlen(linea));
    x_call(&a, imprimir);
    x_dec(&a, RBX);
    x_jcc(&a, CC_NE, ciclo);
    x_jmp(&a, fin);
    x_mov_ri(&a, RSI, oculto_addr);
    x_mov_ri(&a, RDX, strlen(oculto));
    x_call(&a, imprimir);
    asm_bind(&a, fin);
    x_mov_ri(&a, RAX, SYS_EXIT);
    x_mov_ri(&a, RDI, 7);
    x_syscall(&a);

    asm_bind(&a, imprimir);
    x_mov_ri(&a, RAX, SYS_WRITE);
    x_mov_ri(&a, RDI, 1);
    x_syscall(&a);
    x_ret(&a);

    asm_finish(&a);
    if (elf_write(argv[1], &a, entry) != 0) { perror(argv[1]); return 1; }
    return 0;
}
