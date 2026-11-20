/* ==========================================================================
 * runtime_arrays.c - Rutinas de ejecución para vectores y matrices
 *
 * Un arreglo es la dirección de un bloque en el heap (como VxArr en la
 * versión anterior):
 *     [+0]   n      cantidad de elementos (vector) o de filas (matriz)
 *     [+8]   cols   -1 en un vector; cantidad de columnas en una matriz
 *     [+16]  los elementos, 8 bytes cada uno; una matriz guarda sus filas
 *            una tras otra
 * Un elemento ocupa 8 bytes igual que cualquier valor de la máquina de
 * pila: entero, bits de un double, 0/1 o la dirección de una cadena.
 *
 * Los arreglos se asignan, se pasan y se retornan por valor: el generador
 * llama a arr_copy cuando el origen es una variable. Las operaciones
 * siempre crean un arreglo nuevo.
 *
 * Las rutinas siguen la convención de runtime.h: pueden modificar rax,
 * rcx, rdx, rsi, rdi y r8..r11, y conservan rbx, rbp, rsp y r12..r15.
 * Las que recorren un arreglo llamando a otras rutinas guardan su estado
 * en rbx y r12..r15 (después de apilar sus valores anteriores).
 * ========================================================================== */
#include <string.h>
#include "runtime.h"

/* ---- Auxiliares de emisión ---- */

/* cmp r, imm (usando r11 para el valor) */
static void cmp_imm(Asm *a, Reg r, int64_t imm) {
    x_mov_ri(a, R11, (uint64_t)imm);
    x_cmp_rr(a, r, R11);
}

/* Error con un mensaje armado al ejecutar, al estilo de printf con %d
 * (entero) y %s (cadena). Antes de este código, el sitio del error apiló
 * la línea y después los nargs valores en orden; el valor k está en
 * [rsp + 8 * (nargs - 1 - k)] y la línea en [rsp + 8 * nargs].
 * El mensaje se arma con concat y str_int y se lanza con raise; raise no
 * vuelve, así que no hace falta desapilar nada.
 *           mov rax, "primer tramo"
 *           (por cada %)
 *           push rax                ; el mensaje parcial
 *           mov rax, [rsp + 8 + 8 * (nargs - 1 - k)]
 *           call str_int            ; solo para %d
 *           mov rcx, rax
 *           pop rax
 *           call concat
 *           mov rcx, "siguiente tramo"
 *           call concat
 *           (al final)
 *           mov rsi, rax
 *           mov rax, [rsp + 8 * nargs]   ; línea
 *           mov rdi, EXC_RUNTIME
 *           call raise */
static void emit_fail_fmt(Asm *a, Runtime *rt, const char *fmt, int nargs) {
    const char *p = fmt;
    int k = 0, len;

    len = (int)strcspn(p, "%");
    x_mov_ri(a, RAX, rt_literal(a, p, len));
    p += len;
    while (*p == '%') {
        char conv = p[1];
        p += 2;
        x_push(a, RAX);
        x_load(a, RAX, RSP, 8 + 8 * (nargs - 1 - k));
        if (conv == 'd') x_call(a, rt->str_int);
        x_mov_rr(a, RCX, RAX);
        x_pop(a, RAX);
        x_call(a, rt->concat);
        len = (int)strcspn(p, "%");
        if (len > 0) {
            x_mov_ri(a, RCX, rt_literal(a, p, len));
            x_call(a, rt->concat);
        }
        p += len;
        k++;
    }
    x_mov_rr(a, RSI, RAX);
    x_load(a, RAX, RSP, 8 * nargs);
    x_mov_ri(a, RDI, EXC_RUNTIME);
    x_call(a, rt->raise);
}

/* ===================== Crear y copiar ===================== */

/* arr_new: rdi = n, rsi = cols (-1 para un vector) -> rax = arreglo nuevo.
 * Los elementos quedan en cero: la memoria viene de mmap y no se reutiliza.
 *           push rdi
 *           push rsi
 *           mov rax, rdi
 *           test rsi, rsi
 *           jl vector
 *           imul rax, rsi           ; matriz: n * cols elementos
 *  vector:  shl rax, 3
 *           add rax, 16
 *           mov rdi, rax
 *           call alloc
 *           pop rsi
 *           pop rdi
 *           mov [rax], rdi
 *           mov [rax+8], rsi
 *           ret
 */
static void emit_arr_new(Asm *a, Runtime *rt) {
    Label vector = asm_label(a);
    asm_bind(a, rt->arr_new);
    x_push(a, RDI);
    x_push(a, RSI);
    x_mov_rr(a, RAX, RDI);
    x_test_rr(a, RSI, RSI);
    x_jcc(a, CC_L, vector);
    x_imul_rr(a, RAX, RSI);
    asm_bind(a, vector);
    x_shl_ri(a, RAX, 3);
    x_add_ri(a, RAX, 16);
    x_mov_rr(a, RDI, RAX);
    x_call(a, rt->alloc);
    x_pop(a, RSI);
    x_pop(a, RDI);
    x_store(a, RAX, 0, RDI);
    x_store(a, RAX, 8, RSI);
    x_ret(a);
}

/* arr_total: rax = arreglo -> rcx = cantidad total de elementos (usa rdx)
 *           mov rcx, [rax]
 *           mov rdx, [rax+8]
 *           test rdx, rdx
 *           jl fin                  ; vector: n
 *           imul rcx, rdx           ; matriz: n * cols
 *  fin:     ret
 */
static void emit_arr_total(Asm *a, Runtime *rt) {
    Label fin = asm_label(a);
    asm_bind(a, rt->arr_total);
    x_load(a, RCX, RAX, 0);
    x_load(a, RDX, RAX, 8);
    x_test_rr(a, RDX, RDX);
    x_jcc(a, CC_L, fin);
    x_imul_rr(a, RCX, RDX);
    asm_bind(a, fin);
    x_ret(a);
}

