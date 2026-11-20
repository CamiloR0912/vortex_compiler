/* ==========================================================================
 * main.c - vortexc, el compilador de Vortex
 *
 *   ./vortexc programa.vx            genera el ejecutable programa
 *   ./vortexc programa.vx -o salida  elige el nombre del ejecutable
 *   ./vortexc programa.vx --run      compila en una carpeta temporal y ejecuta
 *
 * Etapas:
 *   1. Análisis léxico y sintáctico  (lexer.l, parser.y)  -> AST
 *   2. Análisis semántico            (semantic.c)         -> AST anotado
 *   3. Generación de código          (x86gen.c)           -> instrucciones
 *   4. Codificación y escritura      (x86enc.c, elf.c)    -> ejecutable ELF
 *
 * Ningún paso usa un programa externo: vortexc escribe el ejecutable por
 * sí mismo. Si las etapas 1 o 2 encuentran errores, se reportan todos y no
 * se genera nada.
 * ========================================================================== */
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "vortex.h"

/* Ejecuta un programa y espera a que termine. Devuelve su código de salida.
 * Solo se usa con --run, para ejecutar el programa recién compilado. */
static int run_program(char *const argv[]) {
    int status = 0;
    pid_t pid;
    fflush(stdout);
    pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        execv(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

int main(int argc, char **argv) {
    const char *source = NULL, *output = NULL;
    char exe_path[PATH_MAX];
    char temp_dir[] = "/tmp/vortexc_XXXXXX";
    int run = 0, i, status;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--run") == 0) run = 1;
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) output = argv[++i];
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

    /* ---- Dónde queda el ejecutable ---- */
    if (run) {
        if (mkdtemp(temp_dir) == NULL) { perror("mkdtemp"); return 1; }
        snprintf(exe_path, sizeof exe_path, "%s/program", temp_dir);
    } else if (output != NULL) {
        snprintf(exe_path, sizeof exe_path, "%s", output);
    } else {
        /* programa.vx -> programa */
        size_t len = strlen(source);
        snprintf(exe_path, sizeof exe_path, "%s", source);
        if (len > 3 && strcmp(source + len - 3, ".vx") == 0) exe_path[len - 3] = '\0';
        else strncat(exe_path, ".out", sizeof exe_path - strlen(exe_path) - 1);
    }

    /* ---- Etapas 3 y 4: generación de código y ejecutable ---- */
    status = generate_native(program_root, exe_path);
    if (status != 0) {
        if (status > 0) printf("%d construct(s) not supported by the native backend; program not compiled.\n", status);
        if (run) rmdir(temp_dir);
        return 1;
    }

    if (!run) {
        printf("Compiled %s -> %s\n", source, exe_path);
        return 0;
    }

    /* ---- --run: ejecutar y limpiar ---- */
    {
        char *program[] = { exe_path, NULL };
        status = run_program(program);
    }
    remove(exe_path);
    rmdir(temp_dir);
    return status;
}
