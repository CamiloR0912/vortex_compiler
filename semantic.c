/* ==========================================================================
 * semantic.c - Análisis semántico del compilador de Vortex
 *
 * Recorre el AST una vez y verifica lo que la gramática no puede expresar:
 *   - que cada variable y función exista y sea visible (ámbitos y shadowing)
 *   - que los tipos de cada operación, asignación y condición sean válidos
 *   - a cuál versión de una función sobrecargada corresponde cada llamada
 *   - que break, continue y return estén donde pueden estar
 *
 * Además anota el AST para el generador de código: el tipo de cada expresión
 * (node->type) y la variable o función a la que se refiere (node->sym).
 *
 * Cada error se reporta como  [Semantic error] line N: mensaje
 * ========================================================================== */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "vortex.h"

#define MAX_ARGS 32     /* argumentos por llamada */
#define MAX_CANDIDATES 64   /* versiones de una misma función */

/* ---- Ámbitos ----
 * Cada bloque abre un ámbito con sus propios símbolos. La búsqueda va del
 * ámbito actual hacia afuera, así una declaración interna oculta a la
 * externa del mismo nombre (shadowing) sin modificarla. */
typedef struct Scope {
    Symbol *symbols;
    struct Scope *parent;
} Scope;

static Scope *scope = NULL;         /* ámbito actual                          */
static Node *current_func = NULL;   /* función que se está revisando, o NULL  */
static int loop_depth = 0;          /* ciclos abiertos dentro de la función   */
static int next_id = 1;             /* para numerar los símbolos              */

static const Type T_ERR = { TY_ERROR, TY_ERROR };

static Type check_expr(Node *n);
static void check_stmt(Node *n);
static void check_stmts(Node *list);

static void sem_error(int line, const char *fmt, ...) {
    va_list ap;
    printf("[Semantic error] line %d: ", line);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    num_errors++;
}

static void push_scope(void) {
    Scope *s = calloc(1, sizeof *s);
    s->parent = scope;
    scope = s;
}

static void pop_scope(void) {
    scope = scope->parent;      /* los símbolos siguen vivos: el AST los referencia */
}

static Symbol *new_symbol(const char *name, int is_func, Type type, Node *def) {
    Symbol *s = calloc(1, sizeof *s);
    s->name = strdup(name);
    s->id = next_id++;
    s->is_func = is_func;
    s->type = type;
    s->def = def;
    s->next = scope->symbols;
    scope->symbols = s;
    return s;
}

/* Declara una variable en el ámbito actual. Repetir el nombre en el MISMO
 * ámbito es un error; repetirlo en un ámbito interno es shadowing. */
static Symbol *declare_var(const char *name, Type type, int line) {
    Symbol *s;
    for (s = scope->symbols; s != NULL; s = s->next)
        if (!s->is_func && strcmp(s->name, name) == 0) {
            sem_error(line, "variable '%s' is already declared in this scope", name);
            break;
        }
    return new_symbol(name, 0, type, NULL);
}

static Symbol *lookup_var(const char *name) {
    Scope *sc;
    Symbol *s;
    for (sc = scope; sc != NULL; sc = sc->parent)
        for (s = sc->symbols; s != NULL; s = s->next)
            if (!s->is_func && strcmp(s->name, name) == 0) return s;
    return NULL;
}

/* Reúne todas las versiones de una función: las del ámbito más cercano que
 * tenga alguna con ese nombre. Devuelve cuántas encontró. */
static int lookup_funcs(const char *name, Symbol **found) {
    Scope *sc;
    Symbol *s;
    int n = 0;
    for (sc = scope; sc != NULL && n == 0; sc = sc->parent)
        for (s = sc->symbols; s != NULL; s = s->next)
            if (s->is_func && strcmp(s->name, name) == 0 && n < MAX_CANDIDATES)
                found[n++] = s;
    return n;
}

/* ¿Dos funciones tienen la misma lista de tipos de parámetros? */
static int same_params(Node *f, Node *g) {
    Node *p = f->a, *q = g->a;
    for (; p != NULL && q != NULL; p = p->next, q = q->next)
        if (!type_equal(p->type, q->type)) return 0;
    return p == NULL && q == NULL;
}

