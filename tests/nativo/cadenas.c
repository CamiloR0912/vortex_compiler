/* ==========================================================================
 * tests/nativo/cadenas.c - Prueba del heap y de las rutinas de cadenas
 *
 * El programa generado:
 *   1. imprime literales y concatenaciones, incluidas conversiones de
 *      enteros y booleanos a cadena;
 *   2. compara cadenas con strcmp (incluido un byte mayor que 127);
 *   3. construye una cadena de 3000 caracteres concatenando de a dos: cada
 *      concatenación reserva una cadena nueva, unos 2.3 MB en total, así
 *      que el heap necesita varios bloques de mmap;
 *   4. reserva un bloque de 3 MB, mayor que HEAP_CHUNK, y escribe y lee su
 *      último byte.
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "../../runtime.h"

static Asm a;
static Runtime rt;

static uint64_t lit(const char *s) { return rt_literal(&a, s, (int)strlen(s)); }

static void newline(void) { rt_print_text(&a, &rt, "\n"); }

/* print(strcmp(x, y)) */
static void compare(const char *x, const char *y) {
    x_mov_ri(&a, RAX, lit(x));
    x_mov_ri(&a, RCX, lit(y));
    x_call(&a, rt.strcmp);
    x_call(&a, rt.print_int);
    newline();
}

int main(int argc, char **argv) {
    Label ciclo;
    int entry;

    if (argc != 2) { fprintf(stderr, "Usage: %s output\n", argv[0]); return 1; }
    asm_init(&a);
    rt_init(&a, &rt);
    ciclo = asm_label(&a);
    entry = a.code.len;

    /* 1. Literales y concatenación */
    x_mov_ri(&a, RAX, lit("hola"));
    x_call(&a, rt.print_str);
    newline();

    x_mov_ri(&a, RAX, lit("Hola"));
    x_mov_ri(&a, RCX, lit(", mundo"));
    x_call(&a, rt.concat);
    x_call(&a, rt.print_str);
    newline();

    x_mov_ri(&a, RAX, (uint64_t)-1234);         /* "n = " + -1234 */
    x_call(&a, rt.str_int);
    x_mov_rr(&a, RCX, RAX);
    x_mov_ri(&a, RAX, lit("n = "));
    x_call(&a, rt.concat);
    x_call(&a, rt.print_str);
    newline();

    x_mov_ri(&a, RAX, 0);                       /* false + "!" */
    x_call(&a, rt.str_bool);
    x_mov_ri(&a, RCX, lit("!"));
    x_call(&a, rt.concat);
    x_call(&a, rt.print_str);
    newline();

    x_mov_ri(&a, RAX, lit(""));                 /* "" + "" */
    x_mov_ri(&a, RCX, lit(""));
    x_call(&a, rt.concat);
    x_mov_rr(&a, RSI, RAX);
    x_call(&a, rt.strlen);
    x_mov_rr(&a, RAX, RDX);
    x_call(&a, rt.print_int);
    newline();

    /* 2. Comparaciones */
    compare("abc", "abd");
    compare("abd", "abc");
    compare("abc", "abc");
    compare("ab", "abc");
    compare("", "");
    compare("\xc3\xa1", "a");                   /* "á" en UTF-8: 0xC3 > 'a' */

    /* 3. Muchas concatenaciones: s = s + "ab", 1500 veces.
     *    s vive en rbx, que las rutinas conservan; r12 es el contador. */
    x_mov_ri(&a, RBX, lit(""));
    x_mov_ri(&a, R12, 1500);
    asm_bind(&a, ciclo);
    x_mov_rr(&a, RAX, RBX);
    x_mov_ri(&a, RCX, lit("ab"));
    x_call(&a, rt.concat);
    x_mov_rr(&a, RBX, RAX);
    x_dec(&a, R12);
    x_jcc(&a, CC_NE, ciclo);
    x_mov_rr(&a, RSI, RBX);
    x_call(&a, rt.strlen);
    x_mov_rr(&a, RAX, RDX);
    x_call(&a, rt.print_int);
    newline();

    /* 4. Un bloque mayor que HEAP_CHUNK: escribir 'Z' en su último byte y
     *    leerlo de vuelta. */
    x_mov_ri(&a, RDI, 3 << 20);
    x_call(&a, rt.alloc);
    x_mov_ri(&a, RCX, 'Z');
    x_store8(&a, RAX, (3 << 20) - 1, RCX);
    x_load8(&a, RAX, RAX, (3 << 20) - 1);
    x_call(&a, rt.print_int);                   /* 90 */
    newline();

    x_mov_ri(&a, RDI, 0);
    x_call(&a, rt.exit);

    rt_emit(&a, &rt);
    asm_finish(&a);
    if (elf_write(argv[1], &a, entry) != 0) { perror(argv[1]); return 1; }
    return 0;
}
