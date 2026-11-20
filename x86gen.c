/* ==========================================================================
 * x86gen.c - Generación de código: AST anotado -> instrucciones x86-64
 *
 * Recorre el AST que semantic.c ya verificó y emite, con el codificador de
 * x86enc.c, las instrucciones del programa. Al final agrega las rutinas de
 * runtime.c y elf.c escribe el ejecutable.
 *
 * Modelo de máquina de pila:
 *   - El resultado de toda expresión queda en rax. Un bool es 0 o 1.
 *   - Un operador binario calcula el operando izquierdo y lo apila,
 *     calcula el derecho, lo pasa a rcx y desapila el izquierdo en rax:
 *         <izquierdo>       push rax
 *         <derecho>         mov rcx, rax
 *                           pop rax          ; rax = izq, rcx = der
 *                           <operación>      ; resultado en rax
 *   - No se asignan registros: cada valor intermedio vive en la pila.
 *
 * Variables:
 *   Todo el programa es una sola función con marco de pila:
 *         push rbp
 *         mov rbp, rsp
 *         sub rsp, N        ; N = 8 bytes por variable
 *   Cada variable ocupa 8 bytes en [rbp - 8k]. semantic.c le dio a cada
 *   símbolo un número único, así que dos variables con el mismo nombre en
 *   ámbitos distintos (shadowing) tienen posiciones distintas sin hacer
 *   nada especial.
 *
 * Alcance de la etapa 2: int, bool, variables, asignación, bloques,
 * aritmética, comparaciones, operadores lógicos y print. Si el programa usa
 * algo más, se reporta como no soportado y no se genera el ejecutable.
 * ========================================================================== */
#include <stdlib.h>
#include <string.h>
#include "vortex.h"
#include "runtime.h"

static Asm a;
static Runtime rt;
static int unsupported = 0;     /* construcciones que la etapa aún no genera */

/* Posición de cada variable en el marco, indexada por el id del símbolo */
static int *slot_of = NULL;
static int slot_cap = 0;
static int frame_size = 0;

static void gen_expr(Node *n);
static void gen_stmt(Node *n);
static void gen_stmts(Node *list);

static void not_supported(int line, const char *what) {
    printf("[Native backend] line %d: %s is not supported yet\n", line, what);
    unsupported++;
}

/* ¿El tipo se representa como un entero de 64 bits en esta etapa? */
static int is_scalar(Type t) {
    return t.kind == TY_INT || t.kind == TY_BOOL;
}

/* Desplazamiento respecto a rbp de la variable s; la primera vez que
 * aparece se le asigna la siguiente posición libre del marco. */
static int var_offset(Symbol *s) {
    if (s->id >= slot_cap) {
        int old = slot_cap;
        slot_cap = s->id * 2 + 16;
        slot_of = realloc(slot_of, slot_cap * sizeof *slot_of);
        memset(slot_of + old, 0, (slot_cap - old) * sizeof *slot_of);
    }
    if (slot_of[s->id] == 0) {
        frame_size += 8;
        slot_of[s->id] = -frame_size;
    }
    return slot_of[s->id];
}

/* ===================== Expresiones ===================== */

/* Deja en rax un 1 si se cumple la condición sobre las banderas, si no 0 */
static void set_bool(Cond cc) {
    x_setcc_al(&a, cc);
    x_movzx_rax_al(&a);
}

/* División y residuo enteros: rax = izq, rcx = der.
 *   - Divisor cero: error de ejecución.
 *   - Divisor -1: idiv fallaría con el entero más negativo (su negado no
 *     cabe en 64 bits), así que se calcula aparte: a / -1 = -a, a % -1 = 0.
 *   - En otro caso, cqo + idiv: cociente en rax, residuo en rdx. */