/* Registra una función en el ámbito actual. Convención de sobrecarga: se
 * puede repetir el nombre si cambia la cantidad o el tipo de los parámetros. */
static void declare_func(Node *f) {
    Symbol *s;
    for (s = scope->symbols; s != NULL; s = s->next)
        if (s->is_func && strcmp(s->name, f->text) == 0 && same_params(s->def, f)) {
            sem_error(f->line, "function '%s' is already defined with the same parameter types", f->text);
            break;
        }
    f->sym = new_symbol(f->text, 1, f->type, f);
}

static int is_basic(Type t) {
    return t.kind == TY_INT || t.kind == TY_FLOAT || t.kind == TY_BOOL || t.kind == TY_STRING;
}

static const char *op_text(int op) {
    static const char *names[] = { "+", "-", "*", "/", "%", "<", ">", "<=", ">=", "==", "!=",
                                   "&&", "||", "-", "!", "sum", "prod", "max", "min", "len" };
    return names[op];
}

/* ===================== Expresiones ===================== */

/* Tipo de una operación aritmética (+ - * / %) entre l y r, o TY_ERROR.
 *   escalar con escalar: int si ambos son int; si no, float
 *   arreglo con arreglo o con escalar: elemento a elemento */
static Type arith_type(Type l, Type r) {
    if (is_numeric(l) && is_numeric(r))
        return type_of(l.kind == TY_INT && r.kind == TY_INT ? TY_INT : TY_FLOAT, TY_ERROR);

    if (is_array(l) && is_array(r)) {
        Type u = unify(l, r);
        if (u.kind == TY_EMPTY) return u;
        if (u.kind != TY_ERROR && (u.elem == TY_INT || u.elem == TY_FLOAT)) return u;
        return T_ERR;
    }

    if (is_array(l) && is_numeric(r)) { Type t = l; l = r; r = t; }    /* deja el arreglo en r */
    if (is_numeric(l) && is_array(r)) {
        if (r.kind == TY_EMPTY) return r;
        if (r.elem != TY_INT && r.elem != TY_FLOAT) return T_ERR;
        return type_of(r.kind, l.kind == TY_INT && r.elem == TY_INT ? TY_INT : TY_FLOAT);
    }
    return T_ERR;
}

static Type binary_type(Node *n, Type l, Type r) {
    Type result = T_ERR;
    if (l.kind == TY_ERROR || r.kind == TY_ERROR) return T_ERR;

    switch (n->op) {
        case OP_ADD:
            /* string + cualquier tipo básico: concatenación */
            if ((l.kind == TY_STRING && is_basic(r)) || (r.kind == TY_STRING && is_basic(l))) {
                result = type_of(TY_STRING, TY_ERROR);
                break;
            }
            result = arith_type(l, r);
            break;
        case OP_SUB: case OP_MUL: case OP_DIV:
            result = arith_type(l, r);
            break;
        case OP_MOD:
            /* el residuo solo existe entre enteros */
            result = arith_type(l, r);
            if (result.kind == TY_FLOAT || (is_array(result) && result.elem == TY_FLOAT))
                result = T_ERR;
            break;
        case OP_LT: case OP_GT: case OP_LE: case OP_GE:
            if ((is_numeric(l) && is_numeric(r)) || (l.kind == TY_STRING && r.kind == TY_STRING))
                result = type_of(TY_BOOL, TY_ERROR);
            break;
        case OP_EQ: case OP_NE:
            if (l.kind != TY_VOID && unify(l, r).kind != TY_ERROR)
                result = type_of(TY_BOOL, TY_ERROR);
            break;
        case OP_AND: case OP_OR:
            if (l.kind == TY_BOOL && r.kind == TY_BOOL)
                result = type_of(TY_BOOL, TY_ERROR);
            break;
    }
    if (result.kind == TY_ERROR)
        sem_error(n->line, "operator '%s' cannot be applied to %s and %s",
                  op_text(n->op), type_name(l), type_name(r));
    return result;
}

