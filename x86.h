/* ==========================================================================
 * x86.h - Codificador de instrucciones x86-64 (el "ensamblador" de vortexc)
 *
 * vortexc no usa un ensamblador externo: cada instrucción se convierte aquí
 * mismo en los bytes que la representan. Los bytes se acumulan en dos
 * buffers que después elf.c escribe en el archivo ejecutable:
 *
 *   code   instrucciones del programa         (segmento de lectura y ejecución)
 *   data   literales: cadenas, mensajes, ...   (segmento de lectura y escritura)
 *
 * Direcciones fijas en memoria (el ejecutable no es reubicable):
 *
 *   CODE_BASE  0x400000     donde el sistema carga el archivo; el código
 *                           empieza justo después de las cabeceras ELF
 *   DATA_BASE  0x10000000   donde se carga el segmento de datos
 *
 * Como DATA_BASE no depende del tamaño del código, la dirección de un dato
 * se conoce en el momento de crearlo y puede escribirse directamente en una
 * instrucción.
 * ========================================================================== */
#ifndef X86_H
#define X86_H

#include <stdint.h>

#define CODE_BASE  0x400000ULL
#define DATA_BASE  0x10000000ULL

/* ---- Buffer de bytes que crece según se necesite ---- */
typedef struct {
    unsigned char *bytes;
    int len, cap;
} Buf;

/* ---- Registros de propósito general de 64 bits ----
 * El número es el que usa la codificación de las instrucciones. Los
 * registros 8 a 15 necesitan un bit extra en el prefijo REX. */
typedef enum {
    RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI,
    R8, R9, R10, R11, R12, R13, R14, R15
} Reg;

/* ---- Etiquetas ----
 * Una etiqueta nombra una posición del código. Puede usarse en un salto
 * antes de saber dónde queda (salto hacia adelante): el salto se emite con
 * un desplazamiento provisional y se anota como pendiente. asm_finish
 * calcula todos los desplazamientos cuando ya se conocen las posiciones
 * (relleno posterior o backpatching). */
typedef int Label;

typedef struct {
    int at;         /* posición en el código del campo rel32 a completar */
    Label target;
} Fixup;

/* ---- El programa que se está ensamblando ---- */
typedef struct {
    Buf code;
    Buf data;
    int *labels;            /* posición de cada etiqueta; -1 si aún no se fijó */
    int nlabels, cap_labels;
    Fixup *fixups;          /* saltos pendientes de completar */
    int nfixups, cap_fixups;
} Asm;

void asm_init(Asm *a);

Label asm_label(Asm *a);            /* crea una etiqueta todavía sin posición */
void asm_bind(Asm *a, Label l);     /* la fija en la posición actual del código */
void asm_finish(Asm *a);            /* completa los saltos; error si falta una etiqueta */

/* Condiciones de los saltos condicionales: el valor es el que se suma
 * al opcode 0F 80 (manual de Intel, instrucción Jcc). */
typedef enum {
    CC_E = 0x4,  CC_NE = 0x5,   /* igual / distinto (ZF)            */
    CC_L = 0xC,  CC_GE = 0xD,   /* menor / mayor o igual, con signo */
    CC_LE = 0xE, CC_G = 0xF     /* menor o igual / mayor, con signo */
} Cond;

/* Agrega bytes al final de un buffer */
void buf_byte(Buf *b, int byte);
void buf_u32(Buf *b, uint32_t v);
void buf_u64(Buf *b, uint64_t v);
void buf_bytes(Buf *b, const void *src, int len);

/* Copia len bytes al segmento de datos y devuelve su dirección en memoria */
uint64_t asm_data(Asm *a, const void *src, int len);

/* ---- Instrucciones ---- */
void x_mov_ri(Asm *a, Reg dst, uint64_t imm);   /* mov dst, imm   */
void x_syscall(Asm *a);                         /* syscall        */
void x_dec(Asm *a, Reg r);                      /* dec r          */
void x_jmp(Asm *a, Label l);                    /* jmp l          */
void x_jcc(Asm *a, Cond cc, Label l);           /* j<cc> l        */
void x_call(Asm *a, Label l);                   /* call l         */
void x_ret(Asm *a);                             /* ret            */

void x_mov_rr(Asm *a, Reg dst, Reg src);                /* mov dst, src           */
void x_load(Asm *a, Reg dst, Reg base, int32_t disp);   /* mov dst, [base+disp]   */
void x_store(Asm *a, Reg base, int32_t disp, Reg src);  /* mov [base+disp], src   */
void x_push(Asm *a, Reg r);                             /* push r                 */
void x_pop(Asm *a, Reg r);                              /* pop r                  */
void x_add_rr(Asm *a, Reg dst, Reg src);                /* add dst, src           */
void x_sub_rr(Asm *a, Reg dst, Reg src);                /* sub dst, src           */
void x_imul_rr(Asm *a, Reg dst, Reg src);               /* imul dst, src          */
void x_cmp_rr(Asm *a, Reg x, Reg y);                    /* cmp x, y               */
void x_test_rr(Asm *a, Reg x, Reg y);                   /* test x, y              */
void x_add_ri(Asm *a, Reg r, int32_t imm);              /* add r, imm             */
void x_sub_ri(Asm *a, Reg r, int32_t imm);              /* sub r, imm             */
void x_neg(Asm *a, Reg r);                              /* neg r                  */
void x_cqo(Asm *a);                                     /* cqo                    */
void x_idiv(Asm *a, Reg r);                             /* idiv r                 */
void x_div(Asm *a, Reg r);                              /* div r (sin signo)      */
void x_store8(Asm *a, Reg base, int32_t disp, Reg src); /* mov byte [base+disp], src8 */
void x_rep_movsb(Asm *a);                               /* rep movsb              */
void x_setcc_al(Asm *a, Cond cc);                       /* set<cc> al             */
void x_movzx_rax_al(Asm *a);                            /* movzx rax, al          */

/* ---- elf.c ----
 * Escribe el ejecutable; entry es la posición de la primera instrucción
 * dentro del código. Devuelve 0 si todo salió bien. */
int elf_write(const char *path, const Asm *a, int entry);

/* Números de las llamadas al sistema de Linux x86-64 que usa el compilador */
#define SYS_WRITE  1
#define SYS_EXIT   60

#endif
