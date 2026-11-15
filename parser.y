/* ==========================================================================
 * parser.y - Analizador sintáctico del compilador de Vortex (.vx)
 *
 * Usa la misma gramática y las mismas reglas de error del punto 4. La
 * diferencia es que cada regla construye un nodo del árbol de sintaxis
 * abstracta (AST); el árbol completo queda en program_root y lo recorren
 * semantic.c y codegen.c.
 *
 * Errores que reporta, con su línea:
 *   [Invalid construct]  constructo reconocido pero mal formado
 *   [Syntax error]       cualquier otro error de sintaxis
 * ========================================================================== */

/* Va también a parser.tab.h: %union usa los tipos Node y Type */
%code requires {
#include "vortex.h"
}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vortex.h"

int yylex(void);
void yyerror(const char *s);

Node *program_root = NULL;      /* el AST del programa completo */

/* ---- Error de sintaxis pendiente ----------------------------------------
 * Bison llama a yyerror() ANTES de saber qué regla de error va a recuperar
 * el análisis. Por eso yyerror() no imprime: guarda el mensaje. Luego la
 * regla que recupera decide cómo se reporta:
 *   - una regla específica  -> [Invalid construct] (invalid_from_error)
 *   - la regla genérica     -> [Syntax error]      (flush_pending)        */
static char pending_msg[256];
static int  pending_line = 0;
static int  has_pending  = 0;

/* Imprime el error pendiente como error de sintaxis genérico */
void flush_pending(void) {
    if (!has_pending) return;
    printf("[Syntax error] line %d: %s\n", pending_line, pending_msg);
    num_errors++;
    has_pending = 0;
}

/* Constructo inválido detectado por una regla SIN el token error */
static void invalid_at(int line, const char *msg) {
    flush_pending();
    printf("[Invalid construct] line %d: %s\n", line, msg);
    num_errors++;
}

/* Constructo inválido detectado por una regla CON el token error: agrega el
 * detalle de Bison ("unexpected X, expecting Y") entre paréntesis */
static void invalid_from_error(const char *msg) {
    const char *detail = pending_msg;
    if (!has_pending) return;                 /* ya se reportó este error */
    if (strncmp(detail, "syntax error, ", 14) == 0) detail += 14;
    printf("[Invalid construct] line %d: %s (%s)\n", pending_line, msg, detail);
    num_errors++;
    has_pending = 0;
}

/* Igual que invalid_at, pero con el nombre de un identificador en el mensaje */
static void invalid_name(int line, const char *fmt, const char *name) {
    char msg[256];
    snprintf(msg, sizeof msg, fmt, name);
    invalid_at(line, msg);
}

/* ¿Se puede asignar a esta expresión? Solo a una variable o a v[i], m[i][j] */
static int is_lvalue(Node *n) {
    while (n != NULL && n->kind == N_INDEX) n = n->a;
    return n != NULL && n->kind == N_VAR;
}

/* ¿La lista de argumentos ya tiene alguno nombrado? */
static int has_named(Node *args) {
    for (; args != NULL; args = args->next)
        if (args->kind == N_NAMED) return 1;
    return 0;
}

/* Nodo de operador binario o unario */
static Node *binary(Op op, Node *l, Node *r, int line) {
    Node *n = mk(N_BINARY, line, NULL, l, r, NULL, NULL);
    n->op = op;
    return n;
}
static Node *unary(Op op, Node *e, int line) {
    Node *n = mk(N_UNARY, line, NULL, e, NULL, NULL, NULL);
    n->op = op;
    return n;
}
%}

%define parse.error verbose
%locations

%union {
    char *str;      /* lexema de un identificador o literal */
    Node *node;     /* subárbol                             */
    Type  type;     /* tipo declarado                       */
    int   op;       /* operador de reducción                */
}

/* ---- Tokens (el texto entre comillas es el nombre en los mensajes) ---- */
%token T_INT "int" T_FLOAT "float" T_BOOL "bool" T_STRING "string"
%token T_VOID "void" T_VECTOR "vector" T_MATRIX "matrix"

%token FUNC "func" IF "if" ELSE "else" WHILE "while" FOR "for"
%token SWITCH "switch" CASE "case" DEFAULT "default"
%token BREAK "break" CONTINUE "continue" RETURN "return"
%token TRY "try" CATCH "catch" THROW "throw" ASSERT "assert" PRINT "print"

