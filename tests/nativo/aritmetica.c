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

/* Bits IEEE 754 de un double: así viaja un float en rax */
static uint64_t bits(double d) {
    uint64_t u;
    memcpy(&u, &d, sizeof u);
    return u;
}

/* xmm = el double v, pasando por rax */
static void load_double(Xmm x, double v) {
    x_mov_ri(&a, RAX, bits(v));
    x_movq_xr(&a, x, RAX);
}

/* rax = 0 o 1 según una condición después de ucomisd x, y */
static void compare_doubles(double x, double y, Cond cc) {
    load_double(XMM0, x);
    load_double(XMM1, y);
    x_ucomisd(&a, XMM0, XMM1);
    x_setcc_al(&a, cc);
    x_movzx_rax_al(&a);
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

    /* div sin signo: 0x8000000000000000 (= 2^63 sin signo) / 10 */
    x_mov_ri(&a, RAX, 0x8000000000000000ULL);
    x_mov_ri(&a, RDX, 0);
    x_mov_ri(&a, RCX, 10);
    x_div(&a, RCX);
    check("div sin signo: 2^63 / 10", 922337203685477580LL);

    /* Escritura de un byte desde dil (registro 7: necesita REX 0x40) */
    constant(0);
    x_store(&a, RBP, VAR_B, RAX);
    x_mov_ri(&a, RDI, 0x141);               /* el byte bajo es 0x41 */
    x_store8(&a, RBP, VAR_B, RDI);
    x_load(&a, RAX, RBP, VAR_B);
    check("byte desde dil", 0x41);

    /* rep movsb: copia los 8 bytes de a sobre b */
    x_mov_rr(&a, RSI, RBP);
    x_sub_ri(&a, RSI, -VAR_A);              /* rsi = &a */
    x_mov_rr(&a, RDI, RBP);
    x_sub_ri(&a, RDI, -VAR_B);              /* rdi = &b */
    x_mov_ri(&a, RCX, 8);
    x_rep_movsb(&a);
    x_load(&a, RAX, RBP, VAR_B);
    check("rep movsb", 7);

    /* ---- Punto flotante: los resultados se comparan como bits IEEE 754 ---- */
    load_double(XMM0, 1.5);
    load_double(XMM1, 2.25);
    x_addsd(&a, XMM0, XMM1);
    x_movq_rx(&a, RAX, XMM0);
    check("addsd: 1.5 + 2.25", (int64_t)bits(3.75));

    load_double(XMM2, 10.0);
    load_double(XMM3, 4.0);
    x_subsd(&a, XMM2, XMM3);                /* 6.0 */
    load_double(XMM3, 3.0);
    x_mulsd(&a, XMM2, XMM3);                /* 18.0 */
    load_double(XMM3, 4.0);
    x_divsd(&a, XMM2, XMM3);                /* 4.5 */
    x_movq_rx(&a, RAX, XMM2);
    check("subsd, mulsd, divsd: (10 - 4) * 3 / 4", (int64_t)bits(4.5));

    x_mov_ri(&a, RAX, (uint64_t)-7);
    x_cvtsi2sd(&a, XMM5, RAX);
    x_movq_rx(&a, RAX, XMM5);
    check("cvtsi2sd: -7 -> -7.0", (int64_t)bits(-7.0));

    load_double(XMM0, 2.5);
    x_cvtsd2si(&a, RAX, XMM0);
    check("cvtsd2si: 2.5 -> 2 (empate al par)", 2);
    load_double(XMM0, 3.5);
    x_cvtsd2si(&a, RAX, XMM0);
    check("cvtsd2si: 3.5 -> 4 (empate al par)", 4);
    load_double(XMM0, -2.7);
    x_cvtsd2si(&a, R10, XMM0);
    x_mov_rr(&a, RAX, R10);
    check("cvtsd2si: -2.7 -> -3 (en r10)", -3);

    compare_doubles(2.0, 1.0, CC_A);
    check("ucomisd: 2.0 > 1.0", 1);
    compare_doubles(1.0, 2.0, CC_A);
    check("ucomisd: 1.0 > 2.0", 0);
    compare_doubles(1.0, 1.0, CC_E);
    check("ucomisd: 1.0 == 1.0", 1);
    x_mov_ri(&a, RAX, 0x7FF8000000000000ULL);  /* NaN */
    x_movq_xr(&a, XMM0, RAX);
    load_double(XMM1, 1.0);
    x_ucomisd(&a, XMM0, XMM1);
    x_setcc_al(&a, CC_P);
    x_movzx_rax_al(&a);
    check("ucomisd: NaN activa la paridad", 1);

    /* Operaciones de bits */
    x_mov_ri(&a, RAX, bits(2.5));
    x_mov_ri(&a, RCX, 0x8000000000000000ULL);
    x_xor_rr(&a, RAX, RCX);
    check("xor con el bit de signo: 2.5 -> -2.5", (int64_t)bits(-2.5));
    x_mov_ri(&a, RAX, 0xF0F0);
    x_mov_ri(&a, R11, 0xFF00);
    x_and_rr(&a, RAX, R11);
    x_mov_ri(&a, RCX, 0x000F);
    x_or_rr(&a, RAX, RCX);
    check("and y or: (0xF0F0 & 0xFF00) | 0xF", 0xF00F);

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