/* arr_copy: rax = arreglo -> rax = copia con la misma forma.
 * arr_to_float: rax = arreglo de enteros -> rax = arreglo de reales.
 * Ambas crean el destino con arr_new y recorren los elementos.
 *           push rax                ; origen
 *           mov rdi, [rax]
 *           mov rsi, [rax+8]
 *           call arr_new            ; rax = destino
 *           pop rsi
 *           push rax
 *           mov rax, rsi
 *           push rsi
 *           call arr_total          ; rcx = elementos
 *           pop rsi
 *           pop rax
 *           mov rdi, rax
 *           add rdi, 16
 *           add rsi, 16
 *   copia:  shl rcx, 3
 *           rep movsb
 *           ret
 *   a real: (por cada elemento) mov rdx, [rsi]; cvtsi2sd xmm0, rdx;
 *           movq rdx, xmm0; mov [rdi], rdx; avanzar 8 bytes
 */
static void emit_copy_header(Asm *a, Runtime *rt) {
    x_push(a, RAX);
    x_load(a, RDI, RAX, 0);
    x_load(a, RSI, RAX, 8);
    x_call(a, rt->arr_new);
    x_pop(a, RSI);
    x_push(a, RAX);
    x_mov_rr(a, RAX, RSI);
    x_push(a, RSI);
    x_call(a, rt->arr_total);
    x_pop(a, RSI);
    x_pop(a, RAX);
    x_mov_rr(a, RDI, RAX);
    x_add_ri(a, RDI, 16);
    x_add_ri(a, RSI, 16);
}

static void emit_arr_copy(Asm *a, Runtime *rt) {
    Label ciclo = asm_label(a), fin = asm_label(a);

    asm_bind(a, rt->arr_copy);
    emit_copy_header(a, rt);
    x_shl_ri(a, RCX, 3);
    x_rep_movsb(a);
    x_ret(a);

    asm_bind(a, rt->arr_to_float);
    emit_copy_header(a, rt);
    asm_bind(a, ciclo);
    x_test_rr(a, RCX, RCX);
    x_jcc(a, CC_E, fin);
    x_load(a, RDX, RSI, 0);
    x_cvtsi2sd(a, XMM0, RDX);
    x_movq_rx(a, RDX, XMM0);
    x_store(a, RDI, 0, RDX);
    x_add_ri(a, RSI, 8);
    x_add_ri(a, RDI, 8);
    x_sub_ri(a, RCX, 1);
    x_jmp(a, ciclo);
    asm_bind(a, fin);
    x_ret(a);
}

/* mat_from_rows: el literal [[...], [...]].
 * Entrada: rdi = cantidad de filas, rdx = línea; las filas (vectores) están
 * en la pila de quien llama: la fila 0 se apiló primero. Al entrar, la
 * fila k está en [rsp + 8 + 8 * (n - 1 - k)].
 * Todas las filas deben tener el largo de la fila 0; si no, es un error
 * ("matrix rows must have the same length (row 0 has A, row K has B)").
 * Registros conservados: rbx = filas, r12 = dirección de la ranura de la
 * fila 0 (la fila k está en [r12 - 8k]), r13 = columnas, r14 = línea. */
static void emit_mat_from_rows(Asm *a, Runtime *rt) {
    Label revisar = asm_label(a), siguiente = asm_label(a), revisadas = asm_label(a);
    Label copiar = asm_label(a), fin = asm_label(a);

    asm_bind(a, rt->mat_from_rows);
    x_mov_rr(a, R9, RSP);
    x_add_ri(a, R9, 8);                         /* ranura de la última fila */
    x_push(a, RBX);
    x_push(a, R12);
    x_push(a, R13);
    x_push(a, R14);
    x_mov_rr(a, RBX, RDI);
    x_mov_rr(a, R12, RDI);
    x_sub_ri(a, R12, 1);
    x_shl_ri(a, R12, 3);
    x_add_rr(a, R12, R9);                       /* ranura de la fila 0 */
    x_mov_rr(a, R14, RDX);
    x_load(a, RAX, R12, 0);
    x_load(a, R13, RAX, 0);                     /* columnas = largo de la fila 0 */

    /* Revisar el largo de las filas 1 .. n-1 */
    x_mov_ri(a, R10, 1);
    asm_bind(a, revisar);
    x_cmp_rr(a, R10, RBX);
    x_jcc(a, CC_GE, revisadas);
    x_mov_rr(a, R11, R10);
    x_shl_ri(a, R11, 3);
    x_mov_rr(a, RAX, R12);
    x_sub_rr(a, RAX, R11);
    x_load(a, RAX, RAX, 0);
    x_load(a, RAX, RAX, 0);                     /* largo de la fila k */
    x_cmp_rr(a, RAX, R13);
    x_jcc(a, CC_E, siguiente);
    x_push(a, R14);
    x_push(a, R13);
    x_push(a, R10);
    x_push(a, RAX);
    emit_fail_fmt(a, rt, "matrix rows must have the same length (row 0 has %d, row %d has %d)", 3);
    asm_bind(a, siguiente);
    x_add_ri(a, R10, 1);
    x_jmp(a, revisar);
    asm_bind(a, revisadas);

    /* Crear la matriz y copiar las filas una tras otra */
    x_mov_rr(a, RDI, RBX);
    x_mov_rr(a, RSI, R13);
    x_call(a, rt->arr_new);
    x_mov_rr(a, RDI, RAX);
    x_add_ri(a, RDI, 16);
    x_mov_ri(a, R10, 0);
    asm_bind(a, copiar);
    x_cmp_rr(a, R10, RBX);
    x_jcc(a, CC_GE, fin);
    x_mov_rr(a, R11, R10);
    x_shl_ri(a, R11, 3);
    x_mov_rr(a, RSI, R12);
    x_sub_rr(a, RSI, R11);
    x_load(a, RSI, RSI, 0);
    x_add_ri(a, RSI, 16);
    x_mov_rr(a, RCX, R13);
    x_shl_ri(a, RCX, 3);
    x_rep_movsb(a);                             /* rdi avanza a la fila siguiente */
    x_add_ri(a, R10, 1);
    x_jmp(a, copiar);
    asm_bind(a, fin);
    x_pop(a, R14);
    x_pop(a, R13);
    x_pop(a, R12);
    x_pop(a, RBX);
    x_ret(a);
}

