/* ==========================================================================
 * ast.c - Construcción del AST y operaciones sobre los tipos de Vortex
 * ========================================================================== */
#include <stdlib.h>
#include <string.h>
#include "vortex.h"

/* Crea un nodo. Los hijos que no apliquen se pasan como NULL. */
Node *mk(NodeKind kind, int line, char *text, Node *a, Node *b, Node *c, Node *d) {
    Node *n = calloc(1, sizeof *n);
    if (n == NULL) { fprintf(stderr, "out of memory\n"); exit(1); }
    n->kind = kind;
    n->line = line;
    n->text = text;
    n->a = a; n->b = b; n->c = c; n->d = d;
    return n;
}

/* Agrega item al final de la lista y devuelve la cabeza.
 * item puede ser NULL (sentencia con error de sintaxis): se ignora. */
Node *list_append(Node *list, Node *item) {
    Node *last = list;
    if (item == NULL) return list;
    if (list == NULL) return item;
    while (last->next != NULL) last = last->next;
    last->next = item;
    return list;
}

Type type_of(TypeKind kind, TypeKind elem) {
    Type t;
    t.kind = kind;
    t.elem = elem;
    return t;
}

static const char *basic_name(TypeKind k) {
    switch (k) {
        case TY_VOID:   return "void";
        case TY_INT:    return "int";
        case TY_FLOAT:  return "float";
        case TY_BOOL:   return "bool";
        case TY_STRING: return "string";
        default:        return "?";
    }
}

/* Nombre del tipo como se escribe en Vortex: "int", "vector<float>", ...
 * Usa varios búferes en rotación para poder llamarse más de una vez dentro
 * del mismo printf. */
const char *type_name(Type t) {
    static char bufs[8][32];
    static int turn = 0;
    char *buf = bufs[turn++ % 8];
    switch (t.kind) {
        case TY_VECTOR: snprintf(buf, 32, "vector<%s>", basic_name(t.elem)); break;
        case TY_MATRIX: snprintf(buf, 32, "matrix<%s>", basic_name(t.elem)); break;
        case TY_EMPTY:  snprintf(buf, 32, "[]"); break;
        case TY_ERROR:  snprintf(buf, 32, "?"); break;
        default:        snprintf(buf, 32, "%s", basic_name(t.kind)); break;
    }
    return buf;
}

int is_numeric(Type t) { return t.kind == TY_INT || t.kind == TY_FLOAT; }

int is_array(Type t) {
    return t.kind == TY_VECTOR || t.kind == TY_MATRIX || t.kind == TY_EMPTY;
}

int type_equal(Type a, Type b) {
    if (a.kind != b.kind) return 0;
    if (a.kind == TY_VECTOR || a.kind == TY_MATRIX) return a.elem == b.elem;
    return 1;
}

/* ¿Un valor de tipo from se puede guardar donde se espera un tipo to?
 *   0  sí, el tipo es exactamente el mismo
 *   1  sí, con promoción: int -> float, vector<int> -> vector<float>, ...
 *  -1  no
 * La cantidad de promociones decide entre funciones sobrecargadas. */
int assign_cost(Type from, Type to) {
    if (from.kind == TY_ERROR || to.kind == TY_ERROR) return 0;
    if (type_equal(from, to)) return 0;
    if (from.kind == TY_INT && to.kind == TY_FLOAT) return 1;
    if (from.kind == TY_EMPTY && (to.kind == TY_VECTOR || to.kind == TY_MATRIX)) return 0;
    if ((from.kind == TY_VECTOR || from.kind == TY_MATRIX) && from.kind == to.kind &&
        from.elem == TY_INT && to.elem == TY_FLOAT) return 1;
    return -1;
}

/* Tipo común de dos valores que se van a comparar o combinar.
 * Devuelve TY_ERROR si no son compatibles. */
Type unify(Type a, Type b) {
    if (type_equal(a, b)) return a;
    if (is_numeric(a) && is_numeric(b)) return type_of(TY_FLOAT, TY_ERROR);
    if (a.kind == TY_EMPTY && is_array(b)) return b;
    if (b.kind == TY_EMPTY && is_array(a)) return a;
    if ((a.kind == TY_VECTOR || a.kind == TY_MATRIX) && a.kind == b.kind) {
        Type ea = type_of(a.elem, TY_ERROR), eb = type_of(b.elem, TY_ERROR);
        if (is_numeric(ea) && is_numeric(eb)) return type_of(a.kind, TY_FLOAT);
    }
    return type_of(TY_ERROR, TY_ERROR);
}
