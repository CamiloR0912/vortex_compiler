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
 * Variables y funciones:
 *   El programa principal y cada función tienen su marco de pila:
 *         push rbp
 *         mov rbp, rsp
 *         sub rsp, N        ; N = 8 bytes por variable local
 *   Cada variable ocupa 8 bytes en [rbp - 8k] del marco de la función que
 *   la declara; los parámetros están en [rbp + 24...] y el enlace estático
 *   en [rbp + 16]. semantic.c le dio a cada símbolo un número único, así
 *   que dos variables con el mismo nombre en ámbitos distintos (shadowing)
 *   tienen posiciones distintas sin hacer nada especial. Una función
 *   anidada llega a las variables de las funciones que la contienen
 *   siguiendo la cadena de enlaces estáticos.
 *
 * Alcance hasta la etapa 5: int, bool, float, string, variables,
 * asignación, bloques, aritmética, comparaciones, operadores lógicos,
 * concatenación, len de cadenas, print, if/else, while, for, break,
 * continue, switch y funciones (recursión, sobrecarga, parámetros
 * nombrados y por defecto, funciones anidadas). Si el programa usa algo
 * más, se reporta como no soportado y no se genera el ejecutable.
 * ========================================================================== */
#include <stdlib.h>
#include <string.h>
#include "vortex.h"
#include "runtime.h"

static Asm a;
static Runtime rt;
static int unsupported = 0;     /* construcciones que la etapa aún no genera */

/* Lo que el generador sabe de cada símbolo, indexado por su id */
typedef struct {
    int level;          /* variable: nivel de la función dueña (0 = programa)
                           función: nivel de su cuerpo (1 si se define en el programa) */
    int offset;         /* variable: posición respecto al rbp de esa función */
    int frame;          /* función: bytes de variables locales de su marco */
    int has_label;
    Label label;        /* función: dirección de su código */
} SymInfo;

static SymInfo *info = NULL;
static int info_cap = 0;

static int cur_level = 0;       /* nivel de la función que se está generando */
static int frame_size = 0;      /* bytes de variables y temporales de su marco */

static void gen_expr(Node *n);
static void gen_stmt(Node *n);
static void gen_stmts(Node *list);

static void not_supported(int line, const char *what) {
    printf("[Native backend] line %d: %s is not supported yet\n", line, what);
    unsupported++;
}

/* ¿El tipo ya lo genera esta etapa? Todos viajan en rax como 64 bits:
 * int y bool como enteros, float como los bits IEEE 754 del double y
 * string como la dirección de sus bytes (terminados en 0). */
static int is_scalar(Type t) {
    return t.kind == TY_INT || t.kind == TY_BOOL || t.kind == TY_FLOAT || t.kind == TY_STRING;
}

/* Bytes de un literal de cadena. El lexema conserva las comillas y las
 * secuencias de escape tal como se escribieron; la versión anterior las
 * copiaba al código C y las interpretaba gcc. Aquí se interpretan las de
 * C: \n \t \r \0 \\ \" \' \a \b \f \v; en cualquier otra, la barra se
 * descarta y queda el carácter. */
static int unescape(const char *lexeme, char *out) {
    const char *p = lexeme + 1;                 /* sin la comilla inicial */
    int len = 0;
    while (*p != '\0' && *(p + 1) != '\0') {    /* hasta la comilla final */
        char c = *p++;
        if (c == '\\') {
            c = *p++;
            switch (c) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '0': c = '\0'; break;
                case 'a': c = '\a'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'v': c = '\v'; break;
                default:  break;                /* \\ \" \' y el resto */
            }
        }
        out[len++] = c;
    }
    return len;
}

static SymInfo *sym_info(Symbol *s) {
    if (s->id >= info_cap) {
        int old = info_cap;
        info_cap = s->id * 2 + 16;
        info = realloc(info, info_cap * sizeof *info);
        memset(info + old, 0, (info_cap - old) * sizeof *info);
    }
    return &info[s->id];
}

/* ===================== Posiciones de las variables =====================
 * Antes de generar código se recorre el árbol y se asigna a cada variable
 * su nivel y su posición en el marco de la función que la declara. Así,
 * cuando el código de cualquier parte del programa menciona una variable,
 * su posición ya se conoce aunque su declaración se genere después. */

