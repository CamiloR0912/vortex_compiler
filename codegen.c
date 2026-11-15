/* ==========================================================================
 * codegen.c - Generación de código: AST anotado -> programa en C
 *
 * Recorre el AST que semantic.c ya verificó y escribe un programa en C
 * equivalente, que usa la biblioteca vx_runtime.h. Decisiones principales:
 *
 *   - Todo el programa Vortex queda dentro de main() de C. Las funciones de
 *     Vortex se generan como funciones anidadas de C (extensión de GCC), que
 *     ven las variables de la función que las contiene. Así las funciones
 *     anidadas de Vortex se traducen directamente.
 *   - Cada variable recibe un nombre único (v_nombre_N). Con eso el shadowing
 *     y la sobrecarga no dependen de las reglas de nombres de C.
 *   - int es long long, float es double, bool es int, string es const char*,
 *     y vector/matrix son la estructura VxArr.
 *   - try/catch se traduce a setjmp/longjmp sobre una pila de manejadores.
 * ========================================================================== */
#include <stdlib.h>
#include <string.h>
#include "vortex.h"

static FILE *out;
static int level = 0;           /* nivel de indentación del código generado */
static int next_temp = 1;       /* para nombrar variables temporales        */

/* Bloques try abiertos en la función que se está generando. Al salir con
 * break, continue o return hay que cerrar los que queden atrás. */
static int try_depth = 0;
static int loop_base[256];      /* try_depth al entrar a cada ciclo abierto */
static int loop_count = 0;

static void gen_expr(Node *n);
static void gen_stmt(Node *n);
static void gen_stmts(Node *list);

static void indent(void) {
    int i;
    for (i = 0; i < level; i++) fputs("    ", out);
}

/* ---- Correspondencia de tipos Vortex -> C ---- */

static const char *c_type(Type t) {
    switch (t.kind) {
        case TY_INT:    return "long long";
        case TY_FLOAT:  return "double";
        case TY_BOOL:   return "int";
        case TY_STRING: return "const char *";
        case TY_VOID:   return "void";
        default:        return "VxArr";
    }
}

/* Campo de VxVal y constante de la biblioteca para un tipo de elemento */
static const char *field(TypeKind elem) {
    return elem == TY_FLOAT ? "f" : elem == TY_BOOL ? "b" : elem == TY_STRING ? "s" : "i";
}
static const char *kind_const(TypeKind elem) {
    return elem == TY_FLOAT ? "VX_FLOAT" : elem == TY_BOOL ? "VX_BOOL"
         : elem == TY_STRING ? "VX_STR" : "VX_INT";
}

static void var_name(Symbol *s)  { fprintf(out, "v_%s_%d", s->name, s->id); }
static void func_name(Symbol *s) { fprintf(out, "f_%s_%d", s->name, s->id); }

/* ===================== Expresiones ===================== */

/* Genera n convertida al tipo to cuando hace falta una promoción */
static void gen_conv(Node *n, Type to) {
    Type from = n->type;
    if (from.kind == TY_EMPTY) {
        fputs("vx_empty()", out);
    } else if (from.kind == TY_INT && to.kind == TY_FLOAT) {
        fputs("((double)(", out); gen_expr(n); fputs("))", out);
    } else if ((to.kind == TY_VECTOR || to.kind == TY_MATRIX) &&
               from.elem == TY_INT && to.elem == TY_FLOAT) {
        fputs("vx_to_float(", out); gen_expr(n); fputs(")", out);
    } else {
        gen_expr(n);
    }
}

/* Como gen_conv, pero para un valor que se va a guardar (variable, argumento,
 * retorno). Los arreglos se guardan por valor: si n es una variable, se copia
 * para que el destino no comparta memoria con ella. */
static void gen_store(Node *n, Type to) {
    int is_arr = n->type.kind == TY_VECTOR || n->type.kind == TY_MATRIX;
    int converts = n->type.elem == TY_INT && to.elem == TY_FLOAT;
    if (n->kind == N_VAR && is_arr && !converts) {
        fputs("vx_copy(", out); gen_expr(n); fputs(")", out);
    } else {
        gen_conv(n, to);
    }
}

/* Genera n como cadena, para la concatenación con + */
static void gen_as_string(Node *n) {
    const char *fn = n->type.kind == TY_INT ? "vx_str_i"
                   : n->type.kind == TY_FLOAT ? "vx_str_f"
                   : n->type.kind == TY_BOOL ? "vx_str_b" : NULL;
    if (fn == NULL) { gen_expr(n); return; }
    fprintf(out, "%s(", fn); gen_expr(n); fputs(")", out);
}

