/* ==========================================================================
 * tests/nativo/aritmetica.c - Prueba de memoria, pila y aritmética entera
 *
 * Genera un programa que calcula expresiones del mismo modo en que lo hará
 * el generador de código de vortexc:
 *
 *   - Las variables viven en el marco de pila, a desplazamientos fijos de rbp:
 *         push rbp
 *         mov rbp, rsp
 *         sub rsp, 16          ; a en [rbp-8], b en [rbp-16]
 *   - Máquina de pila: el resultado de cada expresión queda en rax. Para un
 *     operador binario se calcula el operando izquierdo, se apila, se
 *     calcula el derecho y se combinan:
 *         <izquierdo>          ; rax = izquierdo
 *         push rax
 *         <derecho>            ; rax = derecho
 *         mov rcx, rax
 *         pop rax              ; rax = izquierdo, rcx = derecho
 *         add rax, rcx
 *
 * Cada resultado se compara con el valor esperado y el programa imprime
 * "ok N nombre" o "FALLA N nombre".
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "../../x86.h"

static Asm a;
static Label imprimir;
static int numero = 0;

/* Imprime un texto fijo: rsi = dirección, rdx = largo, call imprimir */
static void emit_print(const char *text) {
    uint64_t addr = asm_data(&a, text, (int)strlen(text));
    x_mov_ri(&a, RSI, addr);
    x_mov_ri(&a, RDX, strlen(text));
    x_call(&a, imprimir);
}

/* Compara rax con el valor esperado e imprime el resultado */
static void check(const char *nombre, int64_t esperado) {
    char ok[80], falla[80];
    Label error = asm_label(&a), siguiente = asm_label(&a);
    numero++;
    snprintf(ok, sizeof ok, "ok %d %s\n", numero, nombre);
    snprintf(falla, sizeof falla, "FALLA %d %s\n", numero, nombre);
    x_mov_ri(&a, RBX, (uint64_t)esperado);
    x_cmp_rr(&a, RAX, RBX);
    x_jcc(&a, CC_NE, error);
    emit_print(ok);
    x_jmp(&a, siguiente);
    asm_bind(&a, error);
    emit_print(falla);
    asm_bind(&a, siguiente);
}

/* Variables del programa generado */
#define VAR_A (-8)
#define VAR_B (-16)
static void load_a(void) { x_load(&a, RAX, RBP, VAR_A); }
static void load_b(void) { x_load(&a, RAX, RBP, VAR_B); }
static void constant(int64_t v) { x_mov_ri(&a, RAX, (uint64_t)v); }

/* Pasos de la máquina de pila */
static void push_left(void) { x_push(&a, RAX); }
static void pop_left(void) { x_mov_rr(&a, RCX, RAX); x_pop(&a, RAX); }   /* rax = izq, rcx = der */

int main(int argc, char **argv) {
    int entry;

    if (argc != 2) { fprintf(stderr, "Usage: %s output\n", argv[0]); return 1; }
    asm_init(&a);
    imprimir = asm_label(&a);

    entry = a.code.len;
    x_push(&a, RBP);
    x_mov_rr(&a, RBP, RSP);
    x_sub_ri(&a, RSP, 16);

    /* int a = 7; int b = 2; */
    constant(7);
    x_store(&a, RBP, VAR_A, RAX);
    constant(2);
    x_store(&a, RBP, VAR_B, RAX);
    load_a();
    check("variables: a", 7);

    /* a + b * 3 */
    load_a(); push_left();
    load_b(); push_left(); constant(3); pop_left(); x_imul_rr(&a, RAX, RCX);
    pop_left(); x_add_rr(&a, RAX, RCX);
    check("a + b * 3", 13);

    /* (a + b) * 3 */
    load_a(); push_left(); load_b(); pop_left(); x_add_rr(&a, RAX, RCX);
    push_left(); constant(3); pop_left(); x_imul_rr(&a, RAX, RCX);
    check("(a + b) * 3", 27);

    /* a / b y a % b: cociente en rax, resto en rdx */
    load_a(); push_left(); load_b(); pop_left(); x_cqo(&a); x_idiv(&a, RCX);
    check("a / b", 3);
    load_a(); push_left(); load_b(); pop_left(); x_cqo(&a); x_idiv(&a, RCX); x_mov_rr(&a, RAX, RDX);
    check("a % b", 1);

    /* División con negativos: se trunca hacia cero */
    constant(-7); push_left(); constant(2); pop_left(); x_cqo(&a); x_idiv(&a, RCX);
    check("-7 / 2", -3);
    constant(-7); push_left(); constant(2); pop_left(); x_cqo(&a); x_idiv(&a, RCX); x_mov_rr(&a, RAX, RDX);
    check("-7 % 2", -1);

    /* -a + 10 */
    load_a(); x_neg(&a, RAX); push_left(); constant(10); pop_left(); x_add_rr(&a, RAX, RCX);
    check("-a + 10", 3);

    /* a - b */
    load_a(); push_left(); load_b(); pop_left(); x_sub_rr(&a, RAX, RCX);
    check("a - b", 5);

    /* Comparaciones: cmp, set<cc> y movzx dejan 0 o 1 en rax */
    load_a(); push_left(); load_b(); pop_left(); x_cmp_rr(&a, RAX, RCX); x_setcc_al(&a, CC_G); x_movzx_rax_al(&a);
    check("a > b", 1);
    load_a(); push_left(); load_b(); pop_left(); x_cmp_rr(&a, RAX, RCX); x_setcc_al(&a, CC_L); x_movzx_rax_al(&a);
    check("a < b", 0);
    load_a(); push_left(); constant(7); pop_left(); x_cmp_rr(&a, RAX, RCX); x_setcc_al(&a, CC_E); x_movzx_rax_al(&a);
    check("a == 7", 1);

    /* Valores que no caben en 32 bits */
    constant(5000000000LL); push_left(); constant(3); pop_left(); x_imul_rr(&a, RAX, RCX);
    check("5000000000 * 3", 15000000000LL);

    /* Registros r8..r15: necesitan los bits R y B del prefijo REX */
    x_mov_ri(&a, R12, 40);
    x_mov_ri(&a, R9, 2);
    x_add_rr(&a, R12, R9);
    x_push(&a, R12);
    x_pop(&a, R15);
    x_mov_rr(&a, RAX, R15);
    check("registros r8-r15", 42);

    /* Memoria con base rsp: necesita el byte SIB. [rsp+0] es [rbp-16] */
    constant(99);
    x_store(&a, RSP, 0, RAX);
    x_load(&a, RCX, RBP, VAR_B);
    x_mov_rr(&a, RAX, RCX);
    check("memoria con base rsp", 99);

    /* Salida: deshacer el marco y exit(0) */
    x_mov_rr(&a, RSP, RBP);
    x_pop(&a, RBP);
    x_mov_ri(&a, RAX, SYS_EXIT);
    x_mov_ri(&a, RDI, 0);
    x_syscall(&a);

    /* imprimir: write(1, rsi, rdx) */
    asm_bind(&a, imprimir);
    x_mov_ri(&a, RAX, SYS_WRITE);
    x_mov_ri(&a, RDI, 1);
    x_syscall(&a);
    x_ret(&a);

    asm_finish(&a);
    if (elf_write(argv[1], &a, entry) != 0) { perror(argv[1]); return 1; }
    return 0;
}