static int layout_size;         /* bytes asignados en el marco que se recorre */

static void layout_var(Symbol *s, int level) {
    SymInfo *si = sym_info(s);
    layout_size += 8;
    si->level = level;
    si->offset = -layout_size;
}

static void layout_stmts(Node *list, int level);

/* Marco de una función de nivel level:
 *   [rbp + 24 + 8*(n-1-k)]  parámetro k de n (el caller los apila en orden)
 *   [rbp + 16]              enlace estático
 *   [rbp + 8]               dirección de retorno (la apila call)
 *   [rbp]                   rbp de quien llamó
 *   [rbp - 8], [rbp - 16]...  variables locales */
static void layout_func(Node *f, int level) {
    SymInfo *fi = sym_info(f->sym);
    int saved = layout_size, count = 0, k;
    Node *p;

    fi->level = level;
    for (p = f->a; p != NULL; p = p->next) count++;
    for (p = f->a, k = 0; p != NULL; p = p->next, k++) {
        SymInfo *pi = sym_info(p->sym);
        pi->level = level;
        pi->offset = 24 + 8 * (count - 1 - k);
    }
    layout_size = 0;
    layout_stmts(f->b->a, level);
    fi->frame = layout_size;
    layout_size = saved;
}

static void layout_stmt(Node *n, int level) {
    Node *c;
    switch (n->kind) {
        case N_VARDECL: layout_var(n->sym, level); break;
        case N_BLOCK:   layout_stmts(n->a, level); break;
        case N_IF:
            layout_stmt(n->b, level);
            if (n->c != NULL) layout_stmt(n->c, level);    /* else o else if */
            break;
        case N_WHILE:   layout_stmt(n->b, level); break;
        case N_FOR:
            if (n->a != NULL) layout_stmt(n->a, level);    /* variable del for */
            layout_stmt(n->d, level);
            break;
        case N_SWITCH:
            for (c = n->b; c != NULL; c = c->next) layout_stmt(c->b, level);
            break;
        case N_FUNC:    layout_func(n, level + 1); break;
        default: break;
    }
}

static void layout_stmts(Node *list, int level) {
    for (; list != NULL; list = list->next) layout_stmt(list, level);
}

/* ===================== Acceso a variables =====================
 * Cada marco de función guarda en [rbp+16] su enlace estático: la dirección
 * del marco de la función que contiene su definición. Para llegar a una
 * variable de una función externa se sigue esa cadena tantos pasos como
 * niveles las separen. */

/* Deja en r la dirección del marco del nivel indicado (<= cur_level) */
static void frame_of(Reg r, int level) {
    int k;
    if (level == cur_level) { x_mov_rr(&a, r, RBP); return; }
    x_load(&a, r, RBP, 16);
    for (k = cur_level - 1; k > level; k--) x_load(&a, r, r, 16);
}

/* rax = valor de la variable */
static void load_var(Symbol *s) {
    SymInfo *si = sym_info(s);
    if (si->level == cur_level) {
        x_load(&a, RAX, RBP, si->offset);
    } else {
        frame_of(RAX, si->level);
        x_load(&a, RAX, RAX, si->offset);
    }
}

/* variable = rax (usa rcx para recorrer los enlaces) */
static void store_var(Symbol *s) {
    SymInfo *si = sym_info(s);
    if (si->level == cur_level) {
        x_store(&a, RBP, si->offset, RAX);
    } else {
        frame_of(RCX, si->level);
        x_store(&a, RCX, si->offset, RAX);
    }
}

/* Etiqueta del código de una función; se crea la primera vez que se pide,
 * que puede ser una llamada anterior a la definición. */
static Label func_label(Symbol *f) {
    SymInfo *fi = sym_info(f);
    if (!fi->has_label) {
        fi->label = asm_label(&a);
        fi->has_label = 1;
    }
    return fi->label;
}

/* Funciones encontradas mientras se genera; se generan después del código
 * que las contiene, para que no queden en medio de él. */
static Node **pending = NULL;
static int pending_count = 0, pending_cap = 0;

