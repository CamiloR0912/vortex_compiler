/* ==========================================================================
 * vortex.h - Estructuras compartidas por las etapas del compilador de Vortex
 *
 *   lexer.l + parser.y   texto  -> árbol de sintaxis abstracta (AST)
 *   semantic.c           revisa tipos, ámbitos y sobrecarga; anota el AST
 *   codegen.c            AST anotado -> programa en C
 *   main.c               coordina las etapas y llama a gcc
 * ========================================================================== */
#ifndef VORTEX_H
#define VORTEX_H

#include <stdio.h>

/* ---- Tipos del lenguaje ---- */
typedef enum {
    TY_ERROR,   /* expresión con un error ya reportado: no se reporta de nuevo */
    TY_VOID, TY_INT, TY_FLOAT, TY_BOOL, TY_STRING,
    TY_VECTOR, TY_MATRIX,
    TY_EMPTY    /* el literal []: sirve como cualquier vector o matriz */
} TypeKind;

typedef struct {
    TypeKind kind;
    TypeKind elem;      /* tipo de los elementos (solo vector y matrix) */
} Type;

/* ---- Operadores ---- */
typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_LT, OP_GT, OP_LE, OP_GE, OP_EQ, OP_NE,
    OP_AND, OP_OR, OP_NEG, OP_NOT,
    OP_SUM, OP_PROD, OP_MAX, OP_MIN, OP_LEN
} Op;

/* ---- Nodos del AST ----
 * En cada tipo de nodo se indica qué guardan text y los hijos a, b, c, d. */
typedef enum {
    /* Sentencias */
    N_VARDECL,   /* text: nombre, type: tipo declarado, a: valor inicial     */
    N_ASSIGN,    /* a: destino (variable o indexación), b: valor             */
    N_EXPRSTMT,  /* a: expresión                                             */
    N_BLOCK,     /* a: lista de sentencias                                   */
    N_IF,        /* a: condición, b: bloque, c: else (bloque, if o NULL)     */
    N_WHILE,     /* a: condición, b: bloque                                  */
    N_FOR,       /* a: inicio, b: condición, c: paso, d: bloque              */
    N_SWITCH,    /* a: valor, b: lista de N_CASE                             */
    N_CASE,      /* a: valor del case (NULL si es default), b: bloque        */
    N_FUNC,      /* text: nombre, type: retorno, a: lista de N_PARAM, b: bloque */
    N_PARAM,     /* text: nombre, type: tipo, a: valor por defecto o NULL    */
    N_RETURN,    /* a: valor o NULL                                          */
    N_BREAK, N_CONTINUE,
    N_THROW,     /* a: valor                                                 */
    N_TRY,       /* a: bloque, text: variable del catch, b: bloque del catch */
    N_ASSERT,    /* a: condición, b: mensaje o NULL                          */
    N_PRINT,     /* a: lista de expresiones                                  */
    /* Expresiones */
    N_INT, N_FLOAT, N_STRING, N_BOOL,   /* text: el lexema                   */
    N_VAR,       /* text: nombre                                             */
    N_CALL,      /* text: nombre, a: argumentos; b: lista de N_BOUND         */
    N_NAMED,     /* argumento nombrado. text: nombre, a: valor               */
    N_BOUND,     /* argumento ya ubicado en su parámetro. a: valor, type: tipo del parámetro */
    N_REDUCE,    /* op: OP_SUM..OP_LEN, a: argumento                         */
    N_BINARY,    /* op, a: izquierda, b: derecha                             */
    N_UNARY,     /* op, a: operando                                          */
    N_INDEX,     /* a: base, b: índice                                       */
    N_ARRAY      /* a: lista de elementos                                    */
} NodeKind;

struct Symbol;

typedef struct Node {
    NodeKind kind;
    int line;               /* línea del programa fuente */
    int op;
    Type type;              /* lo llena el parser (declaraciones) o semantic.c (expresiones) */
    char *text;
    struct Node *a, *b, *c, *d;
    struct Node *next;      /* siguiente elemento cuando el nodo está en una lista */
    struct Symbol *sym;     /* variable o función a la que se refiere (semantic.c) */
} Node;

/* ---- Tabla de símbolos ---- */
typedef struct Symbol {
    char *name;
    int id;                 /* número único: distingue variables con el mismo nombre */
    int is_func;
    Type type;              /* tipo de la variable o tipo de retorno de la función */
    Node *def;              /* funciones: su nodo N_FUNC */
    struct Symbol *next;
} Symbol;

/* ---- ast.c ---- */
Node *mk(NodeKind kind, int line, char *text, Node *a, Node *b, Node *c, Node *d);
Node *list_append(Node *list, Node *item);
Type type_of(TypeKind kind, TypeKind elem);
const char *type_name(Type t);
int type_equal(Type a, Type b);
int is_numeric(Type t);                 /* int o float                          */
int is_array(Type t);                   /* vector, matrix o []                  */
int assign_cost(Type from, Type to);    /* 0 exacto, 1 con promoción, -1 no se puede */
Type unify(Type a, Type b);             /* tipo común de dos valores, o TY_ERROR */

/* ---- parser.y ---- */
extern Node *program_root;
int yyparse(void);
void flush_pending(void);

/* ---- lexer.l ---- */
extern FILE *yyin;
extern int num_errors;

/* ---- semantic.c ---- */
void semantic_check(Node *program);

/* ---- codegen.c ---- */
void generate_c(Node *program, FILE *out);

#endif