/* ===================== Índices ===================== */

/* Revisa que 0 <= rcx < filas de la matriz rax (usa r9). Un índice
 * negativo, visto sin signo, es enorme: una sola comparación jb cubre los
 * dos extremos. Error: "index I out of range for TIPO with R rows". */
static void check_row(Asm *a, Runtime *rt) {
    Label ok = asm_label(a);
    x_load(a, R9, RAX, 0);
    x_cmp_rr(a, RCX, R9);
    x_jcc(a, CC_B, ok);
    x_push(a, RDX);
    x_push(a, RCX);
    x_push(a, RSI);
    x_push(a, R9);
    emit_fail_fmt(a, rt, "index %d out of range for %s with %d rows", 3);
    asm_bind(a, ok);
}

/* vec_addr: rax = vector, rcx = índice, rdx = línea, rsi = nombre del tipo
 *           -> rax = dirección del elemento
 * vec_get:  igual, pero rax = valor del elemento
 *           mov r8, [rax]
 *           cmp rcx, r8
 *           jb ok                   ; 0 <= i < n
 *           (error "index I out of range for TIPO of length N")
 *  ok:      shl rcx, 3
 *           add rax, rcx
 *           add rax, 16
 *           ret
 */
static void emit_vec_index(Asm *a, Runtime *rt) {
    Label ok = asm_label(a);
    asm_bind(a, rt->vec_addr);
    x_load(a, R8, RAX, 0);
    x_cmp_rr(a, RCX, R8);
    x_jcc(a, CC_B, ok);
    x_push(a, RDX);
    x_push(a, RCX);
    x_push(a, RSI);
    x_push(a, R8);
    emit_fail_fmt(a, rt, "index %d out of range for %s of length %d", 3);
    asm_bind(a, ok);
    x_shl_ri(a, RCX, 3);
    x_add_rr(a, RAX, RCX);
    x_add_ri(a, RAX, 16);
    x_ret(a);

    asm_bind(a, rt->vec_get);
    x_call(a, rt->vec_addr);
    x_load(a, RAX, RAX, 0);
    x_ret(a);
}

/* mat_row: rax = matriz, rcx = fila, rdx = línea, rsi = nombre del tipo
 *          -> rax = copia de la fila como vector (como vx_row)
 *           (revisar la fila)
 *           push rax
 *           push rcx
 *           mov rdi, [rax+8]        ; largo de la fila = columnas
 *           mov rsi, -1
 *           call arr_new
 *           pop rcx
 *           pop rsi                 ; rsi = matriz
 *           mov rdx, [rsi+8]
 *           imul rcx, rdx
 *           shl rcx, 3
 *           add rsi, rcx
 *           add rsi, 16             ; inicio de la fila
 *           mov rdi, rax
 *           add rdi, 16
 *           mov rcx, rdx
 *           shl rcx, 3
 *           rep movsb
 *           ret
 */
static void emit_mat_row(Asm *a, Runtime *rt) {
    asm_bind(a, rt->mat_row);
    check_row(a, rt);
    x_push(a, RAX);
    x_push(a, RCX);
    x_load(a, RDI, RAX, 8);
    x_mov_ri(a, RSI, (uint64_t)-1);
    x_call(a, rt->arr_new);
    x_pop(a, RCX);
    x_pop(a, RSI);
    x_load(a, RDX, RSI, 8);
    x_imul_rr(a, RCX, RDX);
    x_shl_ri(a, RCX, 3);
    x_add_rr(a, RSI, RCX);
    x_add_ri(a, RSI, 16);
    x_mov_rr(a, RDI, RAX);
    x_add_ri(a, RDI, 16);
    x_mov_rr(a, RCX, RDX);
    x_shl_ri(a, RCX, 3);
    x_rep_movsb(a);
    x_ret(a);
}

/* mat_addr2: rax = matriz, rcx = fila, r8 = columna, rdx = línea,
 *            rsi = nombre del tipo -> rax = dirección de m[i][j] (para
 *            asignar; como vx_at2)
 *           (revisar la fila)
 *           mov r9, [rax+8]         ; columnas
 *           cmp r8, r9
 *           jb ok
 *           (error "index J out of range for a row of TIPO with C columns")
 *  ok:      imul rcx, r9
 *           add rcx, r8
 *           shl rcx, 3
 *           add rax, rcx
 *           add rax, 16
 *           ret
 */
static void emit_mat_addr2(Asm *a, Runtime *rt) {
    Label ok = asm_label(a);
    asm_bind(a, rt->mat_addr2);
    check_row(a, rt);
    x_load(a, R9, RAX, 8);
    x_cmp_rr(a, R8, R9);
    x_jcc(a, CC_B, ok);
    x_push(a, RDX);
    x_push(a, R8);
    x_push(a, RSI);
    x_push(a, R9);
    emit_fail_fmt(a, rt, "index %d out of range for a row of %s with %d columns", 3);
    asm_bind(a, ok);
    x_imul_rr(a, RCX, R9);
    x_add_rr(a, RCX, R8);
    x_shl_ri(a, RCX, 3);
    x_add_rr(a, RAX, RCX);
    x_add_ri(a, RAX, 16);
    x_ret(a);
}

/* mat_set_row: m[i] = fila. rax = matriz, rcx = fila, r8 = vector,
 *              rdx = línea, rsi = nombre del tipo (como vx_set_row)
 *           (revisar la fila)
 *           mov r9, [rax+8]         ; columnas
 *           mov r10, [r8]           ; largo del vector
 *           cmp r10, r9
 *           je ok
 *           (error "cannot assign a row of length L to TIPO with C columns")
 *  ok:      imul rcx, r9
 *           shl rcx, 3
 *           mov rdi, rax
 *           add rdi, rcx
 *           add rdi, 16
 *           mov rsi, r8
 *           add rsi, 16
 *           mov rcx, r9
 *           shl rcx, 3
 *           rep movsb
 *           ret
 */