static void gen_divmod(Node *n) {
    Label no_cero = asm_label(&a), normal = asm_label(&a), fin = asm_label(&a);
    x_test_rr(&a, RCX, RCX);
    x_jcc(&a, CC_NE, no_cero);
    rt_fail(&a, &rt, n->line, "division by zero");
    asm_bind(&a, no_cero);
    x_mov_ri(&a, RDX, (uint64_t)-1);
    x_cmp_rr(&a, RCX, RDX);
    x_jcc(&a, CC_NE, normal);
    if (n->op == OP_DIV) x_neg(&a, RAX);
    else x_mov_ri(&a, RAX, 0);
    x_jmp(&a, fin);
    asm_bind(&a, normal);
    x_cqo(&a);
    x_idiv(&a, RCX);
    if (n->op == OP_MOD) x_mov_rr(&a, RAX, RDX);
    asm_bind(&a, fin);
}

/* && y || con cortocircuito: si el operando izquierdo ya decide el
 * resultado, el derecho no se evalúa.
 *         <izquierdo>
 *         test rax, rax
 *         je fin            ; && : si es falso, el resultado es 0 (ya en rax)
 *         (jne fin          ; || : si es verdadero, el resultado es 1)
 *         <derecho>         ; si no, el resultado es el del derecho
 *   fin: */
static void gen_logic(Node *n) {
    Label fin = asm_label(&a);
    gen_expr(n->a);
    x_test_rr(&a, RAX, RAX);
    x_jcc(&a, n->op == OP_AND ? CC_E : CC_NE, fin);
    gen_expr(n->b);
    asm_bind(&a, fin);
}

static void gen_binary(Node *n) {
    if (!is_scalar(n->a->type) || !is_scalar(n->b->type)) {
        not_supported(n->line, "an operation on this type");
        return;
    }
    if (n->op == OP_AND || n->op == OP_OR) { gen_logic(n); return; }

    gen_expr(n->a);
    x_push(&a, RAX);
    gen_expr(n->b);
    x_mov_rr(&a, RCX, RAX);
    x_pop(&a, RAX);

    switch (n->op) {
        case OP_ADD: x_add_rr(&a, RAX, RCX); break;
        case OP_SUB: x_sub_rr(&a, RAX, RCX); break;
        case OP_MUL: x_imul_rr(&a, RAX, RCX); break;
        case OP_DIV:
        case OP_MOD: gen_divmod(n); break;
        default:
            /* Comparaciones: cmp y set<cc> según el operador */
            x_cmp_rr(&a, RAX, RCX);
            switch (n->op) {
                case OP_LT: set_bool(CC_L); break;
                case OP_GT: set_bool(CC_G); break;
                case OP_LE: set_bool(CC_LE); break;
                case OP_GE: set_bool(CC_GE); break;
                case OP_EQ: set_bool(CC_E); break;
                default:    set_bool(CC_NE); break;   /* OP_NE */
            }
    }
}

static void gen_expr(Node *n) {
    if (!is_scalar(n->type)) {
        not_supported(n->line, type_name(n->type));
        return;
    }
    switch (n->kind) {
        case N_INT:
            x_mov_ri(&a, RAX, (uint64_t)strtoll(n->text, NULL, 10));
            break;
        case N_BOOL:
            x_mov_ri(&a, RAX, strcmp(n->text, "true") == 0);
            break;
        case N_VAR:
            x_load(&a, RAX, RBP, var_offset(n->sym));
            break;
        case N_UNARY:
            gen_expr(n->a);
            if (n->op == OP_NEG) {
                x_neg(&a, RAX);
            } else {                            /* OP_NOT: rax = (rax == 0) */
                x_test_rr(&a, RAX, RAX);
                set_bool(CC_E);
            }
            break;
        case N_BINARY:
            gen_binary(n);
            break;
        case N_CALL:   not_supported(n->line, "a function call"); break;
        case N_INDEX:  not_supported(n->line, "indexing"); break;
        case N_REDUCE: not_supported(n->line, "a reduction"); break;
        default:       not_supported(n->line, "this expression"); break;
    }
}

/* ===================== Sentencias ===================== */

/* print(e1, ..., en): primero se calculan todos los valores y se apilan;
 * después se imprimen en orden, separados por un espacio, y un salto de
 * línea al final. Así, si el cálculo de un valor falla, no queda una
 * línea impresa a medias. Con n valores apilados, el k-ésimo (desde 0)
 * está en [rsp + 8 * (n - 1 - k)]. */