/* Literal [a, b, c]: todos los elementos deben tener un tipo común.
 *   elementos básicos   -> vector<T>
 *   elementos vector<T> -> matrix<T> (cada elemento es una fila)       */
static Type array_type(Node *n) {
    Type common = T_ERR;
    Node *e;
    int first = 1, failed = 0;

    if (n->a == NULL) return type_of(TY_EMPTY, TY_ERROR);

    for (e = n->a; e != NULL; e = e->next) {
        Type t = check_expr(e);
        if (t.kind == TY_ERROR) { failed = 1; continue; }
        if (first) { common = t; first = 0; continue; }
        if (unify(common, t).kind == TY_ERROR) {
            if (!failed)
                sem_error(e->line, "mixed element types in literal: %s and %s",
                          type_name(common), type_name(t));
            failed = 1;
        } else {
            common = unify(common, t);
        }
    }
    if (failed) return T_ERR;

    if (is_basic(common)) return type_of(TY_VECTOR, common.kind);
    if (common.kind == TY_VECTOR) return type_of(TY_MATRIX, common.elem);
    if (common.kind == TY_EMPTY) {
        sem_error(n->line, "cannot determine the element type of a literal made only of []");
        return T_ERR;
    }
    sem_error(n->line, "%s cannot be an element of a literal", type_name(common));
    return T_ERR;
}

/* Texto de una llamada para los mensajes: describe(bool), mix(int, int) */
static const char *call_text(Node *n, Type *arg_types) {
    static char buf[512];
    Node *a;
    int i = 0;
    size_t len = snprintf(buf, sizeof buf, "%s(", n->text);
    for (a = n->a; a != NULL && len < sizeof buf; a = a->next, i++) {
        if (i > 0) len += snprintf(buf + len, sizeof buf - len, ", ");
        if (a->kind == N_NAMED && len < sizeof buf)
            len += snprintf(buf + len, sizeof buf - len, "%s = ", a->text);
        if (len < sizeof buf)
            len += snprintf(buf + len, sizeof buf - len, "%s", type_name(arg_types[i]));
    }
    if (len < sizeof buf) snprintf(buf + len, sizeof buf - len, ")");
    return buf;
}

/* Intenta ubicar los argumentos de la llamada en los parámetros de func.
 * Posicionales en orden, nombrados por su nombre, y los que falten toman su
 * valor por defecto. Deja en slots[] la expresión de cada parámetro.
 * Devuelve la cantidad de promociones necesarias, o -1 si no encaja. */
static int bind_args(Node *func, Node *call, Type *arg_types, Node **slots) {
    Node *params[MAX_ARGS];
    Node *p, *a;
    int count = 0, cost = 0, positional = 0, i;

    for (p = func->a; p != NULL && count < MAX_ARGS; p = p->next) {
        params[count] = p;
        slots[count++] = NULL;
    }

    for (a = call->a, i = 0; a != NULL; a = a->next, i++) {
        int target = -1, c, k;
        if (a->kind == N_NAMED) {
            for (k = 0; k < count; k++)
                if (strcmp(params[k]->text, a->text) == 0) target = k;
        } else if (positional < count) {
            target = positional++;
        }
        if (target < 0 || slots[target] != NULL) return -1;

        c = assign_cost(arg_types[i], params[target]->type);
        if (c < 0) return -1;
        cost += c;
        slots[target] = (a->kind == N_NAMED) ? a->a : a;
    }

    for (i = 0; i < count; i++) {
        if (slots[i] != NULL) continue;
        if (params[i]->a == NULL) return -1;    /* falta y no tiene valor por defecto */
        slots[i] = params[i]->a;
    }
    return cost;
}

/* Llamada a función: elige la versión que acepta los argumentos con menos
 * promociones. Si ninguna encaja o si hay empate, es un error. */