static void emit_mat_set_row(Asm *a, Runtime *rt) {
    Label ok = asm_label(a);
    asm_bind(a, rt->mat_set_row);
    check_row(a, rt);
    x_load(a, R9, RAX, 8);
    x_load(a, R10, R8, 0);
    x_cmp_rr(a, R10, R9);
    x_jcc(a, CC_E, ok);
    x_push(a, RDX);
    x_push(a, R10);
    x_push(a, RSI);
    x_push(a, R9);
    emit_fail_fmt(a, rt, "cannot assign a row of length %d to %s with %d columns", 3);
    asm_bind(a, ok);
    x_imul_rr(a, RCX, R9);
    x_shl_ri(a, RCX, 3);
    x_mov_rr(a, RDI, RAX);
    x_add_rr(a, RDI, RCX);
    x_add_ri(a, RDI, 16);
    x_mov_rr(a, RSI, R8);
    x_add_ri(a, RSI, 16);
    x_mov_rr(a, RCX, R9);
    x_shl_ri(a, RCX, 3);
    x_rep_movsb(a);
    x_ret(a);
}

/* ===================== Aritmética elemento a elemento ===================== */

/* elem_apply: una operación entre dos elementos (como vx_apply).
 * rax = x, rcx = y, rdi = operación (ARR_ADD..ARR_MOD), rsi = clase
 * (KIND_INT o KIND_FLOAT), rdx = línea -> rax = resultado.
 * La división entera repite lo que hace el generador para dos enteros:
 * divisor 0 es un error y divisor -1 se calcula aparte. Dividir reales
 * entre 0.0 también es un error. */
static void emit_elem_apply(Asm *a, Runtime *rt) {
    Label real = asm_label(a), no_suma = asm_label(a), no_resta = asm_label(a);
    Label no_mult = asm_label(a), no_cero = asm_label(a), normal = asm_label(a);
    Label resto_m1 = asm_label(a), cociente = asm_label(a);
    Label r_no_suma = asm_label(a), r_no_resta = asm_label(a), r_no_mult = asm_label(a);
    Label r_ok = asm_label(a), r_fin = asm_label(a);

    asm_bind(a, rt->elem_apply);
    x_mov_rr(a, R8, RDX);                       /* línea: cqo modifica rdx */
    cmp_imm(a, RSI, KIND_FLOAT);
    x_jcc(a, CC_E, real);

    /* Enteros */
    cmp_imm(a, RDI, ARR_ADD);
    x_jcc(a, CC_NE, no_suma);
    x_add_rr(a, RAX, RCX);
    x_ret(a);
    asm_bind(a, no_suma);
    cmp_imm(a, RDI, ARR_SUB);
    x_jcc(a, CC_NE, no_resta);
    x_sub_rr(a, RAX, RCX);
    x_ret(a);
    asm_bind(a, no_resta);
    cmp_imm(a, RDI, ARR_MUL);
    x_jcc(a, CC_NE, no_mult);
    x_imul_rr(a, RAX, RCX);
    x_ret(a);
    asm_bind(a, no_mult);                       /* ARR_DIV o ARR_MOD */
    x_test_rr(a, RCX, RCX);
    x_jcc(a, CC_NE, no_cero);
    x_push(a, R8);
    emit_fail_fmt(a, rt, "division by zero", 0);
    asm_bind(a, no_cero);
    cmp_imm(a, RCX, -1);
    x_jcc(a, CC_NE, normal);
    cmp_imm(a, RDI, ARR_DIV);
    x_jcc(a, CC_NE, resto_m1);
    x_neg(a, RAX);                              /* a / -1 = -a */
    x_ret(a);
    asm_bind(a, resto_m1);
    x_mov_ri(a, RAX, 0);                        /* a % -1 = 0 */
    x_ret(a);
    asm_bind(a, normal);
    x_mov_rr(a, R9, RDI);
    x_cqo(a);
    x_idiv(a, RCX);
    cmp_imm(a, R9, ARR_MOD);
    x_jcc(a, CC_NE, cociente);
    x_mov_rr(a, RAX, RDX);
    asm_bind(a, cociente);
    x_ret(a);

    /* Reales */
    asm_bind(a, real);
    x_movq_xr(a, XMM0, RAX);
    x_movq_xr(a, XMM1, RCX);
    cmp_imm(a, RDI, ARR_ADD);
    x_jcc(a, CC_NE, r_no_suma);
    x_addsd(a, XMM0, XMM1);
    x_jmp(a, r_fin);
    asm_bind(a, r_no_suma);
    cmp_imm(a, RDI, ARR_SUB);
    x_jcc(a, CC_NE, r_no_resta);
    x_subsd(a, XMM0, XMM1);
    x_jmp(a, r_fin);
    asm_bind(a, r_no_resta);
    cmp_imm(a, RDI, ARR_MUL);
    x_jcc(a, CC_NE, r_no_mult);
    x_mulsd(a, XMM0, XMM1);
    x_jmp(a, r_fin);
    asm_bind(a, r_no_mult);                     /* ARR_DIV */
    x_mov_ri(a, RDX, 0);
    x_movq_xr(a, XMM2, RDX);
    x_ucomisd(a, XMM1, XMM2);
    x_jcc(a, CC_NE, r_ok);
    x_jcc(a, CC_P, r_ok);                       /* NaN no es cero */
    x_push(a, R8);
    emit_fail_fmt(a, rt, "division by zero", 0);
    asm_bind(a, r_ok);
    x_divsd(a, XMM0, XMM1);
    asm_bind(a, r_fin);
    x_movq_rx(a, RAX, XMM0);
    x_ret(a);
}

/* Revisa que dos arreglos (rbx y r12) tengan la misma forma; si no, error
 * con la línea de r15 (como vx_arr_arr):
 *   misma forma: mismo n y, si n > 0, mismas columnas
 *   dos vectores: "vectors of different length (A and B) in an element-wise operation"
 *   si no:        "matrices of different shape in an element-wise operation" */
