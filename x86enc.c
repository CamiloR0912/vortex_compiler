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

void asm_patch_u32(Asm *a, int at, uint32_t v) {
    int k;
    for (k = 0; k < 4; k++) a->code.bytes[at + k] = (v >> (8 * k)) & 0xFF;
}

uint64_t asm_bss(Asm *a, int len) {
    uint64_t addr;
    a->bss_len = (a->bss_len + 7) / 8 * 8;     /* alinear a 8 bytes */
    addr = BSS_BASE + a->bss_len;
    a->bss_len += len;
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

/* ==========================================================================
 * Instrucciones con operandos registro o memoria
 *
 * Casi todas siguen el mismo esquema:  REX  opcode  ModR/M  [SIB] [disp32]
 *
 *   ModR/M = mod(2) reg(3) rm(3)
 *     reg  un registro, o una extensión del opcode ("/digito" en el manual)
 *     rm   el otro operando:
 *            mod = 11  rm es un registro
 *            mod = 10  rm es memoria: [registro base + desplazamiento de 32 bits]
 *
 * Para memoria se usa siempre el desplazamiento de 32 bits (mod = 10): es
 * un byte más largo que la forma de 8 bits cuando el desplazamiento es
 * pequeño, pero una sola forma simplifica el codificador.
 * ========================================================================== */

/* Prefijo REX = 0100WRXB. Se omite cuando valdría 0x40 y no hace falta. */
static void rex(Asm *a, int w, int reg, int base) {
    int byte = 0x40 | (w << 3) | ((reg >> 3) & 1) << 2 | ((base >> 3) & 1);
    if (byte != 0x40) buf_byte(&a->code, byte);
}

/* ModR/M con los dos operandos en registros (mod = 11) */
static void modrm_reg(Asm *a, int reg, int rm) {
    buf_byte(&a->code, 0xC0 | (reg & 7) << 3 | (rm & 7));
}

/* ModR/M para [base + disp32] (mod = 10).
 * Cuando la base es rsp o r12 (rm = 100), ese valor de rm no significa
 * "registro base" sino "sigue un byte SIB"; el SIB 0x24 indica base = rsp
 * (o r12) sin índice. */
static void modrm_mem(Asm *a, int reg, Reg base, int32_t disp) {
    buf_byte(&a->code, 0x80 | (reg & 7) << 3 | (base & 7));
    if ((base & 7) == RSP) buf_byte(&a->code, 0x24);
    buf_u32(&a->code, (uint32_t)disp);
}

/* Operación de 64 bits entre registros: REX.W op /r */
static void op_rr(Asm *a, int opcode, int reg, int rm) {
    rex(a, 1, reg, rm);
    buf_byte(&a->code, opcode);
    modrm_reg(a, reg, rm);
}

/* mov dst, src: REX.W 89 /r  (copia src en dst; reg = src, rm = dst) */
void x_mov_rr(Asm *a, Reg dst, Reg src) { op_rr(a, 0x89, src, dst); }

/* mov dst, [base + disp]: REX.W 8B /r  (lee 8 bytes de memoria) */
void x_load(Asm *a, Reg dst, Reg base, int32_t disp) {
    rex(a, 1, dst, base);
    buf_byte(&a->code, 0x8B);
    modrm_mem(a, dst, base, disp);
}

/* mov [base + disp], src: REX.W 89 /r  (escribe 8 bytes en memoria) */
void x_store(Asm *a, Reg base, int32_t disp, Reg src) {
    rex(a, 1, src, base);
    buf_byte(&a->code, 0x89);
    modrm_mem(a, src, base, disp);
}

/* push r: 50+r   pop r: 58+r
 * Apilar resta 8 a rsp y escribe; desapilar lee y suma 8. Son la base del
 * modelo de máquina de pila del generador de código. */
void x_push(Asm *a, Reg r) {
    if (r >= R8) buf_byte(&a->code, 0x41);
    buf_byte(&a->code, 0x50 + (r & 7));
}

void x_pop(Asm *a, Reg r) {
    if (r >= R8) buf_byte(&a->code, 0x41);
    buf_byte(&a->code, 0x58 + (r & 7));
}

/* Aritmética entre registros; el resultado queda en dst.
 *   add dst, src   REX.W 01 /r
 *   sub dst, src   REX.W 29 /r
 *   imul dst, src  REX.W 0F AF /r   (aquí reg = dst y rm = src)
 *   cmp a, b       REX.W 39 /r      (calcula a - b solo para las banderas)
 *   test a, b      REX.W 85 /r      (calcula a AND b solo para las banderas) */
void x_add_rr(Asm *a, Reg dst, Reg src) { op_rr(a, 0x01, src, dst); }
void x_sub_rr(Asm *a, Reg dst, Reg src) { op_rr(a, 0x29, src, dst); }
void x_cmp_rr(Asm *a, Reg x, Reg y)     { op_rr(a, 0x39, y, x); }
void x_test_rr(Asm *a, Reg x, Reg y)    { op_rr(a, 0x85, y, x); }

void x_imul_rr(Asm *a, Reg dst, Reg src) {
    rex(a, 1, dst, src);
    buf_byte(&a->code, 0x0F);
    buf_byte(&a->code, 0xAF);
    modrm_reg(a, dst, src);
}

/* add r, imm32: REX.W 81 /0 id     sub r, imm32: REX.W 81 /5 id
 * El inmediato de 32 bits se extiende con signo a 64. Se usan para reservar
 * y liberar espacio en la pila (sub rsp, N). */
void x_add_ri(Asm *a, Reg r, int32_t imm) {
    rex(a, 1, 0, r);
    buf_byte(&a->code, 0x81);
    modrm_reg(a, 0, r);
    buf_u32(&a->code, (uint32_t)imm);
}

void x_sub_ri(Asm *a, Reg r, int32_t imm) {
    rex(a, 1, 0, r);
    buf_byte(&a->code, 0x81);
    modrm_reg(a, 5, r);
    buf_u32(&a->code, (uint32_t)imm);
}

/* neg r: REX.W F7 /3  (r = -r) */
void x_neg(Asm *a, Reg r) {
    rex(a, 1, 0, r);
    buf_byte(&a->code, 0xF7);
    modrm_reg(a, 3, r);
}

/* División entera con signo.
 *   cqo        REX.W 99   extiende el signo de rax a rdx: el dividendo
 *                         de 128 bits es rdx:rax
 *   idiv r     REX.W F7 /7  rax = rdx:rax / r (truncando hacia cero),
 *                           rdx = resto, con el signo del dividendo
 * Si r vale 0 el procesador genera una excepción; el generador de código
 * debe comprobar el divisor antes. */
void x_cqo(Asm *a) {
    buf_byte(&a->code, 0x48);
    buf_byte(&a->code, 0x99);
}

void x_idiv(Asm *a, Reg r) {
    rex(a, 1, 0, r);
    buf_byte(&a->code, 0xF7);
    modrm_reg(a, 7, r);
}

/* div r: REX.W F7 /6  división SIN signo: rax = rdx:rax / r, rdx = resto.
 * La usa la conversión de enteros a texto: después de negar un número
 * negativo, el resultado se interpreta sin signo. Así funciona incluso
 * con el entero más negativo, -9223372036854775808, cuyo negado no cabe
 * en un entero con signo de 64 bits pero sí en uno sin signo. */
void x_div(Asm *a, Reg r) {
    rex(a, 1, 0, r);
    buf_byte(&a->code, 0xF7);
    modrm_reg(a, 6, r);
}

/* mov byte [base + disp], src8: [REX] 88 /r  (escribe solo el byte bajo de src)
 * Para los registros 4 a 7 hace falta un prefijo REX aunque valga 0x40:
 * sin él, esos números significan ah, ch, dh y bh en lugar de spl, bpl,
 * sil y dil. */
void x_store8(Asm *a, Reg base, int32_t disp, Reg src) {
    int byte = 0x40 | ((src >> 3) & 1) << 2 | ((base >> 3) & 1);
    if (byte != 0x40 || (src >= RSP && src <= RDI)) buf_byte(&a->code, byte);
    buf_byte(&a->code, 0x88);
    modrm_mem(a, src, base, disp);
}

/* rep movsb: F3 A4
 * Copia rcx bytes desde la dirección rsi a la dirección rdi; al terminar,
 * rsi y rdi quedan después del bloque copiado y rcx en 0. */
void x_rep_movsb(Asm *a) {
    buf_byte(&a->code, 0xF3);
    buf_byte(&a->code, 0xA4);
}

/* Operaciones de bits entre registros:
 *   and dst, src   REX.W 21 /r
 *   or  dst, src   REX.W 09 /r
 *   xor dst, src   REX.W 31 /r */
void x_and_rr(Asm *a, Reg dst, Reg src) { op_rr(a, 0x21, src, dst); }
void x_or_rr(Asm *a, Reg dst, Reg src)  { op_rr(a, 0x09, src, dst); }
void x_xor_rr(Asm *a, Reg dst, Reg src) { op_rr(a, 0x31, src, dst); }

/* ==========================================================================
 * Punto flotante con SSE2
 *
 * Forma:  [prefijo] [REX] 0F opcode ModR/M
 * El prefijo obligatorio (66 o F2) va ANTES del REX; si se invierte el
 * orden, el procesador ignora el REX.
 *   F2 = operación escalar de doble precisión (…sd)
 *   66 = movq y ucomisd
 * ========================================================================== */
static void sse(Asm *a, int prefix, int w, int opcode, int reg, int rm) {
    buf_byte(&a->code, prefix);
    rex(a, w, reg, rm);
    buf_byte(&a->code, 0x0F);
    buf_byte(&a->code, opcode);
    modrm_reg(a, reg, rm);
}

/* movq xmm, r64: 66 REX.W 0F 6E /r    movq r64, xmm: 66 REX.W 0F 7E /r
 * Copian los 64 bits sin convertirlos: así un double viaja en rax igual
 * que cualquier otro valor de la máquina de pila. */
void x_movq_xr(Asm *a, Xmm dst, Reg src) { sse(a, 0x66, 1, 0x6E, dst, src); }
void x_movq_rx(Asm *a, Reg dst, Xmm src) { sse(a, 0x66, 1, 0x7E, src, dst); }

/* Aritmética escalar de doble precisión: F2 0F 58/5C/59/5E /r */
void x_addsd(Asm *a, Xmm dst, Xmm src) { sse(a, 0xF2, 0, 0x58, dst, src); }
void x_subsd(Asm *a, Xmm dst, Xmm src) { sse(a, 0xF2, 0, 0x5C, dst, src); }
void x_mulsd(Asm *a, Xmm dst, Xmm src) { sse(a, 0xF2, 0, 0x59, dst, src); }
void x_divsd(Asm *a, Xmm dst, Xmm src) { sse(a, 0xF2, 0, 0x5E, dst, src); }

/* ucomisd x, y: 66 0F 2E /r
 * Compara y deja el resultado en banderas de comparación SIN signo:
 *   x > y: CF=0 ZF=0    x < y: CF=1 ZF=0    x == y: CF=0 ZF=1
 *   si alguno es NaN ("no ordenados"): ZF=1 PF=1 CF=1
 * Por eso después se usan las condiciones A, AE, B, BE, y P para el NaN. */
void x_ucomisd(Asm *a, Xmm x, Xmm y) { sse(a, 0x66, 0, 0x2E, x, y); }

/* cvtsi2sd xmm, r64: F2 REX.W 0F 2A /r  (entero con signo -> double)
 * cvtsd2si r64, xmm: F2 REX.W 0F 2D /r  (double -> entero, redondeando al
 *                    más cercano y, en caso de empate, al par) */
void x_cvtsi2sd(Asm *a, Xmm dst, Reg src) { sse(a, 0xF2, 1, 0x2A, dst, src); }
void x_cvtsd2si(Asm *a, Reg dst, Xmm src) { sse(a, 0xF2, 1, 0x2D, dst, src); }

/* set<cc> al: 0F 90+cc /0  pone al = 1 si se cumple la condición, si no 0.
 * movzx rax, al: REX.W 0F B6 /r  copia al en rax rellenando con ceros.
 * Juntas convierten el resultado de una comparación en el valor 0 o 1. */
void x_setcc_al(Asm *a, Cond cc) {
    buf_byte(&a->code, 0x0F);
    buf_byte(&a->code, 0x90 + cc);
    modrm_reg(a, 0, RAX);
}

void x_movzx_rax_al(Asm *a) {
    buf_byte(&a->code, 0x48);
    buf_byte(&a->code, 0x0F);
    buf_byte(&a->code, 0xB6);
    modrm_reg(a, RAX, RAX);
}