static const char *c_operator(int op) {
    switch (op) {
        case OP_ADD: return "+";  case OP_SUB: return "-";  case OP_MUL: return "*";
        case OP_LT:  return "<";  case OP_GT:  return ">";  case OP_LE:  return "<=";
        case OP_GE:  return ">="; case OP_EQ:  return "=="; case OP_NE:  return "!=";
        case OP_AND: return "&&"; case OP_OR:  return "||";
        default:     return "?";
    }
}

static const char *arith_const(int op) {
    switch (op) {
        case OP_ADD: return "VX_ADD"; case OP_SUB: return "VX_SUB";
        case OP_MUL: return "VX_MUL"; case OP_DIV: return "VX_DIV";
        default:     return "VX_MOD";
    }
}

/* Aritmética donde al menos un operando es un vector o matriz */
static void gen_array_arith(Node *n) {
    Type result = n->type;
    Type elem = type_of(result.elem, TY_ERROR);
    Node *l = n->a, *r = n->b;

    if (result.kind == TY_EMPTY) { fputs("vx_empty()", out); return; }

    if (is_array(l->type) && is_array(r->type)) {
        fprintf(out, "vx_arr_arr(%s, ", arith_const(n->op));
        gen_conv(l, result); fputs(", ", out); gen_conv(r, result);
        fprintf(out, ", %s, %d)", kind_const(result.elem), n->line);
    } else {
        int scalar_left = !is_array(l->type);
        Node *arr = scalar_left ? r : l;
        Node *scalar = scalar_left ? l : r;
        fprintf(out, "vx_arr_scalar(%s, ", arith_const(n->op));
        gen_conv(arr, result);
        fprintf(out, ", (VxVal){ .%s = ", field(result.elem));
        gen_conv(scalar, elem);
        fprintf(out, " }, %s, %d, %d)", kind_const(result.elem), scalar_left, n->line);
    }
}

static void gen_binary(Node *n) {
    Node *l = n->a, *r = n->b;
    Type result = n->type;

    /* Concatenación de cadenas */
    if (result.kind == TY_STRING) {
        fputs("vx_cat(", out); gen_as_string(l); fputs(", ", out); gen_as_string(r); fputs(")", out);
        return;
    }

    /* Aritmética */
    if (n->op <= OP_MOD) {
        if (is_array(result)) { gen_array_arith(n); return; }
        if (n->op == OP_DIV || n->op == OP_MOD) {
            /* la división pasa por la biblioteca para detectar el divisor cero */
            const char *fn = result.kind == TY_FLOAT ? "vx_fdiv" : n->op == OP_DIV ? "vx_idiv" : "vx_imod";
            fprintf(out, "%s(", fn);
            gen_conv(l, result); fputs(", ", out); gen_conv(r, result);
            fprintf(out, ", %d)", n->line);
            return;
        }
        fputs("(", out); gen_conv(l, result);
        fprintf(out, " %s ", c_operator(n->op));
        gen_conv(r, result); fputs(")", out);
        return;
    }

    /* Comparaciones y operadores lógicos: el resultado es bool */
    if (l->type.kind == TY_STRING) {
        fputs("(strcmp(", out); gen_expr(l); fputs(", ", out); gen_expr(r);
        fprintf(out, ") %s 0)", c_operator(n->op));
    } else if (is_array(l->type) || is_array(r->type)) {
        Type common = unify(l->type, r->type);
        fprintf(out, "(%svx_arr_eq(", n->op == OP_NE ? "!" : "");
        gen_conv(l, common); fputs(", ", out); gen_conv(r, common);
        fprintf(out, ", %s))", kind_const(common.elem));
    } else {
        fputs("(", out); gen_expr(l);
        fprintf(out, " %s ", c_operator(n->op));
        gen_expr(r); fputs(")", out);
    }
}

/* Llamada: vx_enter cuenta la profundidad y luego se llama a la función C */
static void gen_call(Node *n) {
    Node *b;
    fprintf(out, "(vx_enter(\"%s\", %d), ", n->text, n->line);
    func_name(n->sym);
    fputs("(", out);
    for (b = n->b; b != NULL; b = b->next) {
        if (b != n->b) fputs(", ", out);
        gen_store(b->a, b->type);
    }
    fputs("))", out);
}

static void gen_index(Node *n) {
    Type base = n->a->type;
    if (base.kind == TY_MATRIX) {
        fputs("vx_row(", out);
    } else {
        fputs("vx_get(", out);
    }
    gen_expr(n->a); fputs(", ", out); gen_expr(n->b);
    fprintf(out, ", %d, \"%s\")", n->line, type_name(base));
    if (base.kind == TY_VECTOR) fprintf(out, ".%s", field(base.elem));
}