static void check_same_shape(Asm *a, Runtime *rt) {
    Label mala = asm_label(a), matrices = asm_label(a), ok = asm_label(a);
    x_load(a, R8, RBX, 0);
    x_load(a, R9, R12, 0);
    x_cmp_rr(a, R8, R9);
    x_jcc(a, CC_NE, mala);
    x_test_rr(a, R8, R8);
    x_jcc(a, CC_E, ok);
    x_load(a, R8, RBX, 8);
    x_load(a, R9, R12, 8);
    x_cmp_rr(a, R8, R9);
    x_jcc(a, CC_E, ok);
    asm_bind(a, mala);
    x_load(a, R8, RBX, 8);
    x_test_rr(a, R8, R8);
    x_jcc(a, CC_GE, matrices);
    x_load(a, R9, R12, 8);
    x_test_rr(a, R9, R9);
    x_jcc(a, CC_GE, matrices);
    x_push(a, R15);
    x_load(a, RAX, RBX, 0);
    x_push(a, RAX);
    x_load(a, RAX, R12, 0);
    x_push(a, RAX);
    emit_fail_fmt(a, rt, "vectors of different length (%d and %d) in an element-wise operation", 2);
    asm_bind(a, matrices);
    x_push(a, R15);
    emit_fail_fmt(a, rt, "matrices of different shape in an element-wise operation", 0);
    asm_bind(a, ok);
}

static void save_regs(Asm *a) {
    x_push(a, RBX); x_push(a, R12); x_push(a, R13); x_push(a, R14); x_push(a, R15);
}

static void restore_regs(Asm *a) {
    x_pop(a, R15); x_pop(a, R14); x_pop(a, R13); x_pop(a, R12); x_pop(a, RBX);
}

/* Prepara el arreglo resultado con la forma de rbx y deja en la pila:
 *   [rsp] = posición de escritura, [rsp+8] = elementos que faltan,
 *   [rsp+16] = resultado */
static void start_result(Asm *a, Runtime *rt) {
    x_load(a, RDI, RBX, 0);
    x_load(a, RSI, RBX, 8);
    x_call(a, rt->arr_new);
    x_push(a, RAX);
    x_mov_rr(a, RAX, RBX);
    x_call(a, rt->arr_total);
    x_push(a, RCX);
    x_load(a, RAX, RSP, 8);
    x_add_ri(a, RAX, 16);
    x_push(a, RAX);
}

/* Guarda rax en la posición de escritura, la avanza y descuenta un elemento */
static void store_result(Asm *a) {
    x_load(a, RCX, RSP, 0);
    x_store(a, RCX, 0, RAX);
    x_add_ri(a, RCX, 8);
    x_store(a, RSP, 0, RCX);
    x_load(a, RCX, RSP, 8);
    x_sub_ri(a, RCX, 1);
    x_store(a, RSP, 8, RCX);
}

/* arr_arr: arreglo (op) arreglo (como vx_arr_arr).
 * rax = a, rcx = b, rdi = operación, rsi = clase, rdx = línea -> rax.
 * rbx y r12 recorren a y b; r13 = operación, r14 = clase, r15 = línea. */
static void emit_arr_arr(Asm *a, Runtime *rt) {
    Label ciclo = asm_label(a), fin = asm_label(a);
    asm_bind(a, rt->arr_arr);
    save_regs(a);
    x_mov_rr(a, RBX, RAX);
    x_mov_rr(a, R12, RCX);
    x_mov_rr(a, R13, RDI);
    x_mov_rr(a, R14, RSI);
    x_mov_rr(a, R15, RDX);
    check_same_shape(a, rt);
    start_result(a, rt);
    x_add_ri(a, RBX, 16);
    x_add_ri(a, R12, 16);
    asm_bind(a, ciclo);
    x_load(a, RAX, RSP, 8);
    x_test_rr(a, RAX, RAX);
    x_jcc(a, CC_E, fin);
    x_load(a, RAX, RBX, 0);
    x_load(a, RCX, R12, 0);
    x_mov_rr(a, RDI, R13);
    x_mov_rr(a, RSI, R14);
    x_mov_rr(a, RDX, R15);
    x_call(a, rt->elem_apply);
    store_result(a);
    x_add_ri(a, RBX, 8);
    x_add_ri(a, R12, 8);
    x_jmp(a, ciclo);
    asm_bind(a, fin);
    x_add_ri(a, RSP, 16);
    x_pop(a, RAX);
    restore_regs(a);
    x_ret(a);
}

/* arr_scalar: arreglo (op) escalar (como vx_arr_scalar).
 * rax = arreglo, rcx = escalar, rdi = operación, rsi = clase,
 * rdx = línea, r8 = 1 si el escalar va a la izquierda -> rax.
 * rbx recorre el arreglo; r12 = escalar; el indicador queda en la pila. */
static void emit_arr_scalar(Asm *a, Runtime *rt) {
    Label ciclo = asm_label(a), fin = asm_label(a), orden = asm_label(a);
    asm_bind(a, rt->arr_scalar);
    save_regs(a);
    x_mov_rr(a, RBX, RAX);
    x_mov_rr(a, R12, RCX);
    x_mov_rr(a, R13, RDI);
    x_mov_rr(a, R14, RSI);
    x_mov_rr(a, R15, RDX);
    x_push(a, R8);                              /* [rsp+24] = escalar a la izquierda */
    start_result(a, rt);
    x_add_ri(a, RBX, 16);
    asm_bind(a, ciclo);
    x_load(a, RAX, RSP, 8);
    x_test_rr(a, RAX, RAX);
    x_jcc(a, CC_E, fin);
    x_load(a, RAX, RBX, 0);
    x_mov_rr(a, RCX, R12);
    x_load(a, RDX, RSP, 24);
    x_test_rr(a, RDX, RDX);
    x_jcc(a, CC_E, orden);
    x_mov_rr(a, RDX, RAX);                      /* escalar a la izquierda: intercambiar */
    x_mov_rr(a, RAX, RCX);
    x_mov_rr(a, RCX, RDX);
    asm_bind(a, orden);
    x_mov_rr(a, RDI, R13);
    x_mov_rr(a, RSI, R14);
    x_mov_rr(a, RDX, R15);
    x_call(a, rt->elem_apply);
    store_result(a);
    x_add_ri(a, RBX, 8);
    x_jmp(a, ciclo);
    asm_bind(a, fin);
    x_add_ri(a, RSP, 16);
    x_pop(a, RAX);
    x_add_ri(a, RSP, 8);                        /* el indicador */
    restore_regs(a);
    x_ret(a);
}

