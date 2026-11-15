/* ==========================================================================
 * main.c - vortexc, el compilador de Vortex
 *
 *   ./vortexc programa.vx            genera programa.c y el ejecutable programa
 *   ./vortexc programa.vx -o salida  elige el nombre del ejecutable
 *   ./vortexc programa.vx --run      compila en una carpeta temporal y ejecuta
 *
 * Etapas:
 *   1. Análisis léxico y sintáctico  (lexer.l, parser.y)  -> AST
 *   2. Análisis semántico            (semantic.c)         -> AST anotado
 *   3. Generación de código          (codegen.c)          -> programa en C
 *   4. gcc                                                -> ejecutable
 *
 * Si alguna de las dos primeras etapas encuentra errores, se reportan todos
 * y no se genera nada.
 * ========================================================================== */
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "vortex.h"

/* Ejecuta un programa y espera a que termine. Devuelve su código de salida.
 * Se usa fork/exec en vez de system() para que las rutas con espacios no
 * necesiten comillas. */
static int run_program(char *const argv[]) {
    int status = 0;
    pid_t pid;
    fflush(stdout);
    pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

/* Carpeta donde está vortexc: ahí mismo está vx_runtime.h */
static void compiler_dir(char *dir, size_t size, const char *argv0) {
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof path - 1);
    if (len > 0) path[len] = '\0';
    else snprintf(path, sizeof path, "%s", argv0);
    snprintf(dir, size, "%s", dirname(path));
}

int main(int argc, char **argv) {
    const char *source = NULL, *output = NULL;
    char exe_path[PATH_MAX], c_path[PATH_MAX + 8], rt_dir[PATH_MAX], include_flag[PATH_MAX + 8];
    char temp_dir[] = "/tmp/vortexc_XXXXXX";
    int run = 0, i, status;
    FILE *c_file;

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

    /* ---- Dónde quedan el .c y el ejecutable ---- */
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
        else strcat(exe_path, ".out");
    }
    snprintf(c_path, sizeof c_path, "%s.c", exe_path);

    /* ---- Etapa 3: generación de código ---- */
    c_file = fopen(c_path, "w");
    if (c_file == NULL) {
        fprintf(stderr, "Cannot write file: %s\n", c_path);
        return 1;
    }
    generate_c(program_root, c_file);
    fclose(c_file);

    /* ---- Etapa 4: gcc ----
     * -O0: sin optimizar, para que las variables locales conserven su valor
     *      cuando longjmp vuelve a un catch.
     * -w:  sin advertencias; el código generado no es para leerlo a mano. */
    compiler_dir(rt_dir, sizeof rt_dir, argv[0]);
    snprintf(include_flag, sizeof include_flag, "-I%s", rt_dir);
    {
        char *gcc[] = { "gcc", "-O0", "-w", include_flag, "-o", exe_path, c_path, NULL };
        status = run_program(gcc);
    }
    if (status != 0) {
        fprintf(stderr, "Internal error: gcc could not compile %s\n", c_path);
        return 1;
    }

    if (!run) {
        printf("Compiled %s -> %s (C code: %s)\n", source, exe_path, c_path);
        return 0;
    }

    /* ---- --run: ejecutar y limpiar ---- */
    {
        char *program[] = { exe_path, NULL };
        status = run_program(program);
    }
    remove(exe_path);
    remove(c_path);
    rmdir(temp_dir);
    return status;
}
