/* ==========================================================================
 * tests/nativo/reales_esperado.c - Genera reales.out
 *
 * Programa en C (no es un generador del backend): imprime cada valor de
 * reales_valores.h como lo hacía la versión anterior de Vortex (vx_str_f)
 * y agrega la línea del código de salida que escribe run.sh.
 *     gcc -o /tmp/re tests/nativo/reales_esperado.c && /tmp/re > tests/nativo/reales.out
 * run.sh lo omite porque no recibe un nombre de archivo de salida.
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "reales_valores.h"

int main(int argc, char **argv) {
    size_t i;
    if (argc != 1) return 0;
    (void)argv;
    for (i = 0; i < sizeof valores / sizeof valores[0]; i++) {
        char buf[40];
        snprintf(buf, 36, "%.15g", valores[i]);
        if (strpbrk(buf, ".en") == NULL) strcat(buf, ".0");
        printf("%s\n", buf);
    }
    printf("exit code: 0\n");
    return 0;
}