static Type check_call(Node *n) {
    Type arg_types[MAX_ARGS];
    Symbol *candidates[MAX_CANDIDATES];
    Node *slots[MAX_ARGS], *best_slots[MAX_ARGS];
    Node *a, *p, *bound = NULL;
    int n_args = 0, n_cand, i, k, failed = 0;
    int best = -1, best_cost = 0, ties = 0;

    for (a = n->a; a != NULL; a = a->next) {
        Type t;
        if (n_args >= MAX_ARGS) {
            sem_error(n->line, "too many arguments in call to '%s'", n->text);
            return T_ERR;
        }
        t = check_expr(a->kind == N_NAMED ? a->a : a);
        if (t.kind == TY_VOID) {
            sem_error(a->line, "a void value cannot be passed as an argument");
            t = T_ERR;
        }
        if (t.kind == TY_ERROR) failed = 1;
        arg_types[n_args++] = t;
    }

    n_cand = lookup_funcs(n->text, candidates);
    if (n_cand == 0) {
        sem_error(n->line, "undefined function '%s'", n->text);
        return T_ERR;
    }
    if (failed) return T_ERR;

    for (i = 0; i < n_cand; i++) {
        int cost = bind_args(candidates[i]->def, n, arg_types, slots);
        if (cost < 0) continue;
        if (best < 0 || cost < best_cost) {
            best = i; best_cost = cost; ties = 1;
            memcpy(best_slots, slots, sizeof slots);
        } else if (cost == best_cost) {
            ties++;
        }
    }
    if (best < 0) {
        sem_error(n->line, "no version of '%s' matches the call %s", n->text, call_text(n, arg_types));
        return T_ERR;
    }
    if (ties > 1) {
        sem_error(n->line, "ambiguous call %s", call_text(n, arg_types));
        return T_ERR;
    }

    /* Deja los argumentos en el orden de los parámetros para codegen.c */
    n->sym = candidates[best];
    for (p = n->sym->def->a, k = 0; p != NULL; p = p->next, k++) {
        Node *b = mk(N_BOUND, n->line, NULL, best_slots[k], NULL, NULL, NULL);
        b->type = p->type;
        bound = list_append(bound, b);
    }
    n->b = bound;
    return n->sym->type;
}

static Type check_index(Node *n) {
    Type base = check_expr(n->a);
    Type index = check_expr(n->b);
    if (index.kind != TY_ERROR && index.kind != TY_INT)
        sem_error(n->b->line, "index must be int, got %s", type_name(index));
    if (base.kind == TY_ERROR) return T_ERR;
    if (base.kind == TY_VECTOR) return type_of(base.elem, TY_ERROR);
    if (base.kind == TY_MATRIX) return type_of(TY_VECTOR, base.elem);     /* una fila */
    sem_error(n->line, "%s cannot be indexed", type_name(base));
    return T_ERR;
}

static Type check_reduce(Node *n) {
    Type arg = check_expr(n->a);
    if (arg.kind == TY_ERROR) return T_ERR;

    if (n->op == OP_LEN) {
        if (is_array(arg) || arg.kind == TY_STRING) return type_of(TY_INT, TY_ERROR);
        sem_error(n->line, "len requires a vector, matrix or string, got %s", type_name(arg));
        return T_ERR;
    }
    if (arg.kind == TY_EMPTY) return type_of(TY_INT, TY_ERROR);
    if ((arg.kind == TY_VECTOR || arg.kind == TY_MATRIX) &&
        (arg.elem == TY_INT || arg.elem == TY_FLOAT))
        return type_of(arg.elem, TY_ERROR);
    sem_error(n->line, "%s requires a vector or matrix of numbers, got %s",
              op_text(n->op), type_name(arg));
    return T_ERR;
}

static Type check_unary(Node *n) {
    Type t = check_expr(n->a);
    if (t.kind == TY_ERROR) return T_ERR;
    if (n->op == OP_NOT && t.kind == TY_BOOL) return t;
    if (n->op == OP_NEG) {
        if (is_numeric(t) || t.kind == TY_EMPTY) return t;
        if (is_array(t) && (t.elem == TY_INT || t.elem == TY_FLOAT)) return t;
    }
    sem_error(n->line, "operator '%s' cannot be applied to %s", op_text(n->op), type_name(t));
    return T_ERR;
}

