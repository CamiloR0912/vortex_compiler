/* ==========================================================================
 * tests/nativo/error.c - Prueba de la rutina de error de ejecución
 *
 * El programa generado imprime una línea, reporta un error en la línea 42
 * y termina con código 1. El texto que había en el buffer debe aparecer
 * antes del error, y lo que sigue al error no debe ejecutarse.
 * ========================================================================== */
#include <stdio.h>
#include "../../runtime.h"

int main(int argc, char **argv) {
    Asm a;
    Runtime rt;
    int entry;

    if (argc != 2) { fprintf(stderr, "Usage: %s output\n", argv[0]); return 1; }
    asm_init(&a);
    rt_init(&a, &rt);

    entry = a.code.len;
    rt_print_text(&a, &rt, "antes del error\n");
    rt_fail(&a, &rt, 42, "division by zero");
    rt_print_text(&a, &rt, "esto no debe imprimirse\n");
    x_mov_ri(&a, RDI, 0);
    x_call(&a, rt.exit);

    rt_emit(&a, &rt);
    asm_finish(&a);
    if (elf_write(argv[1], &a, entry) != 0) { perror(argv[1]); return 1; }
    return 0;
}