static void gen_reduce(Node *n) {
    Type arg = n->a->type;
    static const char *names[] = { "VX_SUM", "VX_PROD", "VX_MAX", "VX_MIN" };

    if (n->op == OP_LEN) {
        if (arg.kind == TY_STRING) { fputs("((long long)strlen(", out); gen_expr(n->a); fputs("))", out); }
        else { fputs("(", out); gen_conv(n->a, arg); fputs(").n", out); }
        return;
    }
    fprintf(out, "vx_reduce(%s, ", names[n->op - OP_SUM]);
    gen_conv(n->a, arg);
    fprintf(out, ", %s, %d).%s", kind_const(n->type.kind), n->line, field(n->type.kind));
}

/* Literal de vector o matriz */
static void gen_array(Node *n) {
    Type t = n->type;
    Node *e;
    int count = 0;
    for (e = n->a; e != NULL; e = e->next) count++;

    if (t.kind == TY_EMPTY) {
        fputs("vx_empty()", out);
    } else if (t.kind == TY_VECTOR) {
        Type elem = type_of(t.elem, TY_ERROR);
        fprintf(out, "vx_vec(%d, (VxVal[]){ ", count);
        for (e = n->a; e != NULL; e = e->next) {
            fprintf(out, "{ .%s = ", field(t.elem));
            gen_conv(e, elem);
            fputs(e->next != NULL ? " }, " : " }", out);
        }
        fputs(" })", out);
    } else {
        Type row = type_of(TY_VECTOR, t.elem);
        fprintf(out, "vx_mat(%d, (VxArr[]){ ", count);
        for (e = n->a; e != NULL; e = e->next) {
            gen_conv(e, row);
            if (e->next != NULL) fputs(", ", out);
        }
        fprintf(out, " }, %d)", n->line);
    }
}

static void gen_expr(Node *n) {
    switch (n->kind) {
        case N_INT:    fprintf(out, "%lldLL", strtoll(n->text, NULL, 10)); break;  /* sin ceros a la izquierda: en C serían octal */
        case N_FLOAT:  fputs(n->text, out); break;
        case N_STRING: fputs(n->text, out); break;      /* mismo formato de cadena que C */
        case N_BOOL:   fputs(strcmp(n->text, "true") == 0 ? "1" : "0", out); break;
        case N_VAR:    var_name(n->sym); break;
        case N_BINARY: gen_binary(n); break;
        case N_UNARY:
            if (n->op == OP_NOT) {
                fputs("(!", out); gen_expr(n->a); fputs(")", out);
            } else if (n->type.kind == TY_EMPTY) {
                fputs("vx_empty()", out);
            } else if (is_array(n->type)) {
                fputs("vx_arr_neg(", out); gen_expr(n->a);
                fprintf(out, ", %s)", kind_const(n->type.elem));
            } else {
                fputs("(-", out); gen_expr(n->a); fputs(")", out);
            }
            break;
        case N_INDEX:  gen_index(n); break;
        case N_CALL:   gen_call(n); break;
        case N_REDUCE: gen_reduce(n); break;
        case N_ARRAY:  gen_array(n); break;
        default: break;
    }
}

/* ===================== Sentencias ===================== */

/* Asignación o expresión suelta, sin el ';' final (sirve también en el for) */
static void gen_simple(Node *n) {
    Node *target, *value;
    if (n->kind == N_EXPRSTMT) { gen_expr(n->a); return; }

    target = n->a;
    value = n->b;
    if (target->kind == N_VAR) {
        /* x = valor */
        var_name(target->sym); fputs(" = ", out); gen_store(value, target->type);
    } else if (target->a->kind == N_VAR && target->a->type.kind == TY_MATRIX) {
        /* m[i] = fila */
        fputs("vx_set_row(&", out); var_name(target->a->sym); fputs(", ", out);
        gen_expr(target->b); fputs(", ", out);
        gen_store(value, target->type);
        fprintf(out, ", %d, \"%s\")", n->line, type_name(target->a->type));
    } else if (target->a->kind == N_VAR) {
        /* v[i] = valor */
        fputs("vx_at(&", out); var_name(target->a->sym); fputs(", ", out);
        gen_expr(target->b);
        fprintf(out, ", %d, \"%s\")->%s = ", n->line, type_name(target->a->type), field(target->type.kind));
        gen_conv(value, target->type);
    } else {
        /* m[i][j] = valor */
        Node *matrix = target->a->a;
        fputs("vx_at2(&", out); var_name(matrix->sym); fputs(", ", out);
        gen_expr(target->a->b); fputs(", ", out); gen_expr(target->b);
        fprintf(out, ", %d, \"%s\")->%s = ", n->line, type_name(matrix->type), field(target->type.kind));
        gen_conv(value, target->type);
    }
}

