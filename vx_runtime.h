/* ==========================================================================
 * vx_runtime.h - Biblioteca de ejecución de los programas Vortex compilados
 *
 * El compilador traduce cada programa .vx a un archivo .c que incluye este
 * encabezado. Aquí está todo lo que el lenguaje hace en tiempo de ejecución
 * y que C no trae resuelto:
 *   - vectores y matrices (creación, copia, índices verificados, aritmética
 *     elemento a elemento, reducciones)
 *   - cadenas (concatenación y conversión de números a texto)
 *   - excepciones (try / catch / throw / assert) con setjmp y longjmp
 *   - errores de ejecución con su línea (división por cero, índice fuera de
 *     rango, recursión infinita)
 * ========================================================================== */
#ifndef VX_RUNTIME_H
#define VX_RUNTIME_H

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Un elemento de un vector o matriz. El compilador conoce el tipo de cada
 * arreglo y usa el campo que corresponde (i, f, b o s). */
typedef union {
    long long i;        /* int    */
    double f;           /* float  */
    int b;              /* bool   */
    const char *s;      /* string */
} VxVal;

/* Vector o matriz.
 *   vector: n elementos, cols = -1
 *   matriz: n filas de cols columnas, guardadas una fila tras otra en d */
typedef struct {
    long long n;
    long long cols;
    VxVal *d;
} VxArr;

enum { VX_INT, VX_FLOAT, VX_BOOL, VX_STR };             /* tipo de elemento  */
enum { VX_ADD, VX_SUB, VX_MUL, VX_DIV, VX_MOD };        /* aritmética        */
enum { VX_SUM, VX_PROD, VX_MAX, VX_MIN };               /* reducciones       */
enum { VX_E_RUNTIME, VX_E_ASSERT, VX_E_THROW };         /* clase de excepción */

#define VX_MAX_DEPTH 1000   /* llamadas anidadas permitidas       */
#define VX_MAX_TRY   2048   /* bloques try activos al mismo tiempo */

/* ---------------------------------------------------------------- memoria */

/* La memoria de cadenas y arreglos no se libera: los programas son cortos y
 * el sistema operativo la recupera al terminar. */
static void *vx_alloc(size_t bytes) {
    void *p = malloc(bytes > 0 ? bytes : 1);
    if (p == NULL) {
        printf("[Runtime error] out of memory\n");
        exit(1);
    }
    return p;
}

/* ------------------------------------------------------------ excepciones */

/* Un try activo: dónde continuar si algo falla y cuántas llamadas había */
typedef struct {
    jmp_buf jump;
    int depth;
} VxHandler;

static VxHandler vx_handlers[VX_MAX_TRY];
static int vx_try_top = 0;      /* cantidad de try activos        */
static int vx_depth = 0;        /* llamadas a función en curso    */

/* La excepción que se está propagando */
static struct {
    int kind;
    long long line;
    const char *msg;
} vx_exc;

/* Lanza una excepción. Si hay un try activo, salta a su catch; si no, el
 * programa termina mostrando la clase de error y la línea. */
static void vx_raise(int kind, long long line, const char *msg) {
    if (vx_try_top == 0) {
        const char *label = kind == VX_E_ASSERT ? "Assertion error"
                          : kind == VX_E_THROW  ? "Uncaught exception"
                          :                       "Runtime error";
        printf("[%s] line %lld: %s\n", label, line, msg);
        exit(1);
    }
    vx_exc.kind = kind;
    vx_exc.line = line;
    vx_exc.msg = msg;
    vx_try_top--;
    vx_depth = vx_handlers[vx_try_top].depth;
    longjmp(vx_handlers[vx_try_top].jump, 1);
}

/* Error de ejecución con mensaje armado al estilo printf */
static void vx_fail(long long line, const char *fmt, ...) {
    char *msg = vx_alloc(256);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, 256, fmt, ap);
    va_end(ap);
    vx_raise(VX_E_RUNTIME, line, msg);
}

/* Abre un try. El código generado llama a setjmp justo después. */
static void vx_try_begin(long long line) {
    if (vx_try_top >= VX_MAX_TRY) {
        vx_try_top = 0;
        vx_fail(line, "too many nested try blocks");
    }
    vx_handlers[vx_try_top].depth = vx_depth;
    vx_try_top++;
}

