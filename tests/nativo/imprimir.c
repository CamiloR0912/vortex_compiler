/* ==========================================================================
 * tests/nativo/imprimir.c - Prueba de las rutinas de ejecución (runtime.c)
 *
 * Genera un programa que:
 *   1. imprime enteros, incluidos 0, negativos y los dos extremos de 64 bits;
 *   2. imprime true y false;
 *   3. imprime los números de 1500 a 1 en una sola línea (unos 6400 bytes),
 *      más que el buffer de 4096: obliga a vaciarlo a mitad de camino;
 *   4. termina con exit(3) dejando texto en el buffer: exit debe vaciarlo
 *      antes de terminar.
 * ========================================================================== */
#include <stdio.h>
#include "../../runtime.h"

static Asm a;
static Runtime rt;

static void print_int_line(const char *label, int64_t v) {
    rt_print_text(&a, &rt, label);
    x_mov_ri(&a, RAX, (uint64_t)v);
    x_call(&a, rt.print_int);
    rt_print_text(&a, &rt, "\n");
}

int main(int argc, char **argv) {
    Label ciclo;
    int entry;

    if (argc != 2) { fprintf(stderr, "Usage: %s output\n", argv[0]); return 1; }
    asm_init(&a);
    rt_init(&a, &rt);
    ciclo = asm_label(&a);

    entry = a.code.len;

    /* 1. Enteros */
    print_int_line("cero: ", 0);
    print_int_line("siete: ", 7);
    print_int_line("menos siete: ", -7);
    print_int_line("grande: ", 1234567890123LL);
    print_int_line("maximo: ", INT64_MAX);
    print_int_line("minimo: ", INT64_MIN);

    /* 2. Booleanos */
    rt_print_text(&a, &rt, "verdadero: ");
    x_mov_ri(&a, RAX, 1);
    x_call(&a, rt.print_bool);
    rt_print_text(&a, &rt, "\nfalso: ");
    x_mov_ri(&a, RAX, 0);
    x_call(&a, rt.print_bool);
    rt_print_text(&a, &rt, "\n");

    /* 3. Más texto del que cabe en el buffer. rbx es el contador: las
     *    rutinas lo conservan. */
    x_mov_ri(&a, RBX, 1500);
    asm_bind(&a, ciclo);
    x_mov_rr(&a, RAX, RBX);
    x_call(&a, rt.print_int);
    rt_print_text(&a, &rt, " ");
    x_dec(&a, RBX);
    x_jcc(&a, CC_NE, ciclo);
    rt_print_text(&a, &rt, "\n");

    /* 4. Texto pendiente en el buffer y exit(3) */
    rt_print_text(&a, &rt, "fin\n");
    x_mov_ri(&a, RDI, 3);
    x_call(&a, rt.exit);

    rt_emit(&a, &rt);
    asm_finish(&a);
    if (elf_write(argv[1], &a, entry) != 0) { perror(argv[1]); return 1; }
    return 0;
}