/* Declaración de variable, sin el ';' final */
static void gen_vardecl(Node *n) {
    fprintf(out, "%s ", c_type(n->type));
    var_name(n->sym);
    fputs(" = ", out);
    gen_store(n->a, n->type);
}

/* Cabecera de una función:  tipo f_nombre_N(parámetros)  */
static void gen_signature(Node *f, int with_names) {
    Node *p;
    fprintf(out, "%s ", c_type(f->type));
    func_name(f->sym);
    fputs("(", out);
    if (f->a == NULL) fputs("void", out);
    for (p = f->a; p != NULL; p = p->next) {
        if (p != f->a) fputs(", ", out);
        fputs(c_type(p->type), out);
        if (with_names) { fputs(" ", out); var_name(p->sym); }
    }
    fputs(")", out);
}

static void gen_block(Node *block) {
    fputs("{\n", out);
    level++;
    gen_stmts(block->a);
    level--;
    indent(); fputs("}", out);
}

static void gen_func(Node *f) {
    int saved_try = try_depth, saved_loops = loop_count;
    try_depth = 0;
    loop_count = 0;

    indent(); gen_signature(f, 1); fputs(" {\n", out);
    level++;
    gen_stmts(f->b->a);
    indent();
    if (f->type.kind == TY_VOID)
        fputs("vx_depth--;\n", out);
    else    /* llegó al final sin return */
        fprintf(out, "vx_no_return(\"%s\", \"%s\", %d);\n", f->text, type_name(f->type), f->line);
    level--;
    indent(); fputs("}\n", out);

    try_depth = saved_try;
    loop_count = saved_loops;
}

/* Antes de saltar fuera de n bloques try hay que retirar sus manejadores */
static void gen_leave_try(int n) {
    if (n > 0) fprintf(out, "vx_try_top -= %d; ", n);
}

/* switch se traduce a if / else if: sin paso entre ramas, y así break y
 * continue dentro de una rama siguen actuando sobre el ciclo que la contiene */
static void gen_switch(Node *n) {
    Type subject = n->a->type;
    int temp = next_temp++;
    int first = 1;
    Node *c;

    indent(); fputs("{\n", out);
    level++;
    indent(); fprintf(out, "%s vx_sw_%d = ", c_type(subject), temp); gen_expr(n->a); fputs(";\n", out);
    indent();
    for (c = n->b; c != NULL; c = c->next) {
        if (!first) fputs(" else ", out);
        if (c->a != NULL) {
            if (subject.kind == TY_STRING) {
                fprintf(out, "if (strcmp(vx_sw_%d, ", temp); gen_expr(c->a); fputs(") == 0) ", out);
            } else {
                fprintf(out, "if (vx_sw_%d == ", temp); gen_expr(c->a); fputs(") ", out);
            }
        }
        gen_block(c->b);
        first = 0;
    }
    fputs("\n", out);
    level--;
    indent(); fputs("}\n", out);
}

static void gen_try(Node *n) {
    indent(); fprintf(out, "vx_try_begin(%d);\n", n->line);
    indent(); fputs("if (setjmp(vx_handlers[vx_try_top - 1].jump) == 0) {\n", out);
    level++;
    try_depth++;
    indent(); gen_block(n->a); fputs("\n", out);
    try_depth--;
    indent(); fputs("vx_try_top--;\n", out);        /* terminó sin errores */
    level--;
    indent(); fputs("} else {\n", out);
    level++;
    indent(); fputs("const char *", out); var_name(n->sym); fputs(" = vx_exc_text();\n", out);
    indent(); gen_block(n->b); fputs("\n", out);
    level--;
    indent(); fputs("}\n", out);
}

/* print: primero se calculan todos los argumentos y después se imprimen.
 * Si un argumento llama a una función que también imprime, su salida queda
 * antes de la línea de este print y no en medio. */
