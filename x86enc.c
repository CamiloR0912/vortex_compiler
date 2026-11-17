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
 *   ModR/M   modo de direccionamiento y registros de los operandos.
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

/* ---- Etiquetas y relleno posterior ---- */

Label asm_label(Asm *a) {
    if (a->nlabels == a->cap_labels) {
        a->cap_labels = a->cap_labels ? a->cap_labels * 2 : 64;
        a->labels = realloc(a->labels, a->cap_labels * sizeof *a->labels);
    }
    a->labels[a->nlabels] = -1;
    return a->nlabels++;
}

void asm_bind(Asm *a, Label l) {
    a->labels[l] = a->code.len;
}

/* Emite un campo rel32 provisional (cero) y lo anota como pendiente */
static void rel32_to(Asm *a, Label l) {
    if (a->nfixups == a->cap_fixups) {
        a->cap_fixups = a->cap_fixups ? a->cap_fixups * 2 : 64;
        a->fixups = realloc(a->fixups, a->cap_fixups * sizeof *a->fixups);
    }
    a->fixups[a->nfixups].at = a->code.len;
    a->fixups[a->nfixups].target = l;
    a->nfixups++;
    buf_u32(&a->code, 0);
}

/* Los saltos de x86 son relativos: el desplazamiento se mide desde el
 * final de la instrucción, que es justo donde termina el campo rel32.
 *     rel32 = destino - (posición del campo + 4)
 * Un valor negativo salta hacia atrás. */
void asm_finish(Asm *a) {
    int i;
    for (i = 0; i < a->nfixups; i++) {
        Fixup *f = &a->fixups[i];
        int target = a->labels[f->target];
        int32_t rel;
        int k;
        if (target < 0) {
            fprintf(stderr, "vortexc: internal error: label %d never bound\n", f->target);
            exit(1);
        }
        rel = target - (f->at + 4);
        for (k = 0; k < 4; k++) a->code.bytes[f->at + k] = ((uint32_t)rel >> (8 * k)) & 0xFF;
    }
    a->nfixups = 0;
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

/* dec r: REX.W [+B] FF /1
 * "/1" significa que el campo reg del byte ModR/M no nombra un registro
 * sino que completa el opcode (FF con reg=1 es dec). El byte ModR/M es
 *     mod(2 bits) reg(3 bits) rm(3 bits)
 * mod=11 indica que el operando rm es un registro, no memoria. */
void x_dec(Asm *a, Reg r) {
    buf_byte(&a->code, 0x48 | (r >= R8 ? 1 : 0));
    buf_byte(&a->code, 0xFF);
    buf_byte(&a->code, 0xC0 | (1 << 3) | (r & 7));
}

/* jmp rel32: E9 rel32 */
void x_jmp(Asm *a, Label l) {
    buf_byte(&a->code, 0xE9);
    rel32_to(a, l);
}

/* j<cc> rel32: 0F 80+cc rel32
 * Salta si se cumple la condición sobre las banderas que dejó la última
 * operación aritmética o comparación. */
void x_jcc(Asm *a, Cond cc, Label l) {
    buf_byte(&a->code, 0x0F);
    buf_byte(&a->code, 0x80 + cc);
    rel32_to(a, l);
}

/* call rel32: E8 rel32
 * Apila la dirección de la instrucción siguiente y salta. */
void x_call(Asm *a, Label l) {
    buf_byte(&a->code, 0xE8);
    rel32_to(a, l);
}

/* ret: C3. Desapila la dirección de retorno y salta a ella. */
void x_ret(Asm *a) {
    buf_byte(&a->code, 0xC3);
}