/* Texto que recibe la variable del catch. Los errores del lenguaje llevan su
 * línea; los lanzados con throw son exactamente el valor lanzado. */
static const char *vx_exc_text(void) {
    char *text;
    size_t size;
    if (vx_exc.kind == VX_E_THROW) return vx_exc.msg;
    size = strlen(vx_exc.msg) + 32;
    text = vx_alloc(size);
    snprintf(text, size, "%s (line %lld)", vx_exc.msg, vx_exc.line);
    return text;
}

static void vx_throw(const char *msg, long long line) {
    vx_raise(VX_E_THROW, line, msg);
}

static void vx_assert_fail(const char *msg, long long line) {
    char *text;
    size_t size;
    if (msg == NULL) vx_raise(VX_E_ASSERT, line, "assertion failed");
    size = strlen(msg) + 32;
    text = vx_alloc(size);
    snprintf(text, size, "assertion failed: %s", msg);
    vx_raise(VX_E_ASSERT, line, text);
}

/* -------------------------------------------------------------- funciones */

/* Se llama antes de cada llamada a función: cuenta la profundidad para
 * detener una recursión infinita con un error en vez de un fallo del sistema.
 * Devuelve 0 para poder usarse dentro de una expresión. */
static int vx_enter(const char *name, long long line) {
    if (vx_depth >= VX_MAX_DEPTH)
        vx_fail(line, "maximum recursion depth (%d) exceeded in '%s'", VX_MAX_DEPTH, name);
    vx_depth++;
    return 0;
}

/* Una función con tipo de retorno llegó al final sin ejecutar return */
static void vx_no_return(const char *name, const char *type, long long line) {
    vx_fail(line, "function '%s' must return a value of type %s", name, type);
}

/* ---------------------------------------------------------------- cadenas */

/* Un float siempre se muestra con punto decimal: 5.0, 2.5, 6.28318 */
static const char *vx_str_f(double x) {
    char *buf = vx_alloc(40);
    snprintf(buf, 36, "%.15g", x);
    if (strpbrk(buf, ".en") == NULL) strcat(buf, ".0");     /* e: exponente; n: inf, nan */
    return buf;
}

static const char *vx_str_i(long long x) {
    char *buf = vx_alloc(24);
    snprintf(buf, 24, "%lld", x);
    return buf;
}

static const char *vx_str_b(int x) { return x ? "true" : "false"; }

static const char *vx_cat(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    char *r = vx_alloc(la + lb + 1);
    memcpy(r, a, la);
    memcpy(r + la, b, lb + 1);
    return r;
}

/* ------------------------------------------------------ aritmética escalar */

static long long vx_idiv(long long a, long long b, long long line) {
    if (b == 0) vx_fail(line, "division by zero");
    return a / b;
}

static long long vx_imod(long long a, long long b, long long line) {
    if (b == 0) vx_fail(line, "division by zero");
    return a % b;
}

static double vx_fdiv(double a, double b, long long line) {
    if (b == 0.0) vx_fail(line, "division by zero");
    return a / b;
}

/* --------------------------------------------------------------- arreglos */

/* Cantidad total de elementos guardados */
static long long vx_total(VxArr a) { return a.cols < 0 ? a.n : a.n * a.cols; }

static VxArr vx_new(long long n, long long cols) {
    VxArr a;
    a.n = n;
    a.cols = cols;
    a.d = vx_alloc((size_t)vx_total(a) * sizeof(VxVal));
    return a;
}

/* El literal [] */
static VxArr vx_empty(void) { return vx_new(0, -1); }

/* Vector a partir de sus elementos: [1, 2, 3] */
static VxArr vx_vec(long long n, const VxVal *items) {
    VxArr a = vx_new(n, -1);
    memcpy(a.d, items, (size_t)n * sizeof(VxVal));
    return a;
}

/* Matriz a partir de sus filas: [[1, 2], [3, 4]]. Todas deben medir igual. */
static VxArr vx_mat(long long n, const VxArr *rows, long long line) {
    VxArr m = vx_new(n, rows[0].n);
    long long r;
    for (r = 0; r < n; r++) {
        if (rows[r].n != m.cols)
            vx_fail(line, "matrix rows must have the same length (row 0 has %lld, row %lld has %lld)",
                    m.cols, r, rows[r].n);
        memcpy(m.d + r * m.cols, rows[r].d, (size_t)m.cols * sizeof(VxVal));
    }
    return m;
}

