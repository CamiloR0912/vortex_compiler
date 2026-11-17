/* ==========================================================================
 * main.c - vortexc, el compilador de Vortex
 *
 *   ./vortexc programa.vx            compila programa.vx
 *   ./vortexc programa.vx -o salida  elige el nombre del ejecutable
 *   ./vortexc programa.vx --run      compila en una carpeta temporal y ejecuta
 *
 * Etapas:
 *   1. Análisis léxico y sintáctico  (lexer.l, parser.y)  -> AST
 *   2. Análisis semántico            (semantic.c)         -> AST anotado
 *
 * Si alguna de las etapas encuentra errores, se reportan todos y no se
 * genera nada. La generación de código nativo todavía no está conectada:
 * por ahora vortexc solo valida el programa.
 * ========================================================================== */
#include <stdlib.h>
#include <string.h>
#include "vortex.h"

int main(int argc, char **argv) {
    const char *source = NULL;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--run") == 0) continue;
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) i++;
        else if (source == NULL) source = argv[i];
        else source = "";       /* más de un archivo: se trata como error de uso */
    }
    if (source == NULL || source[0] == '\0') {
        fprintf(stderr, "Usage: %s file.vx [-o output] [--run]\n", argv[0]);
        return 1;
    }

    /* ---- Etapas 1 y 2: análisis ---- */
    yyin = fopen(source, "r");
    if (yyin == NULL) {
        fprintf(stderr, "Cannot open file: %s\n", source);
        return 1;
    }
    yyparse();
    flush_pending();    /* error de sintaxis del que no se pudo recuperar */
    fclose(yyin);

    /* El análisis semántico necesita un AST completo */
    if (num_errors == 0) semantic_check(program_root);

    if (num_errors > 0) {
        printf("%d error(s) found; program not compiled.\n", num_errors);
        return 1;
    }

    fprintf(stderr, "%s: valid program; native code generation is not implemented yet.\n", source);
    return 1;
}