/* Calcula el tipo de una expresión, lo guarda en el nodo y lo devuelve */
static Type check_expr(Node *n) {
    Type t = T_ERR;
    switch (n->kind) {
        case N_INT:    t = type_of(TY_INT, TY_ERROR); break;
        case N_FLOAT:  t = type_of(TY_FLOAT, TY_ERROR); break;
        case N_STRING: t = type_of(TY_STRING, TY_ERROR); break;
        case N_BOOL:   t = type_of(TY_BOOL, TY_ERROR); break;
        case N_VAR:
            n->sym = lookup_var(n->text);
            if (n->sym == NULL) sem_error(n->line, "undefined variable '%s'", n->text);
            else t = n->sym->type;
            break;
        case N_BINARY: {
            Type l = check_expr(n->a);
            Type r = check_expr(n->b);
            t = binary_type(n, l, r);
            break;
        }
        case N_UNARY:  t = check_unary(n); break;
        case N_INDEX:  t = check_index(n); break;
        case N_CALL:   t = check_call(n); break;
        case N_REDUCE: t = check_reduce(n); break;
        case N_ARRAY:  t = array_type(n); break;
        default: break;
    }
    n->type = t;
    return t;
}

/* ===================== Sentencias ===================== */

/* Revisa que value se pueda guardar en un destino de tipo target */
static void check_assignable(Type value, Type target, int line) {
    if (value.kind == TY_ERROR || target.kind == TY_ERROR) return;
    if (assign_cost(value, target) < 0)
        sem_error(line, "type mismatch: cannot assign %s to %s", type_name(value), type_name(target));
}

/* Las condiciones deben ser bool: no hay conversión implícita desde int */
static void check_condition(Node *cond, const char *what) {
    Type t;
    if (cond == NULL) return;
    t = check_expr(cond);
    if (t.kind != TY_ERROR && t.kind != TY_BOOL)
        sem_error(cond->line, "%s condition requires bool, got %s", what, type_name(t));
}

/* Destino de una asignación. Formas válidas:  x   v[i]   m[i]   m[i][j]  */
static Type check_target(Node *target) {
    Type t = check_expr(target);
    Node *base = target;
    int levels = 0;
    while (base->kind == N_INDEX) { base = base->a; levels++; }
    if (base->kind != N_VAR || levels > 2) {
        sem_error(target->line, "invalid assignment target");
        return T_ERR;
    }
    return t;
}

static void check_block(Node *block) {
    push_scope();
    check_stmts(block->a);
    pop_scope();
}

/* Definición de función. Los parámetros y el cuerpo comparten un ámbito
 * nuevo; un parámetro puede ocultar una variable externa del mismo nombre. */
static void check_func(Node *f) {
    Node *saved_func = current_func;
    int saved_loops = loop_depth;
    Node *p;

    /* Los valores por defecto se evalúan fuera de la función */
    for (p = f->a; p != NULL; p = p->next)
        if (p->a != NULL) check_assignable(check_expr(p->a), p->type, p->line);

    push_scope();
    current_func = f;
    loop_depth = 0;
    for (p = f->a; p != NULL; p = p->next)
        p->sym = declare_var(p->text, p->type, p->line);
    check_stmts(f->b->a);
    pop_scope();

    current_func = saved_func;
    loop_depth = saved_loops;
}

static void check_return(Node *n) {
    Type expected;
    if (current_func == NULL) {
        sem_error(n->line, "'return' outside of a function");
        if (n->a != NULL) check_expr(n->a);
        return;
    }
    expected = current_func->type;
    n->type = expected;
    if (n->a == NULL) {
        if (expected.kind != TY_VOID)
            sem_error(n->line, "function '%s' must return a value of type %s",
                      current_func->text, type_name(expected));
        return;
    }
    {
        Type t = check_expr(n->a);
        if (expected.kind == TY_VOID)
            sem_error(n->line, "void function '%s' cannot return a value", current_func->text);
        else if (t.kind != TY_ERROR && assign_cost(t, expected) < 0)
            sem_error(n->line, "type mismatch: function '%s' returns %s, got %s",
                      current_func->text, type_name(expected), type_name(t));
    }
}