%token SUM "sum" PROD "prod" MAX "max" MIN "min" LEN "len"

%token <str> INT_LIT "integer literal" FLOAT_LIT "float literal"
%token <str> STRING_LIT "string literal" BOOL_LIT "boolean literal"
%token <str> IDENTIFIER "identifier"

%token EQ "==" NE "!=" LE "<=" GE ">=" AND "&&" OR "||"

%type <node> stmt_list stmt block var_decl simple_stmt condition
%type <node> if_stmt while_stmt for_stmt for_init for_step expr_opt
%type <node> switch_stmt case_list default_opt
%type <node> func_def params_opt param_list param
%type <node> try_stmt assert_stmt jump_stmt print_stmt invalid_stmt
%type <node> expr or_expr and_expr eq_expr rel_expr add_expr mul_expr
%type <node> unary postfix primary expr_list_opt expr_list args_opt arg_list arg
%type <type> basic_type type return_type
%type <op>   reduction

%destructor { free($$); } <str>

/* Conflictos desplazar/reducir de else y catch: gana desplazar (ver punto 4) */
%nonassoc NO_ELSE
%nonassoc ELSE
%nonassoc NO_CATCH
%nonassoc CATCH

%start program

%%

/* ===================== Programa y sentencias ===================== */

program:
    stmt_list                       { program_root = $1; }
    ;

stmt_list:
    %empty                          { $$ = NULL; }
    | stmt_list stmt                { $$ = list_append($1, $2); }
    ;

stmt:
    var_decl
    | simple_stmt ';'
    | block
    | if_stmt
    | while_stmt
    | for_stmt
    | switch_stmt
    | func_def
    | try_stmt
    | jump_stmt
    | assert_stmt
    | print_stmt
    | invalid_stmt
    /* Recuperación genérica: salta hasta el siguiente ';' o bloque */
    | error ';'                     { flush_pending(); yyerrok; $$ = NULL; }
    | error block                   { flush_pending(); yyerrok; $$ = NULL; }
    ;

block:
    '{' stmt_list '}'               { $$ = mk(N_BLOCK, @1.first_line, NULL, $2, NULL, NULL, NULL); }
    ;

/* ===================== Tipos y declaraciones ===================== */

basic_type:
    T_INT                           { $$ = type_of(TY_INT, TY_ERROR); }
    | T_FLOAT                       { $$ = type_of(TY_FLOAT, TY_ERROR); }
    | T_BOOL                        { $$ = type_of(TY_BOOL, TY_ERROR); }
    | T_STRING                      { $$ = type_of(TY_STRING, TY_ERROR); }
    ;

type:
    basic_type
    | T_VECTOR '<' basic_type '>'   { $$ = type_of(TY_VECTOR, $3.kind); }
    | T_MATRIX '<' basic_type '>'   { $$ = type_of(TY_MATRIX, $3.kind); }
    ;

var_decl:
    type IDENTIFIER '=' expr ';'
        { $$ = mk(N_VARDECL, @2.first_line, $2, $4, NULL, NULL, NULL); $$->type = $1; }
    /* Reglas de error */
    | type IDENTIFIER ';'
        { invalid_name(@2.first_line, "variable '%s' must be initialized", $2); free($2); $$ = NULL; }
    | T_VECTOR IDENTIFIER '=' expr ';'
        { invalid_at(@1.first_line, "'vector' requires an element type, e.g. vector<int>"); free($2); $$ = NULL; }
    | T_MATRIX IDENTIFIER '=' expr ';'
        { invalid_at(@1.first_line, "'matrix' requires an element type, e.g. matrix<int>"); free($2); $$ = NULL; }
    ;

/* Asignación o expresión suelta (factor común: ambas empiezan con expr) */
simple_stmt:
    expr
        { $$ = mk(N_EXPRSTMT, @1.first_line, NULL, $1, NULL, NULL, NULL); }
    | expr '=' expr
        { if (!is_lvalue($1)) invalid_at(@1.first_line, "invalid assignment target");
          $$ = mk(N_ASSIGN, @1.first_line, NULL, $1, $3, NULL, NULL); }
    ;

