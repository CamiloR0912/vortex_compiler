/* ==========================================================================
 * tests/nativo/arreglos.c - Prueba de las rutinas de vectores y matrices
 *
 * Llama directamente a las rutinas de runtime_arrays.c: crear, convertir,
 * armar una matriz con sus filas, operar elemento a elemento, reducir,
 * comparar, indexar e imprimir. Termina con un índice fuera de rango.
 * Los arreglos se guardan en el marco: [rbp-8] = v, [rbp-16] = w (reales),
 * [rbp-24] = m (matriz).
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "../../runtime.h"

static Asm a;
static Runtime rt;

#define V  (-8)
#define W  (-16)
#define M  (-24)

static void newline(void) { rt_print_text(&a, &rt, "\n"); }

static void print_array(int slot, int kind) {
    x_load(&a, RAX, RBP, slot);
    x_mov_ri(&a, RSI, kind);
    x_call(&a, rt.print_arr);
    newline();
}

static void print_rax_array(int kind) {
    x_mov_ri(&a, RSI, kind);
    x_call(&a, rt.print_arr);
    newline();
}

/* Vector con los valores dados (como el literal [x, y, ...]) en rax */
static void vector_of(const uint64_t *values, int n) {
    int k;
    x_mov_ri(&a, RDI, n);
    x_mov_ri(&a, RSI, (uint64_t)-1);
    x_call(&a, rt.arr_new);
    for (k = 0; k < n; k++) {
        x_mov_ri(&a, RCX, values[k]);
        x_store(&a, RAX, 16 + 8 * k, RCX);
    }
}

static void arith(int op, int left_slot, int right_slot, int kind) {
    x_load(&a, RAX, RBP, left_slot);
    x_load(&a, RCX, RBP, right_slot);
    x_mov_ri(&a, RDI, op);
    x_mov_ri(&a, RSI, kind);
    x_mov_ri(&a, RDX, 1);
    x_call(&a, rt.arr_arr);
    print_rax_array(kind);
}

static void scalar(int op, int64_t value, int left) {
    x_load(&a, RAX, RBP, V);
    x_mov_ri(&a, RCX, (uint64_t)value);
    x_mov_ri(&a, RDI, op);
    x_mov_ri(&a, RSI, KIND_INT);
    x_mov_ri(&a, RDX, 1);
    x_mov_ri(&a, R8, left);
    x_call(&a, rt.arr_scalar);
    print_rax_array(KIND_INT);
}

static void reduce(int slot, int op, int kind) {
    x_load(&a, RAX, RBP, slot);
    x_mov_ri(&a, RDI, op);
    x_mov_ri(&a, RSI, kind);
    x_mov_ri(&a, RDX, 1);
    x_call(&a, rt.reduce);
    x_call(&a, kind == KIND_FLOAT ? rt.print_float : rt.print_int);
    newline();
}

