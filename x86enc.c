/* ==========================================================================
 * x86enc.c - Codificación de instrucciones x86-64
 *
 * Formato general de una instrucción x86-64 (manual de Intel, vol. 2, cap. 2):
 *
 *   [REX] opcode [ModR/M] [SIB] [desplazamiento] [inmediato]
 *
 *   REX      prefijo 0100WRXB. W=1: operandos de 64 bits. R, X y B son el
 *            cuarto bit de los registros de los campos reg, index y base,
 *            necesarios para nombrar r8..r15.
 *   opcode   uno o más bytes que identifican la operación.
 *   ModR/M   modo de direccionamiento y registros (se usa desde la etapa 2).
 *   inmediato valor constante incluido en la instrucción, little endian.
 * ========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "x86.h"

/* ---- Buffers ---- */

static void buf_reserve(Buf *b, int extra) {
    if (b->len + extra <= b->cap) return;
    while (b->len + extra > b->cap) b->cap = b->cap ? b->cap * 2 : 256;
    b->bytes = realloc(b->bytes, b->cap);
    if (b->bytes == NULL) { fprintf(stderr, "vortexc: out of memory\n"); exit(1); }
}

void buf_byte(Buf *b, int byte) {
    buf_reserve(b, 1);
    b->bytes[b->len++] = (unsigned char)byte;
}

/* x86 es little endian: el byte menos significativo va primero */
void buf_u32(Buf *b, uint32_t v) {
    int i;
    for (i = 0; i < 4; i++) buf_byte(b, (v >> (8 * i)) & 0xFF);
}

void buf_u64(Buf *b, uint64_t v) {
    int i;
    for (i = 0; i < 8; i++) buf_byte(b, (v >> (8 * i)) & 0xFF);
}

void buf_bytes(Buf *b, const void *src, int len) {
    buf_reserve(b, len);
    memcpy(b->bytes + b->len, src, len);
    b->len += len;
}

void asm_init(Asm *a) {
    memset(a, 0, sizeof *a);
}

uint64_t asm_data(Asm *a, const void *src, int len) {
    uint64_t addr = DATA_BASE + a->data.len;
    buf_bytes(&a->data, src, len);
    return addr;
}

/* ---- Instrucciones ---- */

/* mov dst, imm
 *
 * Si el valor cabe en 32 bits sin signo se usa la forma corta
 *     [REX.B] B8+r imm32          (5 o 6 bytes)
 * que escribe los 32 bits bajos del registro; el procesador pone en cero
 * los 32 bits altos, así que el resultado es el mismo valor de 64 bits.
 * Si no cabe, se usa la forma larga
 *     REX.W B8+r imm64            (10 bytes)
 * En ambas el registro va sumado al opcode (B8 + sus 3 bits bajos) y el
 * cuarto bit, si lo hay, va en REX.B. */
void x_mov_ri(Asm *a, Reg dst, uint64_t imm) {
    if (imm <= 0xFFFFFFFFULL) {
        if (dst >= R8) buf_byte(&a->code, 0x41);           /* REX.B */
        buf_byte(&a->code, 0xB8 + (dst & 7));
        buf_u32(&a->code, (uint32_t)imm);
    } else {
        buf_byte(&a->code, 0x48 | (dst >= R8 ? 1 : 0));     /* REX.W [+B] */
        buf_byte(&a->code, 0xB8 + (dst & 7));
        buf_u64(&a->code, imm);
    }
}

/* syscall: 0F 05
 * Pasa el control al núcleo de Linux. Convención (System V AMD64):
 *   rax = número de la llamada, argumentos en rdi, rsi, rdx, r10, r8, r9,
 *   resultado en rax. La instrucción destruye rcx y r11. */
void x_syscall(Asm *a) {
    buf_byte(&a->code, 0x0F);
    buf_byte(&a->code, 0x05);
}