/* ===================== Control de flujo ===================== */

condition:
    '(' expr ')'                    { $$ = $2; }
    /* Reglas de error */
    | '(' expr '=' expr ')'
        { invalid_at(@3.first_line, "assignment '=' used as a condition; use '==' to compare"); $$ = NULL; }
    | '(' error ')'
        { invalid_from_error("invalid condition"); yyerrok; $$ = NULL; }
    ;

if_stmt:
    IF condition block %prec NO_ELSE
        { $$ = mk(N_IF, @1.first_line, NULL, $2, $3, NULL, NULL); }
    | IF condition block ELSE block
        { $$ = mk(N_IF, @1.first_line, NULL, $2, $3, $5, NULL); }
    | IF condition block ELSE if_stmt
        { $$ = mk(N_IF, @1.first_line, NULL, $2, $3, $5, NULL); }
    /* Reglas de error */
    | IF error
        { invalid_from_error("condition must be enclosed in parentheses"); }
      block
        { yyerrok; $$ = NULL; }
    | IF condition error ';'
        { invalid_from_error("body must be enclosed in braces { }"); yyerrok; $$ = NULL; }
    | IF condition block ELSE error ';'
        { invalid_from_error("body must be enclosed in braces { }"); yyerrok; $$ = NULL; }
    ;

while_stmt:
    WHILE condition block
        { $$ = mk(N_WHILE, @1.first_line, NULL, $2, $3, NULL, NULL); }
    /* Reglas de error */
    | WHILE error
        { invalid_from_error("condition must be enclosed in parentheses"); }
      block
        { yyerrok; $$ = NULL; }
    | WHILE condition error ';'
        { invalid_from_error("body must be enclosed in braces { }"); yyerrok; $$ = NULL; }
    ;

/* for (inicio; condición; paso) - las tres partes son opcionales */
for_stmt:
    FOR '(' for_init ';' expr_opt ';' for_step ')' block
        { $$ = mk(N_FOR, @1.first_line, NULL, $3, $5, $7, $9); }
    /* Reglas de error */
    | FOR '(' error ')'
        { invalid_from_error("malformed 'for' header, expected for (init; condition; step)"); yyerrok; }
      block
        { $$ = NULL; }
    | FOR '(' for_init ';' expr_opt ';' for_step ')' error ';'
        { invalid_from_error("body must be enclosed in braces { }"); yyerrok; $$ = NULL; }
    ;

for_init:
    %empty                          { $$ = NULL; }
    | type IDENTIFIER '=' expr
        { $$ = mk(N_VARDECL, @2.first_line, $2, $4, NULL, NULL, NULL); $$->type = $1; }
    | simple_stmt
    ;

for_step:
    %empty                          { $$ = NULL; }
    | simple_stmt
    ;

expr_opt:
    %empty                          { $$ = NULL; }
    | expr
    ;

/* switch: cero o más case y, al final, un default opcional */
switch_stmt:
    SWITCH condition '{' case_list default_opt '}'
        { $$ = mk(N_SWITCH, @1.first_line, NULL, $2, list_append($4, $5), NULL, NULL); }
    /* Regla de error: algo inválido dentro del switch */
    | SWITCH condition '{' case_list default_opt switch_error '}'
        { yyerrok; $$ = NULL; }
    ;

switch_error:
    error block                             { flush_pending(); }
    | error ';'                             { flush_pending(); }
    | switch_error CASE expr ':' block
    | switch_error DEFAULT ':' block
    ;

case_list:
    %empty                          { $$ = NULL; }
    | case_list CASE expr ':' block
        { $$ = list_append($1, mk(N_CASE, @2.first_line, NULL, $3, $5, NULL, NULL)); }
    ;

default_opt:
    %empty                          { $$ = NULL; }
    | DEFAULT ':' block
        { $$ = mk(N_CASE, @1.first_line, NULL, NULL, $3, NULL, NULL); }
    ;

/* ===================== Funciones ===================== */

func_def:
    FUNC return_type IDENTIFIER '(' params_opt ')' block
        { $$ = mk(N_FUNC, @3.first_line, $3, $5, $7, NULL, NULL); $$->type = $2; }
    /* Regla de error */
    | FUNC return_type IDENTIFIER '(' params_opt ')' ';'
        { invalid_name(@3.first_line, "function '%s' has no body; use { }", $3); free($3); $$ = NULL; }
    ;