int main(int argc, char **argv) {
    static const uint64_t v_values[] = { 3, 1, 4 };
    uint64_t strings[2], bools[2] = { 1, 0 };
    int entry;

    if (argc != 2) { fprintf(stderr, "Usage: %s output\n", argv[0]); return 1; }
    asm_init(&a);
    rt_init(&a, &rt);
    strings[0] = rt_literal(&a, "a", 1);
    strings[1] = rt_literal(&a, "b c", 3);
    entry = a.code.len;

    x_push(&a, RBP);
    x_mov_rr(&a, RBP, RSP);
    x_sub_ri(&a, RSP, 32);

    /* v = [3, 1, 4]; w = v convertido a reales */
    vector_of(v_values, 3);
    x_store(&a, RBP, V, RAX);
    print_array(V, KIND_INT);                       /* [3, 1, 4] */
    x_load(&a, RAX, RBP, V);
    x_call(&a, rt.arr_to_float);
    x_store(&a, RBP, W, RAX);
    print_array(W, KIND_FLOAT);                     /* [3.0, 1.0, 4.0] */

    /* m = [v, v]: las filas se apilan en orden */
    x_load(&a, RAX, RBP, V);
    x_push(&a, RAX);
    x_load(&a, RAX, RBP, V);
    x_push(&a, RAX);
    x_mov_ri(&a, RDI, 2);
    x_mov_ri(&a, RDX, 1);
    x_call(&a, rt.mat_from_rows);
    x_add_ri(&a, RSP, 16);
    x_store(&a, RBP, M, RAX);
    print_array(M, KIND_INT);                       /* [[3, 1, 4], [3, 1, 4]] */

    /* Elemento a elemento */
    arith(ARR_ADD, V, V, KIND_INT);                 /* [6, 2, 8] */
    arith(ARR_MUL, M, M, KIND_INT);                 /* [[9, 1, 16], [9, 1, 16]] */
    arith(ARR_DIV, W, W, KIND_FLOAT);               /* [1.0, 1.0, 1.0] */
    scalar(ARR_MUL, 10, 0);                         /* v * 10: [30, 10, 40] */
    scalar(ARR_SUB, 1, 0);                          /* v - 1:  [2, 0, 3] */
    scalar(ARR_SUB, 1, 1);                          /* 1 - v:  [-2, 0, -3] */
    scalar(ARR_MOD, 3, 0);                          /* v % 3:  [0, 1, 1] */
    x_load(&a, RAX, RBP, W);
    x_mov_ri(&a, RSI, KIND_FLOAT);
    x_call(&a, rt.arr_neg);
    print_rax_array(KIND_FLOAT);                    /* [-3.0, -1.0, -4.0] */

    /* Reducciones */
    reduce(V, RED_SUM, KIND_INT);                   /* 8 */
    reduce(V, RED_PROD, KIND_INT);                  /* 12 */
    reduce(V, RED_MAX, KIND_INT);                   /* 4 */
    reduce(V, RED_MIN, KIND_INT);                   /* 1 */
    reduce(M, RED_SUM, KIND_INT);                   /* 16 */
    reduce(W, RED_SUM, KIND_FLOAT);                 /* 8.0 */
    reduce(W, RED_MIN, KIND_FLOAT);                 /* 1.0 */

    /* Igualdad: v con su copia, y v con m */
    x_load(&a, RAX, RBP, V);
    x_call(&a, rt.arr_copy);
    x_mov_rr(&a, RCX, RAX);
    x_load(&a, RAX, RBP, V);
    x_mov_ri(&a, RSI, KIND_INT);
    x_call(&a, rt.arr_eq);
    x_call(&a, rt.print_bool);                      /* true */
    newline();
    x_load(&a, RAX, RBP, V);
    x_load(&a, RCX, RBP, M);
    x_mov_ri(&a, RSI, KIND_INT);
    x_call(&a, rt.arr_eq);
    x_call(&a, rt.print_bool);                      /* false */
    newline();

    /* Índices: v[2], m[1] y m[0][1] = 9 */
    x_load(&a, RAX, RBP, V);
    x_mov_ri(&a, RCX, 2);
    x_mov_ri(&a, RDX, 1);
    x_mov_ri(&a, RSI, rt_literal(&a, "vector<int>", 11));
    x_call(&a, rt.vec_get);
    x_call(&a, rt.print_int);                       /* 4 */
    newline();
    x_load(&a, RAX, RBP, M);
    x_mov_ri(&a, RCX, 1);
    x_mov_ri(&a, RDX, 1);
    x_mov_ri(&a, RSI, rt_literal(&a, "matrix<int>", 11));
    x_call(&a, rt.mat_row);
    print_rax_array(KIND_INT);                      /* [3, 1, 4] */
    x_load(&a, RAX, RBP, M);
    x_mov_ri(&a, RCX, 0);
    x_mov_ri(&a, R8, 1);
    x_mov_ri(&a, RDX, 1);
    x_mov_ri(&a, RSI, rt_literal(&a, "matrix<int>", 11));
    x_call(&a, rt.mat_addr2);
    x_mov_ri(&a, RCX, 9);
    x_store(&a, RAX, 0, RCX);
    print_array(M, KIND_INT);                       /* [[3, 9, 4], [3, 1, 4]] */

    /* Otros tipos de elementos y el vector vacío */
    vector_of(strings, 2);
    print_rax_array(KIND_STRING);                   /* ["a", "b c"] */
    vector_of(bools, 2);
    print_rax_array(KIND_BOOL);                     /* [true, false] */
    x_mov_ri(&a, RDI, 0);
    x_mov_ri(&a, RSI, (uint64_t)-1);
    x_call(&a, rt.arr_new);
    print_rax_array(KIND_INT);                      /* [] */

    /* Índice fuera de rango: termina el programa */
    x_load(&a, RAX, RBP, V);
    x_mov_ri(&a, RCX, 5);
    x_mov_ri(&a, RDX, 77);
    x_mov_ri(&a, RSI, rt_literal(&a, "vector<int>", 11));
    x_call(&a, rt.vec_get);
    rt_print_text(&a, &rt, "no debe imprimirse\n");
    x_mov_ri(&a, RDI, 0);
    x_call(&a, rt.exit);

    rt_emit(&a, &rt);
    asm_finish(&a);
    if (elf_write(argv[1], &a, entry) != 0) { perror(argv[1]); return 1; }
    return 0;
}