/* arr_neg: -arreglo. rax = arreglo, rsi = clase -> rax.
 * Un entero se niega con neg; un real invirtiendo su bit de signo. */
static void emit_arr_neg(Asm *a, Runtime *rt) {
    Label ciclo = asm_label(a), entero = asm_label(a), guardar = asm_label(a), fin = asm_label(a);
    asm_bind(a, rt->arr_neg);
    x_push(a, RBX);
    x_push(a, R12);
    x_mov_rr(a, RBX, RAX);
    x_mov_rr(a, R12, RSI);
    x_load(a, RDI, RBX, 0);
    x_load(a, RSI, RBX, 8);
    x_call(a, rt->arr_new);
    x_push(a, RAX);
    x_mov_rr(a, RAX, RBX);
    x_call(a, rt->arr_total);
    x_pop(a, RAX);
    x_mov_rr(a, RSI, RBX);
    x_add_ri(a, RSI, 16);
    x_mov_rr(a, RDI, RAX);
    x_add_ri(a, RDI, 16);
    asm_bind(a, ciclo);
    x_test_rr(a, RCX, RCX);
    x_jcc(a, CC_E, fin);
    x_load(a, RDX, RSI, 0);
    cmp_imm(a, R12, KIND_FLOAT);
    x_jcc(a, CC_NE, entero);
    x_mov_ri(a, R8, 0x8000000000000000ULL);
    x_xor_rr(a, RDX, R8);
    x_jmp(a, guardar);
    asm_bind(a, entero);
    x_neg(a, RDX);
    asm_bind(a, guardar);
    x_store(a, RDI, 0, RDX);
    x_add_ri(a, RSI, 8);
    x_add_ri(a, RDI, 8);
    x_sub_ri(a, RCX, 1);
    x_jmp(a, ciclo);
    asm_bind(a, fin);
    x_pop(a, R12);
    x_pop(a, RBX);
    x_ret(a);
}

/* arr_eq: rax = a, rcx = b, rsi = clase de los elementos -> rax = 1 si
 * tienen la misma forma y los mismos elementos (como vx_arr_eq). Los reales
 * se comparan con ucomisd (NaN no es igual a nada) y las cadenas con strcmp.
 * rbx y r12 recorren a y b; r13 = clase; r14 = elementos que faltan. */
static void emit_arr_eq(Asm *a, Runtime *rt) {
    Label distintos = asm_label(a), iguales = asm_label(a), fin = asm_label(a);
    Label forma_ok = asm_label(a), ciclo = asm_label(a), siguiente = asm_label(a);
    Label no_cadena = asm_label(a), no_real = asm_label(a);

    asm_bind(a, rt->arr_eq);
    x_push(a, RBX); x_push(a, R12); x_push(a, R13); x_push(a, R14);
    x_mov_rr(a, RBX, RAX);
    x_mov_rr(a, R12, RCX);
    x_mov_rr(a, R13, RSI);
    x_load(a, R8, RBX, 0);
    x_load(a, R9, R12, 0);
    x_cmp_rr(a, R8, R9);
    x_jcc(a, CC_NE, distintos);
    x_test_rr(a, R8, R8);
    x_jcc(a, CC_E, forma_ok);
    x_load(a, R8, RBX, 8);
    x_load(a, R9, R12, 8);
    x_cmp_rr(a, R8, R9);
    x_jcc(a, CC_NE, distintos);
    asm_bind(a, forma_ok);
    x_mov_rr(a, RAX, RBX);
    x_call(a, rt->arr_total);
    x_mov_rr(a, R14, RCX);
    x_add_ri(a, RBX, 16);
    x_add_ri(a, R12, 16);
    asm_bind(a, ciclo);
    x_test_rr(a, R14, R14);
    x_jcc(a, CC_E, iguales);
    x_load(a, RAX, RBX, 0);
    x_load(a, RCX, R12, 0);
    cmp_imm(a, R13, KIND_STRING);
    x_jcc(a, CC_NE, no_cadena);
    x_call(a, rt->strcmp);
    x_test_rr(a, RAX, RAX);
    x_jcc(a, CC_NE, distintos);
    x_jmp(a, siguiente);
    asm_bind(a, no_cadena);
    cmp_imm(a, R13, KIND_FLOAT);
    x_jcc(a, CC_NE, no_real);
    x_movq_xr(a, XMM0, RAX);
    x_movq_xr(a, XMM1, RCX);
    x_ucomisd(a, XMM0, XMM1);
    x_jcc(a, CC_NE, distintos);
    x_jcc(a, CC_P, distintos);
    x_jmp(a, siguiente);
    asm_bind(a, no_real);                       /* int o bool */
    x_cmp_rr(a, RAX, RCX);
    x_jcc(a, CC_NE, distintos);
    asm_bind(a, siguiente);
    x_add_ri(a, RBX, 8);
    x_add_ri(a, R12, 8);
    x_sub_ri(a, R14, 1);
    x_jmp(a, ciclo);
    asm_bind(a, iguales);
    x_mov_ri(a, RAX, 1);
    x_jmp(a, fin);
    asm_bind(a, distintos);
    x_mov_ri(a, RAX, 0);
    asm_bind(a, fin);
    x_pop(a, R14); x_pop(a, R13); x_pop(a, R12); x_pop(a, RBX);
    x_ret(a);
}

/* ===================== Reducciones ===================== */

/* reduce: sum, prod, max o min sobre todos los elementos de un vector o
 * matriz (como vx_reduce). rax = arreglo, rdi = operación (RED_SUM..
 * RED_MIN), rsi = clase (KIND_INT o KIND_FLOAT), rdx = línea -> rax.
 * max y min de un arreglo vacío son un error. Valores iniciales: sum 0,
 * prod 1, max y min el primer elemento. Los reales siguen las reglas de
 * C: max solo reemplaza si x > r, min si x < r (NaN nunca reemplaza).
 * Registros: rsi = posición, rcx = elementos que faltan, rdi = operación. */