return_type:
    type
    | T_VOID                        { $$ = type_of(TY_VOID, TY_ERROR); }
    ;

params_opt:
    %empty                          { $$ = NULL; }
    | param_list
    ;

param_list:
    param
    | param_list ',' param          { $$ = list_append($1, $3); }
    ;

param:
    type IDENTIFIER
        { $$ = mk(N_PARAM, @2.first_line, $2, NULL, NULL, NULL, NULL); $$->type = $1; }
    | type IDENTIFIER '=' expr
        { $$ = mk(N_PARAM, @2.first_line, $2, $4, NULL, NULL, NULL); $$->type = $1; }
    ;

/* ===================== Excepciones y validación ===================== */

try_stmt:
    TRY block CATCH '(' IDENTIFIER ')' block
        { $$ = mk(N_TRY, @1.first_line, $5, $2, $7, NULL, NULL); }
    /* Regla de error */
    | TRY block %prec NO_CATCH
        { invalid_at(@1.first_line, "'try' without 'catch'"); $$ = NULL; }
    ;

assert_stmt:
    ASSERT '(' expr ')' ';'
        { $$ = mk(N_ASSERT, @1.first_line, NULL, $3, NULL, NULL, NULL); }
    | ASSERT '(' expr ',' expr ')' ';'
        { $$ = mk(N_ASSERT, @1.first_line, NULL, $3, $5, NULL, NULL); }
    /* Regla de error */
    | ASSERT '(' ')' ';'
        { invalid_at(@1.first_line, "'assert' requires a condition"); $$ = NULL; }
    ;

jump_stmt:
    RETURN ';'              { $$ = mk(N_RETURN, @1.first_line, NULL, NULL, NULL, NULL, NULL); }
    | RETURN expr ';'       { $$ = mk(N_RETURN, @1.first_line, NULL, $2, NULL, NULL, NULL); }
    | BREAK ';'             { $$ = mk(N_BREAK, @1.first_line, NULL, NULL, NULL, NULL, NULL); }
    | CONTINUE ';'          { $$ = mk(N_CONTINUE, @1.first_line, NULL, NULL, NULL, NULL, NULL); }
    | THROW expr ';'        { $$ = mk(N_THROW, @1.first_line, NULL, $2, NULL, NULL, NULL); }
    ;

print_stmt:
    PRINT '(' expr_list_opt ')' ';'
        { $$ = mk(N_PRINT, @1.first_line, NULL, $3, NULL, NULL, NULL); }
    ;

/* Palabras reservadas fuera de su estructura: mensaje específico */
invalid_stmt:
    ELSE block
        { invalid_at(@1.first_line, "'else' without a matching 'if'"); $$ = NULL; }
    | ELSE if_stmt
        { invalid_at(@1.first_line, "'else' without a matching 'if'"); $$ = NULL; }
    | CASE expr ':' block
        { invalid_at(@1.first_line, "'case' outside of a 'switch'"); $$ = NULL; }
    | DEFAULT ':' block
        { invalid_at(@1.first_line, "'default' outside of a 'switch'"); $$ = NULL; }
    | CATCH '(' IDENTIFIER ')' block
        { invalid_at(@1.first_line, "'catch' without a preceding 'try'"); free($3); $$ = NULL; }
    ;

/* ===================== Expresiones =====================
 * Un no terminal por nivel de precedencia, de menor a mayor:
 *   ||  <  &&  <  == !=  <  < > <= >=  <  + -  <  * / %  <  - ! (unarios)
 */

expr:
    or_expr
    ;

or_expr:
    and_expr
    | or_expr OR and_expr           { $$ = binary(OP_OR, $1, $3, @2.first_line); }
    ;

and_expr:
    eq_expr
    | and_expr AND eq_expr          { $$ = binary(OP_AND, $1, $3, @2.first_line); }
    ;

eq_expr:
    rel_expr
    | eq_expr EQ rel_expr           { $$ = binary(OP_EQ, $1, $3, @2.first_line); }
    | eq_expr NE rel_expr           { $$ = binary(OP_NE, $1, $3, @2.first_line); }
    ;

