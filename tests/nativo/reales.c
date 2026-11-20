/* ==========================================================================
 * tests/nativo/reales.c - Prueba de la conversión de reales a texto
 *
 * Imprime con print_float valores elegidos para cubrir cada camino de
 * fmt_float. La salida esperada (reales.out) es la de la versión anterior:
 * snprintf("%.15g") de glibc más ".0" cuando el texto no tiene '.', 'e'
 * ni 'n'. Se generó con tests/nativo/reales_esperado.c.
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "../../runtime.h"
#include "reales_valores.h"

int main(int argc, char **argv) {
    Asm a;
    Runtime rt;
    size_t i;
    int entry;

    if (argc != 2) { fprintf(stderr, "Usage: %s output\n", argv[0]); return 1; }
    asm_init(&a);
    rt_init(&a, &rt);
    entry = a.code.len;

    for (i = 0; i < sizeof valores / sizeof valores[0]; i++) {
        uint64_t bits;
        memcpy(&bits, &valores[i], sizeof bits);
        x_mov_ri(&a, RAX, bits);
        x_call(&a, rt.print_float);
        rt_print_text(&a, &rt, "\n");
    }

    x_mov_ri(&a, RDI, 0);
    x_call(&a, rt.exit);
    rt_emit(&a, &rt);
    asm_finish(&a);
    if (elf_write(argv[1], &a, entry) != 0) { perror(argv[1]); return 1; }
    return 0;
}