/* Los arreglos se pasan y se asignan por valor: cada destino tiene su copia */
static VxArr vx_copy(VxArr a) {
    VxArr c = vx_new(a.n, a.cols);
    memcpy(c.d, a.d, (size_t)vx_total(a) * sizeof(VxVal));
    return c;
}

/* vector<int> -> vector<float> (o matriz) */
static VxArr vx_to_float(VxArr a) {
    VxArr c = vx_new(a.n, a.cols);
    long long k, total = vx_total(a);
    for (k = 0; k < total; k++) c.d[k].f = (double)a.d[k].i;
    return c;
}

/* Dirección del elemento v[i], verificando el índice */
static VxVal *vx_at(VxArr *v, long long i, long long line, const char *type) {
    if (i < 0 || i >= v->n)
        vx_fail(line, "index %lld out of range for %s of length %lld", i, type, v->n);
    return &v->d[i];
}

/* Valor del elemento v[i] */
static VxVal vx_get(VxArr v, long long i, long long line, const char *type) {
    return *vx_at(&v, i, line, type);
}

static void vx_check_row(VxArr *m, long long i, long long line, const char *type) {
    if (i < 0 || i >= m->n)
        vx_fail(line, "index %lld out of range for %s with %lld rows", i, type, m->n);
}

/* Dirección del elemento m[i][j] */
static VxVal *vx_at2(VxArr *m, long long i, long long j, long long line, const char *type) {
    vx_check_row(m, i, line, type);
    if (j < 0 || j >= m->cols)
        vx_fail(line, "index %lld out of range for a row of %s with %lld columns", j, type, m->cols);
    return &m->d[i * m->cols + j];
}

/* Copia de la fila m[i], como vector */
static VxArr vx_row(VxArr m, long long i, long long line, const char *type) {
    vx_check_row(&m, i, line, type);
    return vx_vec(m.cols, m.d + i * m.cols);
}

/* m[i] = fila */
static void vx_set_row(VxArr *m, long long i, VxArr row, long long line, const char *type) {
    vx_check_row(m, i, line, type);
    if (row.n != m->cols)
        vx_fail(line, "cannot assign a row of length %lld to %s with %lld columns", row.n, type, m->cols);
    memcpy(m->d + i * m->cols, row.d, (size_t)m->cols * sizeof(VxVal));
}

/* Una operación aritmética entre dos elementos del mismo tipo */
static VxVal vx_apply(int op, VxVal a, VxVal b, int kind, long long line) {
    VxVal r;
    if (kind == VX_INT) {
        switch (op) {
            case VX_ADD: r.i = a.i + b.i; break;
            case VX_SUB: r.i = a.i - b.i; break;
            case VX_MUL: r.i = a.i * b.i; break;
            case VX_DIV: r.i = vx_idiv(a.i, b.i, line); break;
            default:     r.i = vx_imod(a.i, b.i, line); break;
        }
    } else {
        switch (op) {
            case VX_ADD: r.f = a.f + b.f; break;
            case VX_SUB: r.f = a.f - b.f; break;
            case VX_MUL: r.f = a.f * b.f; break;
            default:     r.f = vx_fdiv(a.f, b.f, line); break;
        }
    }
    return r;
}

/* arreglo (op) arreglo, elemento a elemento: deben tener la misma forma */
static VxArr vx_arr_arr(int op, VxArr a, VxArr b, int kind, long long line) {
    VxArr r;
    long long k, total;
    if (a.n != b.n || (a.n > 0 && a.cols != b.cols)) {
        if (a.cols < 0 && b.cols < 0)
            vx_fail(line, "vectors of different length (%lld and %lld) in an element-wise operation", a.n, b.n);
        vx_fail(line, "matrices of different shape in an element-wise operation");
    }
    r = vx_new(a.n, a.cols);
    total = vx_total(a);
    for (k = 0; k < total; k++) r.d[k] = vx_apply(op, a.d[k], b.d[k], kind, line);
    return r;
}