rel_expr:
    add_expr
    | rel_expr '<' add_expr         { $$ = binary(OP_LT, $1, $3, @2.first_line); }
    | rel_expr '>' add_expr         { $$ = binary(OP_GT, $1, $3, @2.first_line); }
    | rel_expr LE add_expr          { $$ = binary(OP_LE, $1, $3, @2.first_line); }
    | rel_expr GE add_expr          { $$ = binary(OP_GE, $1, $3, @2.first_line); }
    ;

add_expr:
    mul_expr
    | add_expr '+' mul_expr         { $$ = binary(OP_ADD, $1, $3, @2.first_line); }
    | add_expr '-' mul_expr         { $$ = binary(OP_SUB, $1, $3, @2.first_line); }
    ;

mul_expr:
    unary
    | mul_expr '*' unary            { $$ = binary(OP_MUL, $1, $3, @2.first_line); }
    | mul_expr '/' unary            { $$ = binary(OP_DIV, $1, $3, @2.first_line); }
    | mul_expr '%' unary            { $$ = binary(OP_MOD, $1, $3, @2.first_line); }
    ;

unary:
    postfix
    | '-' unary                     { $$ = unary(OP_NEG, $2, @1.first_line); }
    | '!' unary                     { $$ = unary(OP_NOT, $2, @1.first_line); }
    ;

postfix:
    primary
    | postfix '[' expr ']'          { $$ = mk(N_INDEX, @2.first_line, NULL, $1, $3, NULL, NULL); }
    ;

primary:
    INT_LIT                         { $$ = mk(N_INT, @1.first_line, $1, NULL, NULL, NULL, NULL); }
    | FLOAT_LIT                     { $$ = mk(N_FLOAT, @1.first_line, $1, NULL, NULL, NULL, NULL); }
    | STRING_LIT                    { $$ = mk(N_STRING, @1.first_line, $1, NULL, NULL, NULL, NULL); }
    | BOOL_LIT                      { $$ = mk(N_BOOL, @1.first_line, $1, NULL, NULL, NULL, NULL); }
    | IDENTIFIER                    { $$ = mk(N_VAR, @1.first_line, $1, NULL, NULL, NULL, NULL); }
    | IDENTIFIER '(' args_opt ')'   { $$ = mk(N_CALL, @1.first_line, $1, $3, NULL, NULL, NULL); }
    | reduction '(' expr ')'
        { $$ = mk(N_REDUCE, @1.first_line, NULL, $3, NULL, NULL, NULL); $$->op = $1; }
    | '(' expr ')'                  { $$ = $2; }
    | '[' ']'                       { $$ = mk(N_ARRAY, @1.first_line, NULL, NULL, NULL, NULL, NULL); }
    | '[' expr_list ']'             { $$ = mk(N_ARRAY, @1.first_line, NULL, $2, NULL, NULL, NULL); }
    ;

reduction:
    SUM         { $$ = OP_SUM; }
    | PROD      { $$ = OP_PROD; }
    | MAX       { $$ = OP_MAX; }
    | MIN       { $$ = OP_MIN; }
    | LEN       { $$ = OP_LEN; }
    ;

expr_list_opt:
    %empty                          { $$ = NULL; }
    | expr_list
    ;

expr_list:
    expr
    | expr_list ',' expr            { $$ = list_append($1, $3); }
    ;

/* Argumentos: primero los posicionales, luego los nombrados */
args_opt:
    %empty                          { $$ = NULL; }
    | arg_list
    ;

arg_list:
    arg
    | arg_list ',' arg
        { if (has_named($1) && $3->kind != N_NAMED)
              invalid_at(@3.first_line, "positional argument after a named argument");
          $$ = list_append($1, $3); }
    ;

arg:
    expr
    | IDENTIFIER '=' expr
        { $$ = mk(N_NAMED, @1.first_line, $1, $3, NULL, NULL, NULL); }
    ;

%%

/* Bison la llama al detectar un error de sintaxis. Solo guarda el mensaje y
 * la línea; lo imprime la regla que recupere el análisis. */
void yyerror(const char *s) {
    flush_pending();
    snprintf(pending_msg, sizeof pending_msg, "%s", s);
    pending_line = yylloc.first_line;
    has_pending = 1;
}