static void gen_print(Node *n) {
    int base = next_temp;
    int k;
    Node *e;

    indent(); fputs("{\n", out);
    level++;
    for (e = n->a; e != NULL; e = e->next) {
        indent(); fprintf(out, "%s vx_p_%d = ", c_type(e->type), next_temp++);
        gen_conv(e, e->type); fputs(";\n", out);
    }
    for (e = n->a, k = base; e != NULL; e = e->next, k++) {
        Type t = e->type;
        indent();
        if (is_array(t))
            fprintf(out, "vx_print_arr(vx_p_%d, %s);", k, kind_const(t.kind == TY_EMPTY ? TY_INT : t.elem));
        else
            fprintf(out, "vx_print_%s(vx_p_%d);", field(t.kind), k);
        fputs(e->next != NULL ? " putchar(' ');\n" : "\n", out);
    }
    indent(); fputs("putchar('\\n');\n", out);
    level--;
    indent(); fputs("}\n", out);
}

static void gen_return(Node *n) {
    indent(); fputs("{ ", out);
    if (n->a != NULL) {
        /* primero se calcula el valor, todavía dentro de los try abiertos */
        fprintf(out, "%s vx_r = ", c_type(n->type)); gen_store(n->a, n->type); fputs("; ", out);
    }
    gen_leave_try(try_depth);
    fputs(n->a != NULL ? "vx_depth--; return vx_r; }\n" : "vx_depth--; return; }\n", out);
}

static void gen_stmt(Node *n) {
    switch (n->kind) {
        case N_VARDECL:
            indent(); gen_vardecl(n); fputs(";\n", out);
            break;
        case N_ASSIGN:
        case N_EXPRSTMT:
            indent(); gen_simple(n); fputs(";\n", out);
            break;
        case N_BLOCK:
            indent(); gen_block(n); fputs("\n", out);
            break;
        case N_IF:
            indent();
            for (;;) {      /* recorre la cadena if / else if / else */
                fputs("if (", out); gen_expr(n->a); fputs(") ", out);
                gen_block(n->b);
                if (n->c == NULL) break;
                fputs(" else ", out);
                if (n->c->kind == N_BLOCK) { gen_block(n->c); break; }
                n = n->c;
            }
            fputs("\n", out);
            break;
        case N_WHILE:
            indent(); fputs("while (", out); gen_expr(n->a); fputs(") ", out);
            loop_base[loop_count++] = try_depth;
            gen_block(n->b);
            loop_count--;
            fputs("\n", out);
            break;
        case N_FOR:
            indent(); fputs("for (", out);
            if (n->a != NULL) { if (n->a->kind == N_VARDECL) gen_vardecl(n->a); else gen_simple(n->a); }
            fputs("; ", out);
            if (n->b != NULL) gen_expr(n->b);
            fputs("; ", out);
            if (n->c != NULL) gen_simple(n->c);
            fputs(") ", out);
            loop_base[loop_count++] = try_depth;
            gen_block(n->d);
            loop_count--;
            fputs("\n", out);
            break;
        case N_SWITCH:
            gen_switch(n);
            break;
        case N_FUNC:
            gen_func(n);
            break;
        case N_RETURN:
            gen_return(n);
            break;
        case N_BREAK:
        case N_CONTINUE:
            indent(); fputs("{ ", out);
            gen_leave_try(try_depth - loop_base[loop_count - 1]);
            fputs(n->kind == N_BREAK ? "break; }\n" : "continue; }\n", out);
            break;
        case N_THROW:
            indent(); fputs("vx_throw(", out); gen_as_string(n->a); fprintf(out, ", %d);\n", n->line);
            break;
        case N_TRY:
            gen_try(n);
            break;
        case N_ASSERT:
            indent(); fputs("if (!", out); gen_expr(n->a); fputs(") vx_assert_fail(", out);
            if (n->b != NULL) gen_expr(n->b); else fputs("NULL", out);
            fprintf(out, ", %d);\n", n->line);
            break;
        case N_PRINT:
            gen_print(n);
            break;
        default:
            break;
    }
}

/* Lista de sentencias. Primero declara las funciones de la lista (prototipos
 * con auto, como exige GCC para funciones anidadas): así pueden llamarse
 * entre sí sin importar el orden en que estén definidas. */
static void gen_stmts(Node *list) {
    Node *n;
    for (n = list; n != NULL; n = n->next)
        if (n->kind == N_FUNC) {
            indent(); fputs("auto ", out); gen_signature(n, 0); fputs(";\n", out);
        }
    for (n = list; n != NULL; n = n->next)
        gen_stmt(n);
}

void generate_c(Node *program, FILE *file) {
    out = file;
    fputs("/* Generado por vortexc, el compilador de Vortex. No editar. */\n", out);
    fputs("#include \"vx_runtime.h\"\n\n", out);
    fputs("int main(void) {\n", out);
    level = 1;
    gen_stmts(program);
    indent(); fputs("return 0;\n", out);
    fputs("}\n", out);
}