static void check_switch(Node *n) {
    Type subject = check_expr(n->a);
    Node *c;
    if (subject.kind != TY_ERROR && !is_basic(subject)) {
        sem_error(n->a->line, "switch requires int, float, bool or string, got %s", type_name(subject));
        subject = T_ERR;
    }
    for (c = n->b; c != NULL; c = c->next) {
        if (c->a != NULL) {
            Type t = check_expr(c->a);
            if (t.kind != TY_ERROR && subject.kind != TY_ERROR && unify(subject, t).kind == TY_ERROR)
                sem_error(c->line, "case of type %s does not match switch of type %s",
                          type_name(t), type_name(subject));
        }
        check_block(c->b);
    }
}

static void check_stmt(Node *n) {
    switch (n->kind) {
        case N_VARDECL: {
            /* Primero el valor y después la declaración: en  int x = x * 10;
             * la x de la derecha es la del ámbito externo */
            Type t = check_expr(n->a);
            if (t.kind == TY_VOID) {
                sem_error(n->line, "cannot initialize '%s' with a void value", n->text);
                t = T_ERR;
            }
            check_assignable(t, n->type, n->line);
            n->sym = declare_var(n->text, n->type, n->line);
            break;
        }
        case N_ASSIGN: {
            Type target = check_target(n->a);
            Type value = check_expr(n->b);
            check_assignable(value, target, n->line);
            break;
        }
        case N_EXPRSTMT:
            check_expr(n->a);
            break;
        case N_BLOCK:
            check_block(n);
            break;
        case N_IF:
            check_condition(n->a, "if");
            check_block(n->b);
            if (n->c != NULL) check_stmt(n->c);
            break;
        case N_WHILE:
            check_condition(n->a, "while");
            loop_depth++;
            check_block(n->b);
            loop_depth--;
            break;
        case N_FOR:
            push_scope();       /* la variable del for solo existe dentro del ciclo */
            if (n->a != NULL) check_stmt(n->a);
            check_condition(n->b, "for");
            if (n->c != NULL) check_stmt(n->c);
            loop_depth++;
            check_block(n->d);
            loop_depth--;
            pop_scope();
            break;
        case N_SWITCH:
            check_switch(n);
            break;
        case N_FUNC:
            check_func(n);
            break;
        case N_RETURN:
            check_return(n);
            break;
        case N_BREAK:
        case N_CONTINUE:
            if (loop_depth == 0)
                sem_error(n->line, "'%s' outside of a loop", n->kind == N_BREAK ? "break" : "continue");
            break;
        case N_THROW: {
            Type t = check_expr(n->a);
            if (t.kind != TY_ERROR && !is_basic(t))
                sem_error(n->line, "throw requires int, float, bool or string, got %s", type_name(t));
            break;
        }
        case N_TRY:
            check_block(n->a);
            push_scope();       /* la variable del catch es un string local */
            n->sym = declare_var(n->text, type_of(TY_STRING, TY_ERROR), n->line);
            check_block(n->b);
            pop_scope();
            break;
        case N_ASSERT:
            check_condition(n->a, "assert");
            if (n->b != NULL) {
                Type t = check_expr(n->b);
                if (t.kind != TY_ERROR && t.kind != TY_STRING)
                    sem_error(n->b->line, "assert message must be string, got %s", type_name(t));
            }
            break;
        case N_PRINT: {
            Node *e;
            for (e = n->a; e != NULL; e = e->next)
                if (check_expr(e).kind == TY_VOID)
                    sem_error(e->line, "cannot print a void value");
            break;
        }
        default:
            break;
    }
}

/* Revisa una lista de sentencias. Primero registra todas las funciones de
 * la lista: así una función puede llamar a otra definida más abajo
 * (recursión mutua) o a sí misma. */
static void check_stmts(Node *list) {
    Node *n;
    for (n = list; n != NULL; n = n->next)
        if (n->kind == N_FUNC) declare_func(n);
    for (n = list; n != NULL; n = n->next)
        check_stmt(n);
}

void semantic_check(Node *program) {
    push_scope();       /* ámbito global */
    check_stmts(program);
    pop_scope();
}