static void gen_print(Node *n) {
    Node *e;
    int count = 0, k;

    for (e = n->a; e != NULL; e = e->next) {
        gen_expr(e);
        x_push(&a, RAX);
        count++;
    }
    for (e = n->a, k = 0; e != NULL; e = e->next, k++) {
        if (k > 0) rt_print_text(&a, &rt, " ");
        x_load(&a, RAX, RSP, 8 * (count - 1 - k));
        x_call(&a, e->type.kind == TY_BOOL ? rt.print_bool : rt.print_int);
    }
    rt_print_text(&a, &rt, "\n");
    if (count > 0) x_add_ri(&a, RSP, 8 * count);
}

/* if / else if / else. Cada condición deja 0 o 1 en rax:
 *           <condición 1>
 *           test rax, rax
 *           je siguiente1         ; falsa: probar la siguiente rama
 *           <bloque 1>
 *           jmp fin
 * siguiente1:
 *           <condición 2>         ; else if
 *           ...
 *           <bloque else>
 * fin: */
static void gen_if(Node *n) {
    Label fin = asm_label(&a);
    for (;;) {
        Label siguiente = asm_label(&a);
        gen_expr(n->a);
        x_test_rr(&a, RAX, RAX);
        x_jcc(&a, CC_E, siguiente);
        gen_stmt(n->b);
        if (n->c == NULL) {                 /* sin else */
            asm_bind(&a, siguiente);
            break;
        }
        x_jmp(&a, fin);
        asm_bind(&a, siguiente);
        if (n->c->kind == N_BLOCK) {        /* else final */
            gen_stmt(n->c);
            break;
        }
        n = n->c;                           /* else if: otra vuelta */
    }
    asm_bind(&a, fin);
}

static void gen_stmt(Node *n) {
    switch (n->kind) {
        case N_VARDECL:
            if (!is_scalar(n->type)) { not_supported(n->line, type_name(n->type)); break; }
            gen_expr(n->a);
            x_store(&a, RBP, var_offset(n->sym), RAX);
            break;
        case N_ASSIGN:
            if (n->a->kind != N_VAR) { not_supported(n->line, "assignment to an element"); break; }
            gen_expr(n->b);
            x_store(&a, RBP, var_offset(n->a->sym), RAX);
            break;
        case N_EXPRSTMT:
            gen_expr(n->a);             /* el valor se descarta */
            break;
        case N_BLOCK:
            gen_stmts(n->a);
            break;
        case N_PRINT:
            gen_print(n);
            break;
        case N_IF:
            gen_if(n);
            break;
        case N_WHILE:    not_supported(n->line, "'while'"); break;
        case N_FOR:      not_supported(n->line, "'for'"); break;
        case N_SWITCH:   not_supported(n->line, "'switch'"); break;
        case N_FUNC:     not_supported(n->line, "a function definition"); break;
        case N_TRY:      not_supported(n->line, "'try'"); break;
        case N_ASSERT:   not_supported(n->line, "'assert'"); break;
        case N_THROW:    not_supported(n->line, "'throw'"); break;
        default:         not_supported(n->line, "this statement"); break;
    }
}

static void gen_stmts(Node *list) {
    for (; list != NULL; list = list->next) gen_stmt(list);
}

/* Genera el programa completo y escribe el ejecutable en path.
 * Devuelve el número de construcciones no soportadas (0 si se generó). */
int generate_native(Node *program, const char *path) {
    int frame_at;

    asm_init(&a);
    rt_init(&a, &rt);

    /* Prólogo. El tamaño del marco se completa al final. */
    x_push(&a, RBP);
    x_mov_rr(&a, RBP, RSP);
    x_sub_ri(&a, RSP, 0);
    frame_at = a.code.len - 4;

    gen_stmts(program);

    /* Fin del programa: exit(0) */
    x_mov_ri(&a, RDI, 0);
    x_call(&a, rt.exit);

    if (unsupported > 0) return unsupported;

    asm_patch_u32(&a, frame_at, (uint32_t)frame_size);
    rt_emit(&a, &rt);
    asm_finish(&a);
    if (elf_write(path, &a, 0) != 0) {
        perror(path);
        return -1;
    }
    return 0;
}