/* arreglo (op) escalar. Si scalar_left es 1, el escalar va a la izquierda. */
static VxArr vx_arr_scalar(int op, VxArr a, VxVal s, int kind, int scalar_left, long long line) {
    VxArr r = vx_new(a.n, a.cols);
    long long k, total = vx_total(a);
    for (k = 0; k < total; k++)
        r.d[k] = scalar_left ? vx_apply(op, s, a.d[k], kind, line)
                             : vx_apply(op, a.d[k], s, kind, line);
    return r;
}

static VxArr vx_arr_neg(VxArr a, int kind) {
    VxArr r = vx_new(a.n, a.cols);
    long long k, total = vx_total(a);
    for (k = 0; k < total; k++) {
        if (kind == VX_INT) r.d[k].i = -a.d[k].i;
        else                r.d[k].f = -a.d[k].f;
    }
    return r;
}

/* Igualdad: misma forma y mismos elementos */
static int vx_arr_eq(VxArr a, VxArr b, int kind) {
    long long k, total = vx_total(a);
    if (a.n != b.n || (a.n > 0 && a.cols != b.cols)) return 0;
    for (k = 0; k < total; k++) {
        int same = kind == VX_INT   ? a.d[k].i == b.d[k].i
                 : kind == VX_FLOAT ? a.d[k].f == b.d[k].f
                 : kind == VX_BOOL  ? a.d[k].b == b.d[k].b
                 :                    strcmp(a.d[k].s, b.d[k].s) == 0;
        if (!same) return 0;
    }
    return 1;
}

/* sum, prod, max y min sobre todos los elementos de un vector o matriz */
static VxVal vx_reduce(int op, VxArr a, int kind, long long line) {
    long long k, total = vx_total(a);
    VxVal r;
    if (total == 0 && (op == VX_MAX || op == VX_MIN))
        vx_fail(line, "%s of an empty vector", op == VX_MAX ? "max" : "min");

    if (kind == VX_INT) {
        r.i = op == VX_SUM ? 0 : op == VX_PROD ? 1 : a.d[0].i;
        for (k = 0; k < total; k++) {
            long long x = a.d[k].i;
            if (op == VX_SUM) r.i += x;
            else if (op == VX_PROD) r.i *= x;
            else if (op == VX_MAX) { if (x > r.i) r.i = x; }
            else if (x < r.i) r.i = x;
        }
    } else {
        r.f = op == VX_SUM ? 0.0 : op == VX_PROD ? 1.0 : a.d[0].f;
        for (k = 0; k < total; k++) {
            double x = a.d[k].f;
            if (op == VX_SUM) r.f += x;
            else if (op == VX_PROD) r.f *= x;
            else if (op == VX_MAX) { if (x > r.f) r.f = x; }
            else if (x < r.f) r.f = x;
        }
    }
    return r;
}

/* ------------------------------------------------------------------ print */

static void vx_print_i(long long x) { printf("%lld", x); }
static void vx_print_f(double x)    { fputs(vx_str_f(x), stdout); }
static void vx_print_b(int x)       { fputs(vx_str_b(x), stdout); }
static void vx_print_s(const char *x) { fputs(x, stdout); }

static void vx_print_items(const VxVal *d, long long n, int kind) {
    long long k;
    putchar('[');
    for (k = 0; k < n; k++) {
        if (k > 0) fputs(", ", stdout);
        switch (kind) {
            case VX_INT:   vx_print_i(d[k].i); break;
            case VX_FLOAT: vx_print_f(d[k].f); break;
            case VX_BOOL:  vx_print_b(d[k].b); break;
            default:       printf("\"%s\"", d[k].s); break;    /* dentro de un arreglo, entre comillas */
        }
    }
    putchar(']');
}

/* [1, 2, 3]  o  [[1, 2], [3, 4]] */
static void vx_print_arr(VxArr a, int kind) {
    long long r;
    if (a.cols < 0) {
        vx_print_items(a.d, a.n, kind);
        return;
    }
    putchar('[');
    for (r = 0; r < a.n; r++) {
        if (r > 0) fputs(", ", stdout);
        vx_print_items(a.d + r * a.cols, a.cols, kind);
    }
    putchar(']');
}

#endif