static void emit_reduce(Asm *a, Runtime *rt) {
    Label no_vacio = asm_label(a), no_max = asm_label(a), real = asm_label(a), fin = asm_label(a);
    Label ini_listo = asm_label(a), r_ini_listo = asm_label(a);
    Label ciclo = asm_label(a), siguiente = asm_label(a), r_ciclo = asm_label(a), r_siguiente = asm_label(a);
    Label r_fin = asm_label(a);
    Label e_no_suma = asm_label(a), e_no_prod = asm_label(a), e_no_max = asm_label(a);
    Label r_no_suma = asm_label(a), r_no_prod = asm_label(a), r_no_max = asm_label(a);
    Label ini_no_suma = asm_label(a), ini_no_prod = asm_label(a);
    Label rini_no_suma = asm_label(a), rini_no_prod = asm_label(a);

    asm_bind(a, rt->reduce);
    x_mov_rr(a, R9, RDX);                       /* línea: arr_total usa rdx */
    x_mov_rr(a, R10, RSI);                      /* clase */
    x_mov_rr(a, RSI, RAX);
    x_call(a, rt->arr_total);                   /* rcx = elementos (rax sigue siendo el arreglo) */
    x_test_rr(a, RCX, RCX);
    x_jcc(a, CC_NE, no_vacio);
    cmp_imm(a, RDI, RED_MAX);
    x_jcc(a, CC_NE, no_max);
    x_push(a, R9);
    emit_fail_fmt(a, rt, "max of an empty vector", 0);
    asm_bind(a, no_max);
    cmp_imm(a, RDI, RED_MIN);
    x_jcc(a, CC_NE, no_vacio);
    x_push(a, R9);
    emit_fail_fmt(a, rt, "min of an empty vector", 0);
    asm_bind(a, no_vacio);
    x_add_ri(a, RSI, 16);
    cmp_imm(a, R10, KIND_FLOAT);
    x_jcc(a, CC_E, real);

    /* ---- Enteros: el resultado en rax ---- */
    cmp_imm(a, RDI, RED_SUM);
    x_jcc(a, CC_NE, ini_no_suma);
    x_mov_ri(a, RAX, 0);
    x_jmp(a, ini_listo);
    asm_bind(a, ini_no_suma);
    cmp_imm(a, RDI, RED_PROD);
    x_jcc(a, CC_NE, ini_no_prod);
    x_mov_ri(a, RAX, 1);
    x_jmp(a, ini_listo);
    asm_bind(a, ini_no_prod);
    x_load(a, RAX, RSI, 0);                     /* max y min: el primer elemento */
    asm_bind(a, ini_listo);
    asm_bind(a, ciclo);
    x_test_rr(a, RCX, RCX);
    x_jcc(a, CC_E, fin);
    x_load(a, RDX, RSI, 0);
    cmp_imm(a, RDI, RED_SUM);
    x_jcc(a, CC_NE, e_no_suma);
    x_add_rr(a, RAX, RDX);
    x_jmp(a, siguiente);
    asm_bind(a, e_no_suma);
    cmp_imm(a, RDI, RED_PROD);
    x_jcc(a, CC_NE, e_no_prod);
    x_imul_rr(a, RAX, RDX);
    x_jmp(a, siguiente);
    asm_bind(a, e_no_prod);
    cmp_imm(a, RDI, RED_MAX);
    x_jcc(a, CC_NE, e_no_max);
    x_cmp_rr(a, RDX, RAX);
    x_jcc(a, CC_LE, siguiente);
    x_mov_rr(a, RAX, RDX);
    x_jmp(a, siguiente);
    asm_bind(a, e_no_max);                      /* RED_MIN */
    x_cmp_rr(a, RDX, RAX);
    x_jcc(a, CC_GE, siguiente);
    x_mov_rr(a, RAX, RDX);
    asm_bind(a, siguiente);
    x_add_ri(a, RSI, 8);
    x_sub_ri(a, RCX, 1);
    x_jmp(a, ciclo);

    /* ---- Reales: el resultado en xmm0 ---- */
    asm_bind(a, real);
    cmp_imm(a, RDI, RED_SUM);
    x_jcc(a, CC_NE, rini_no_suma);
    x_mov_ri(a, RDX, 0);                        /* 0.0 */
    x_movq_xr(a, XMM0, RDX);
    x_jmp(a, r_ini_listo);
    asm_bind(a, rini_no_suma);
    cmp_imm(a, RDI, RED_PROD);
    x_jcc(a, CC_NE, rini_no_prod);
    x_mov_ri(a, RDX, 0x3FF0000000000000ULL);    /* 1.0 */
    x_movq_xr(a, XMM0, RDX);
    x_jmp(a, r_ini_listo);
    asm_bind(a, rini_no_prod);
    x_load(a, RDX, RSI, 0);
    x_movq_xr(a, XMM0, RDX);
    asm_bind(a, r_ini_listo);
    asm_bind(a, r_ciclo);
    x_test_rr(a, RCX, RCX);
    x_jcc(a, CC_E, r_fin);
    x_load(a, RDX, RSI, 0);
    x_movq_xr(a, XMM1, RDX);                    /* xmm1 = elemento, xmm0 = resultado */
    cmp_imm(a, RDI, RED_SUM);
    x_jcc(a, CC_NE, r_no_suma);
    x_addsd(a, XMM0, XMM1);
    x_jmp(a, r_siguiente);
    asm_bind(a, r_no_suma);
    cmp_imm(a, RDI, RED_PROD);
    x_jcc(a, CC_NE, r_no_prod);
    x_mulsd(a, XMM0, XMM1);
    x_jmp(a, r_siguiente);
    asm_bind(a, r_no_prod);
    cmp_imm(a, RDI, RED_MAX);
    x_jcc(a, CC_NE, r_no_max);
    x_ucomisd(a, XMM1, XMM0);                   /* x > r ? */
    x_jcc(a, CC_BE, r_siguiente);
    x_movsd_rr(a, XMM0, XMM1);
    x_jmp(a, r_siguiente);
    asm_bind(a, r_no_max);                      /* RED_MIN */
    x_ucomisd(a, XMM0, XMM1);                   /* r > x ? */
    x_jcc(a, CC_BE, r_siguiente);
    x_movsd_rr(a, XMM0, XMM1);
    asm_bind(a, r_siguiente);
    x_add_ri(a, RSI, 8);
    x_sub_ri(a, RCX, 1);
    x_jmp(a, r_ciclo);
    asm_bind(a, r_fin);
    x_movq_rx(a, RAX, XMM0);
    asm_bind(a, fin);
    x_ret(a);
}