static void defer_func(Node *f) {
    if (pending_count == pending_cap) {
        pending_cap = pending_cap ? pending_cap * 2 : 16;
        pending = realloc(pending, pending_cap * sizeof *pending);
    }
    pending[pending_count++] = f;
}

/* Salida de una función: deshacer el marco y volver. El valor de retorno,
 * si lo hay, ya está en rax. */
static void gen_epilogue(void) {
    x_mov_rr(&a, RSP, RBP);
    x_pop(&a, RBP);
    x_ret(&a);
}

/* Reserva en el marco una posición que no corresponde a ninguna variable
 * del programa (por ejemplo, el valor de un switch) */
static int temp_offset(void) {
    frame_size += 8;
    return -frame_size;
}

/* ===================== Expresiones ===================== */

/* Deja en rax un 1 si se cumple la condición sobre las banderas, si no 0 */
static void set_bool(Cond cc) {
    x_setcc_al(&a, cc);
    x_movzx_rax_al(&a);
}

/* Calcula e y, si hace falta, la promueve al tipo to. La única promoción
 * entre valores simples es int -> float:
 *           cvtsi2sd xmm0, rax
 *           movq rax, xmm0 */
static void gen_conv(Node *e, Type to) {
    gen_expr(e);
    if (e->type.kind == TY_INT && to.kind == TY_FLOAT) {
        x_cvtsi2sd(&a, XMM0, RAX);
        x_movq_rx(&a, RAX, XMM0);
    }
}

/* Calcula e y la convierte en cadena, para la concatenación con +:
 * int, float y bool pasan por str_int, str_float y str_bool. */
static void gen_as_string(Node *e) {
    gen_expr(e);
    switch (e->type.kind) {
        case TY_INT:   x_call(&a, rt.str_int); break;
        case TY_FLOAT: x_call(&a, rt.str_float); break;
        case TY_BOOL:  x_call(&a, rt.str_bool); break;
        default:       break;                   /* ya es una cadena */
    }
}

/* Operación entre dos float: rax = izquierdo, rcx = derecho (bits).
 *           movq xmm0, rax
 *           movq xmm1, rcx
 *           addsd / subsd / mulsd / divsd xmm0, xmm1
 *           movq rax, xmm0
 * Dividir entre 0.0 (o -0.0) es un error de ejecución, como en la versión
 * anterior; ucomisd con 0.0 da ZF = 1 también si el divisor es NaN, por
 * eso además se revisa la paridad.
 * Comparaciones con ucomisd, con la semántica de C ante un NaN (todas
 * falsas salvo !=):
 *   a > b:  ucomisd a, b; seta        a >= b: ucomisd a, b; setae
 *   a < b:  ucomisd b, a; seta        a <= b: ucomisd b, a; setae
 *   a == b: ZF = 1 y PF = 0           a != b: ZF = 0 o PF = 1 */
