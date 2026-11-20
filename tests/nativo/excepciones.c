/* ==========================================================================
 * tests/nativo/excepciones.c - Prueba de raise y de la pila de manejadores
 *
 * El programa generado registra un manejador a mano (lo mismo que hará un
 * try), lanza errores desde dentro de una rutina para comprobar que rsp y
 * rbp se restauran, y termina con una excepción sin manejador:
 *   1. error de ejecución capturado: el catch recibe "mensaje (line 10)";
 *   2. throw capturado: el catch recibe el valor tal cual;
 *   3. throw sin manejador: "[Uncaught exception] line 30: ..." y código 1.
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "../../runtime.h"

static Asm a;
static Runtime rt;

/* Registra un manejador cuyo catch es la etiqueta indicada:
 *   handlers[try_top] = { rsp, rbp, catch, depth }; try_top += 1 */
static void registrar(Label catch_label) {
    x_mov_ri(&a, R8, rt.try_top);
    x_load(&a, RAX, R8, 0);
    x_mov_rr(&a, RCX, RAX);
    x_shl_ri(&a, RCX, 5);
    x_mov_ri(&a, RDX, rt.handlers);
    x_add_rr(&a, RCX, RDX);
    x_store(&a, RCX, 0, RSP);
    x_store(&a, RCX, 8, RBP);
    x_lea_label(&a, RDX, catch_label);
    x_store(&a, RCX, 16, RDX);
    x_mov_ri(&a, R9, rt.depth);
    x_load(&a, RDX, R9, 0);
    x_store(&a, RCX, 24, RDX);
    x_add_ri(&a, RAX, 1);
    x_store(&a, R8, 0, RAX);
}

int main(int argc, char **argv) {
    Label catch1, catch2, lanzar, fin1, fin2;
    int entry;

    if (argc != 2) { fprintf(stderr, "Usage: %s output\n", argv[0]); return 1; }
    asm_init(&a);
    rt_init(&a, &rt);
    catch1 = asm_label(&a); catch2 = asm_label(&a); lanzar = asm_label(&a);
    fin1 = asm_label(&a); fin2 = asm_label(&a);
    entry = a.code.len;

    x_push(&a, RBP);
    x_mov_rr(&a, RBP, RSP);

    /* 1. Error de ejecución dentro de una rutina, con valores apilados */
    registrar(catch1);
    x_push(&a, RBX);                        /* basura en la pila: debe descartarse */
    x_push(&a, RBX);
    x_call(&a, lanzar);
    rt_print_text(&a, &rt, "no debe imprimirse\n");
    x_jmp(&a, fin1);
    asm_bind(&a, catch1);
    x_push(&a, RAX);
    rt_print_text(&a, &rt, "capturado: ");
    x_pop(&a, RAX);
    x_call(&a, rt.print_str);
    rt_print_text(&a, &rt, "\n");
    x_mov_rr(&a, RAX, RSP);                 /* rsp volvió a ser rbp */
    x_cmp_rr(&a, RAX, RBP);
    x_setcc_al(&a, CC_E);
    x_movzx_rax_al(&a);
    x_call(&a, rt.print_bool);
    rt_print_text(&a, &rt, "\n");
    asm_bind(&a, fin1);

    /* 2. throw capturado: el texto es el valor lanzado */
    registrar(catch2);
    x_mov_ri(&a, RSI, rt_literal(&a, "lanzado", 7));
    x_mov_ri(&a, RAX, 20);
    x_mov_ri(&a, RDI, EXC_THROW);
    x_call(&a, rt.raise);
    x_jmp(&a, fin2);
    asm_bind(&a, catch2);
    x_call(&a, rt.print_str);
    rt_print_text(&a, &rt, "\n");
    asm_bind(&a, fin2);

    /* 3. Sin manejador: termina el programa */
    x_mov_ri(&a, RSI, rt_literal(&a, "sin manejador", 13));
    x_mov_ri(&a, RAX, 30);
    x_mov_ri(&a, RDI, EXC_THROW);
    x_call(&a, rt.raise);
    rt_print_text(&a, &rt, "no debe imprimirse\n");
    x_mov_ri(&a, RDI, 0);
    x_call(&a, rt.exit);

    /* lanzar: una rutina que falla en la "línea 10" */
    asm_bind(&a, lanzar);
    x_push(&a, RBP);
    x_mov_rr(&a, RBP, RSP);
    x_sub_ri(&a, RSP, 64);
    rt_fail(&a, &rt, 10, "division by zero");

    rt_emit(&a, &rt);
    asm_finish(&a);
    if (elf_write(argv[1], &a, entry) != 0) { perror(argv[1]); return 1; }
    return 0;
}