/* ===================== Impresión ===================== */

/* print_items: imprime "[e1, e2, ...]" (como vx_print_items).
 * rdi = primer elemento, rsi = cantidad, rdx = clase. Dentro de un arreglo
 * las cadenas van entre comillas.
 * rbx = posición, r12 = cantidad, r13 = clase, r14 = índice. */
static void emit_print_items(Asm *a, Runtime *rt) {
    Label ciclo = asm_label(a), sin_coma = asm_label(a), cierre = asm_label(a), siguiente = asm_label(a);
    Label no_int = asm_label(a), no_float = asm_label(a), no_bool = asm_label(a);

    asm_bind(a, rt->print_items);
    x_push(a, RBX); x_push(a, R12); x_push(a, R13); x_push(a, R14);
    x_mov_rr(a, RBX, RDI);
    x_mov_rr(a, R12, RSI);
    x_mov_rr(a, R13, RDX);
    x_mov_ri(a, R14, 0);
    rt_print_text(a, rt, "[");
    asm_bind(a, ciclo);
    x_cmp_rr(a, R14, R12);
    x_jcc(a, CC_GE, cierre);
    x_test_rr(a, R14, R14);
    x_jcc(a, CC_E, sin_coma);
    rt_print_text(a, rt, ", ");
    asm_bind(a, sin_coma);
    x_load(a, RAX, RBX, 0);
    cmp_imm(a, R13, KIND_INT);
    x_jcc(a, CC_NE, no_int);
    x_call(a, rt->print_int);
    x_jmp(a, siguiente);
    asm_bind(a, no_int);
    cmp_imm(a, R13, KIND_FLOAT);
    x_jcc(a, CC_NE, no_float);
    x_call(a, rt->print_float);
    x_jmp(a, siguiente);
    asm_bind(a, no_float);
    cmp_imm(a, R13, KIND_BOOL);
    x_jcc(a, CC_NE, no_bool);
    x_call(a, rt->print_bool);
    x_jmp(a, siguiente);
    asm_bind(a, no_bool);                       /* cadena, entre comillas */
    x_push(a, RAX);
    rt_print_text(a, rt, "\"");
    x_pop(a, RAX);
    x_call(a, rt->print_str);
    rt_print_text(a, rt, "\"");
    asm_bind(a, siguiente);
    x_add_ri(a, RBX, 8);
    x_add_ri(a, R14, 1);
    x_jmp(a, ciclo);
    asm_bind(a, cierre);
    rt_print_text(a, rt, "]");
    x_pop(a, R14); x_pop(a, R13); x_pop(a, R12); x_pop(a, RBX);
    x_ret(a);
}

/* print_arr: imprime un vector o una matriz (como vx_print_arr).
 * rax = arreglo, rsi = clase de los elementos.
 * Un vector: print_items. Una matriz: "[" y cada fila con print_items,
 * separadas por ", ", y "]".
 * rbx = matriz, r12 = clase, r13 = fila. */
static void emit_print_arr(Asm *a, Runtime *rt) {
    Label matriz = asm_label(a), ciclo = asm_label(a), sin_coma = asm_label(a);
    Label cierre = asm_label(a), fin = asm_label(a);

    asm_bind(a, rt->print_arr);
    x_push(a, RBX); x_push(a, R12); x_push(a, R13);
    x_mov_rr(a, RBX, RAX);
    x_mov_rr(a, R12, RSI);
    x_load(a, RAX, RBX, 8);
    x_test_rr(a, RAX, RAX);
    x_jcc(a, CC_GE, matriz);
    x_mov_rr(a, RDI, RBX);
    x_add_ri(a, RDI, 16);
    x_load(a, RSI, RBX, 0);
    x_mov_rr(a, RDX, R12);
    x_call(a, rt->print_items);
    x_jmp(a, fin);
    asm_bind(a, matriz);
    rt_print_text(a, rt, "[");
    x_mov_ri(a, R13, 0);
    asm_bind(a, ciclo);
    x_load(a, RAX, RBX, 0);
    x_cmp_rr(a, R13, RAX);
    x_jcc(a, CC_GE, cierre);
    x_test_rr(a, R13, R13);
    x_jcc(a, CC_E, sin_coma);
    rt_print_text(a, rt, ", ");
    asm_bind(a, sin_coma);
    x_mov_rr(a, RDI, R13);
    x_load(a, RAX, RBX, 8);
    x_imul_rr(a, RDI, RAX);
    x_shl_ri(a, RDI, 3);
    x_add_rr(a, RDI, RBX);
    x_add_ri(a, RDI, 16);                       /* inicio de la fila */
    x_load(a, RSI, RBX, 8);
    x_mov_rr(a, RDX, R12);
    x_call(a, rt->print_items);
    x_add_ri(a, R13, 1);
    x_jmp(a, ciclo);
    asm_bind(a, cierre);
    rt_print_text(a, rt, "]");
    asm_bind(a, fin);
    x_pop(a, R13); x_pop(a, R12); x_pop(a, RBX);
    x_ret(a);
}

void rt_emit_arrays(Asm *a, Runtime *rt) {
    emit_arr_new(a, rt);
    emit_arr_total(a, rt);
    emit_arr_copy(a, rt);
    emit_mat_from_rows(a, rt);
    emit_vec_index(a, rt);
    emit_mat_row(a, rt);
    emit_mat_addr2(a, rt);
    emit_mat_set_row(a, rt);
    emit_elem_apply(a, rt);
    emit_arr_arr(a, rt);
    emit_arr_scalar(a, rt);
    emit_arr_neg(a, rt);
    emit_arr_eq(a, rt);
    emit_reduce(a, rt);
    emit_print_items(a, rt);
    emit_print_arr(a, rt);
}