static void gen_float_op(Node *n) {
    x_movq_xr(&a, XMM0, RAX);
    x_movq_xr(&a, XMM1, RCX);
    switch (n->op) {
        case OP_ADD: x_addsd(&a, XMM0, XMM1); break;
        case OP_SUB: x_subsd(&a, XMM0, XMM1); break;
        case OP_MUL: x_mulsd(&a, XMM0, XMM1); break;
        case OP_DIV: {
            Label ok = asm_label(&a);
            x_mov_ri(&a, RDX, 0);
            x_movq_xr(&a, XMM2, RDX);
            x_ucomisd(&a, XMM1, XMM2);
            x_jcc(&a, CC_NE, ok);
            x_jcc(&a, CC_P, ok);
            rt_fail(&a, &rt, n->line, "division by zero");
            asm_bind(&a, ok);
            x_divsd(&a, XMM0, XMM1);
            break;
        }
        case OP_GT: x_ucomisd(&a, XMM0, XMM1); set_bool(CC_A);  return;
        case OP_GE: x_ucomisd(&a, XMM0, XMM1); set_bool(CC_AE); return;
        case OP_LT: x_ucomisd(&a, XMM1, XMM0); set_bool(CC_A);  return;
        case OP_LE: x_ucomisd(&a, XMM1, XMM0); set_bool(CC_AE); return;
        case OP_EQ:
        case OP_NE:
            x_ucomisd(&a, XMM0, XMM1);
            set_bool(n->op == OP_EQ ? CC_E : CC_NE);
            x_mov_rr(&a, RDX, RAX);             /* mov no cambia las banderas */
            set_bool(n->op == OP_EQ ? CC_NP : CC_P);
            if (n->op == OP_EQ) x_and_rr(&a, RAX, RDX);
            else x_or_rr(&a, RAX, RDX);
            return;
        default: break;
    }
    x_movq_rx(&a, RAX, XMM0);
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

    /* Concatenación: el resultado es string si alguno de los operandos
     * lo es; el otro se convierte a cadena.
     *           <izquierdo como cadena>
     *           push rax
     *           <derecho como cadena>
     *           mov rcx, rax
     *           pop rax
     *           call concat */
    if (n->type.kind == TY_STRING) {
        gen_as_string(n->a);
        x_push(&a, RAX);
        gen_as_string(n->b);
        x_mov_rr(&a, RCX, RAX);
        x_pop(&a, RAX);
        x_call(&a, rt.concat);
        return;
    }

    /* Operandos: rax = izquierdo, rcx = derecho, según el tipo.
     *   - cadenas: strcmp deja -1, 0 o 1 en rax y rcx = 0; abajo se
     *     compara rax con 0 con el mismo operador (como strcmp(a, b) < 0);
     *   - si interviene un float, los dos se promueven y opera gen_float_op;
     *   - enteros y booleanos: los valores mismos. */
    if (n->a->type.kind == TY_STRING) {
        gen_expr(n->a);
        x_push(&a, RAX);
        gen_expr(n->b);
        x_mov_rr(&a, RCX, RAX);
        x_pop(&a, RAX);
        x_call(&a, rt.strcmp);
        x_mov_ri(&a, RCX, 0);
    } else if (n->a->type.kind == TY_FLOAT || n->b->type.kind == TY_FLOAT) {
        Type f = type_of(TY_FLOAT, TY_ERROR);
        gen_conv(n->a, f);
        x_push(&a, RAX);
        gen_conv(n->b, f);
        x_mov_rr(&a, RCX, RAX);
        x_pop(&a, RAX);
        gen_float_op(n);
        return;
    } else {
        gen_expr(n->a);
        x_push(&a, RAX);
        gen_expr(n->b);
        x_mov_rr(&a, RCX, RAX);
        x_pop(&a, RAX);
    }

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

/* Llamada a función. semantic.c ya eligió la versión (n->sym) y dejó en
 * n->b los argumentos en el orden de los parámetros, con los valores por
 * defecto en lugar de los que faltan.
 *           <argumento 1>
 *           push rax
 *           ...
 *           <argumento n>
 *           push rax
 *           <enlace estático>     ; marco de la función que contiene la definición
 *           push rax
 *           call f
 *           add rsp, 8 * (n + 1)  ; quitar argumentos y enlace
 * El resultado queda en rax. Un valor por defecto se calcula aquí, en
 * cada llamada, como en la versión anterior.
 *
 * Límite de recursión: un contador en la bss lleva las llamadas en curso.
 * Como en la versión anterior, se verifica y se incrementa antes de
 * calcular los argumentos, y se decrementa al volver. Con 1000 llamadas
 * en curso, la siguiente es un error de ejecución en lugar de agotar la
 * pila del proceso. */
#define MAX_DEPTH 1000

static void gen_call(Node *n) {
    SymInfo *fi = sym_info(n->sym);
    Label ok = asm_label(&a);
    char msg[160];
    Node *b;
    int count = 0;

    /*       mov rcx, depth
     *       mov rax, [rcx]
     *       mov rdx, 1000
     *       cmp rax, rdx
     *       jl ok
     *       <error>
     * ok:   add rax, 1
     *       mov [rcx], rax */
    snprintf(msg, sizeof msg, "maximum recursion depth (%d) exceeded in '%s'", MAX_DEPTH, n->text);
    x_mov_ri(&a, RCX, rt.depth);
    x_load(&a, RAX, RCX, 0);
    x_mov_ri(&a, RDX, MAX_DEPTH);
    x_cmp_rr(&a, RAX, RDX);
    x_jcc(&a, CC_L, ok);
    rt_fail(&a, &rt, n->line, msg);
    asm_bind(&a, ok);
    x_add_ri(&a, RAX, 1);
    x_store(&a, RCX, 0, RAX);

    for (b = n->b; b != NULL; b = b->next) {
        if (!is_scalar(b->type)) { not_supported(n->line, "a parameter of this type"); return; }
        gen_conv(b->a, b->type);            /* int -> float si el parámetro es float */
        x_push(&a, RAX);
        count++;
    }
    frame_of(RAX, fi->level - 1);
    x_push(&a, RAX);
    x_call(&a, func_label(n->sym));
    x_add_ri(&a, RSP, 8 * (count + 1));

    /* depth = depth - 1, sin tocar rax, que tiene el resultado */
    x_mov_ri(&a, RCX, rt.depth);
    x_load(&a, RDX, RCX, 0);
    x_sub_ri(&a, RDX, 1);
    x_store(&a, RCX, 0, RDX);
}

static void gen_expr(Node *n) {
    int void_call = n->kind == N_CALL && n->type.kind == TY_VOID;
    if (!is_scalar(n->type) && !void_call) {
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
        case N_STRING: {                        /* dirección del literal en los datos */
            char *bytes = malloc(strlen(n->text) + 1);
            int len = unescape(n->text, bytes);
            x_mov_ri(&a, RAX, rt_literal(&a, bytes, len));
            free(bytes);
            break;
        }
        case N_FLOAT: {                         /* los bits del double, calculados por strtod */
            double v = strtod(n->text, NULL);
            uint64_t bits;
            memcpy(&bits, &v, sizeof bits);
            x_mov_ri(&a, RAX, bits);
            break;
        }
        case N_VAR:
            load_var(n->sym);
            break;
        case N_UNARY:
            gen_expr(n->a);
            if (n->op == OP_NEG && n->type.kind == TY_FLOAT) {
                x_mov_ri(&a, RCX, 0x8000000000000000ULL);  /* invertir el bit de signo */
                x_xor_rr(&a, RAX, RCX);
            } else if (n->op == OP_NEG) {
                x_neg(&a, RAX);
            } else {                            /* OP_NOT: rax = (rax == 0) */
                x_test_rr(&a, RAX, RAX);
                set_bool(CC_E);
            }
            break;
        case N_BINARY:
            gen_binary(n);
            break;
        case N_CALL:   gen_call(n); break;
        case N_INDEX:  not_supported(n->line, "indexing"); break;
        case N_REDUCE:
            if (n->op == OP_LEN && n->a->type.kind == TY_STRING) {
                gen_expr(n->a);                 /* len(cadena) = strlen */
                x_mov_rr(&a, RSI, RAX);
                x_call(&a, rt.strlen);
                x_mov_rr(&a, RAX, RDX);
            } else {
                not_supported(n->line, "a reduction on an array");
            }
            break;
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
        x_call(&a, e->type.kind == TY_BOOL ? rt.print_bool
                 : e->type.kind == TY_FLOAT ? rt.print_float
                 : e->type.kind == TY_STRING ? rt.print_str : rt.print_int);
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

/* Ciclos abiertos, del más externo al más interno. break salta al final
 * del ciclo más interno y continue a su punto de continuación. */
typedef struct {
    Label brk;          /* destino de break: después del ciclo          */
    Label cont;         /* destino de continue: la condición o el paso  */
} Loop;

static Loop loops[256];
static int loop_count = 0;

static void loop_open(Label brk, Label cont) {
    loops[loop_count].brk = brk;
    loops[loop_count].cont = cont;
    loop_count++;
}

static void loop_close(void) {
    loop_count--;
}

/* while (condición) { cuerpo }
 * inicio:   <condición>           ; continue salta aquí
 *           test rax, rax
 *           je fin
 *           <cuerpo>
 *           jmp inicio
 * fin:                            ; break salta aquí */
static void gen_while(Node *n) {
    Label inicio = asm_label(&a), fin = asm_label(&a);
    asm_bind(&a, inicio);
    gen_expr(n->a);
    x_test_rr(&a, RAX, RAX);
    x_jcc(&a, CC_E, fin);
    loop_open(fin, inicio);
    gen_stmt(n->b);
    loop_close();
    x_jmp(&a, inicio);
    asm_bind(&a, fin);
}

/* for (inicio; condición; paso) { cuerpo }
 *           <inicio>              ; declaración o asignación, opcional
 * ciclo:    <condición>           ; si se omite, el ciclo es infinito
 *           test rax, rax
 *           je fin
 *           <cuerpo>
 * paso:     <paso>                ; opcional; continue salta aquí
 *           jmp ciclo
 * fin:                            ; break salta aquí
 * continue va al paso y no a la condición: si saltara a la condición, la
 * variable del ciclo no avanzaría.
 * La variable declarada en el inicio es un símbolo propio del for, así que
 * no choca con otra variable del mismo nombre fuera del ciclo. */
static void gen_for(Node *n) {
    Label ciclo = asm_label(&a), paso = asm_label(&a), fin = asm_label(&a);
    if (n->a != NULL) gen_stmt(n->a);
    asm_bind(&a, ciclo);
    if (n->b != NULL) {
        gen_expr(n->b);
        x_test_rr(&a, RAX, RAX);
        x_jcc(&a, CC_E, fin);
    }
    loop_open(fin, paso);
    gen_stmt(n->d);
    loop_close();
    asm_bind(&a, paso);
    if (n->c != NULL) gen_stmt(n->c);
    x_jmp(&a, ciclo);
    asm_bind(&a, fin);
}

/* switch (valor) { case v1: { ... } case v2: { ... } default: { ... } }
 * Como en la versión anterior, es una cadena de comparaciones sin paso de
 * un case al siguiente (sin fall-through). El valor se calcula una sola
 * vez y se guarda en una posición propia del marco; cada case calcula su
 * expresión y la compara:
 *           <valor>
 *           mov [rbp+t], rax
 *           <expresión del case 1>
 *           mov rcx, [rbp+t]
 *           cmp rcx, rax
 *           jne siguiente1
 *           <bloque 1>
 *           jmp fin
 * siguiente1:
 *           ...
 *           <bloque default>      ; sin comparación
 * fin:
 * El switch no es un ciclo: un break dentro de él sale del ciclo que lo
 * contiene, igual que en la versión anterior. */
static void gen_switch(Node *n) {
    Label fin = asm_label(&a);
    int t = temp_offset();
    Node *c;

    gen_expr(n->a);
    x_store(&a, RBP, t, RAX);
    for (c = n->b; c != NULL; c = c->next) {
        Label siguiente = asm_label(&a);
        if (c->a != NULL) {                 /* case; NULL es default */
            /* Se compara en el tipo común: un case float en un switch
             * int promueve el valor del switch, como en la versión anterior */
            Type common = unify(n->a->type, c->a->type);
            gen_conv(c->a, common);
            x_load(&a, RCX, RBP, t);
            if (common.kind == TY_STRING) {    /* strcmp(valor, case) == 0 */
                x_mov_rr(&a, RDX, RAX);
                x_mov_rr(&a, RAX, RCX);
                x_mov_rr(&a, RCX, RDX);
                x_call(&a, rt.strcmp);
                x_test_rr(&a, RAX, RAX);
                x_jcc(&a, CC_NE, siguiente);
            } else if (common.kind == TY_FLOAT) {
                if (n->a->type.kind == TY_INT) {
                    x_cvtsi2sd(&a, XMM0, RCX);
                } else {
                    x_movq_xr(&a, XMM0, RCX);
                }
                x_movq_xr(&a, XMM1, RAX);
                x_ucomisd(&a, XMM0, XMM1);
                x_jcc(&a, CC_NE, siguiente);
                x_jcc(&a, CC_P, siguiente);     /* NaN: no es igual a nada */
            } else {
                x_cmp_rr(&a, RCX, RAX);
                x_jcc(&a, CC_NE, siguiente);
            }
        }
        gen_stmt(c->b);
        x_jmp(&a, fin);
        asm_bind(&a, siguiente);
    }
    asm_bind(&a, fin);
}

static void gen_stmt(Node *n) {
    switch (n->kind) {
        case N_VARDECL:
            if (!is_scalar(n->type)) { not_supported(n->line, type_name(n->type)); break; }
            gen_conv(n->a, n->type);
            store_var(n->sym);
            break;
        case N_ASSIGN:
            if (n->a->kind != N_VAR) { not_supported(n->line, "assignment to an element"); break; }
            gen_conv(n->b, n->a->type);
            store_var(n->a->sym);
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
        case N_WHILE:
            gen_while(n);
            break;
        case N_FOR:
            gen_for(n);
            break;
        case N_BREAK:                   /* semantic.c ya verificó que hay un ciclo */
            x_jmp(&a, loops[loop_count - 1].brk);
            break;
        case N_CONTINUE:
            x_jmp(&a, loops[loop_count - 1].cont);
            break;
        case N_SWITCH:
            gen_switch(n);
            break;
        case N_FUNC:                    /* su código se genera después */
            defer_func(n);
            break;
        case N_RETURN:
            if (n->a != NULL) gen_conv(n->a, n->type);  /* n->type: tipo de retorno */
            gen_epilogue();
            break;
        case N_TRY:      not_supported(n->line, "'try'"); break;
        case N_ASSERT:   not_supported(n->line, "'assert'"); break;
        case N_THROW:    not_supported(n->line, "'throw'"); break;
        default:         not_supported(n->line, "this statement"); break;
    }
}

static void gen_stmts(Node *list) {
    for (; list != NULL; list = list->next) gen_stmt(list);
}

/* Código de una función:
 *   f:      push rbp
 *           mov rbp, rsp
 *           sub rsp, N            ; N se completa al terminar la función
 *           <cuerpo>
 *           mov rsp, rbp          ; fin sin return (función void)
 *           pop rbp
 *           ret
 * Cada return deja su valor en rax y hace la misma salida. Si una función
 * con tipo llega al final sin return, es un error de ejecución con la
 * línea de su definición, como en la versión anterior. */
static void gen_function(Node *f) {
    SymInfo *fi = sym_info(f->sym);
    Node *p;
    int frame_at;

    if (!is_scalar(f->type) && f->type.kind != TY_VOID) {
        not_supported(f->line, "a function returning this type");
        return;
    }
    for (p = f->a; p != NULL; p = p->next)
        if (!is_scalar(p->type)) { not_supported(p->line, "a parameter of this type"); return; }

    cur_level = fi->level;
    frame_size = fi->frame;
    loop_count = 0;

    asm_bind(&a, func_label(f->sym));
    x_push(&a, RBP);
    x_mov_rr(&a, RBP, RSP);
    x_sub_ri(&a, RSP, 0);
    frame_at = a.code.len - 4;

    gen_stmts(f->b->a);
    if (f->type.kind == TY_VOID) {
        gen_epilogue();
    } else {
        char msg[160];
        snprintf(msg, sizeof msg, "function '%s' must return a value of type %s",
                 f->text, type_name(f->type));
        rt_fail(&a, &rt, f->line, msg);
    }

    asm_patch_u32(&a, frame_at, (uint32_t)frame_size);
}

/* Genera el programa completo y escribe el ejecutable en path.
 * Devuelve el número de construcciones no soportadas (0 si se generó). */
int generate_native(Node *program, const char *path) {
    int frame_at;

    asm_init(&a);
    rt_init(&a, &rt);

    /* Posiciones de las variables del programa principal (nivel 0) */
    layout_size = 0;
    layout_stmts(program, 0);
    cur_level = 0;
    frame_size = layout_size;

    /* Prólogo. El tamaño del marco se completa al final. */
    x_push(&a, RBP);
    x_mov_rr(&a, RBP, RSP);
    x_sub_ri(&a, RSP, 0);
    frame_at = a.code.len - 4;

    gen_stmts(program);

    /* Fin del programa: exit(0) */
    x_mov_ri(&a, RDI, 0);
    x_call(&a, rt.exit);
    asm_patch_u32(&a, frame_at, (uint32_t)frame_size);

    /* Funciones: cada una puede agregar a la cola las que define dentro */
    {
        int k;
        for (k = 0; k < pending_count; k++) gen_function(pending[k]);
    }

    if (unsupported > 0) return unsupported;

    rt_emit(&a, &rt);
    asm_finish(&a);
    if (elf_write(path, &a, 0) != 0) {
        perror(path);
        return -1;
    }
    return 0;
}
