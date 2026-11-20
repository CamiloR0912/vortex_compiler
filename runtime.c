/* ==========================================================================
 * runtime.c - Código máquina de las rutinas de ejecución
 *
 * Cada rutina se presenta primero en ensamblador (sintaxis Intel) y debajo
 * las llamadas al codificador que la producen, una por instrucción.
 * ========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "runtime.h"

#define POW10_MIN (-324)
#define POW10_MAX 308

/* Tabla de potencias de 10 como doubles, de 10^-324 a 10^308. Las calcula
 * strtod de vortexc (el compilador, no el programa generado), que entrega
 * el double más cercano a cada potencia. Las potencias de 10^0 a 10^22 son
 * exactas; las demás tienen el error de redondeo de un double. */
static uint64_t pow10_table(Asm *a) {
    uint64_t addr = DATA_BASE + a->data.len;
    int k;
    for (k = POW10_MIN; k <= POW10_MAX; k++) {
        char text[16];
        double v;
        uint64_t bits;
        snprintf(text, sizeof text, "1e%d", k);
        v = strtod(text, NULL);
        memcpy(&bits, &v, sizeof bits);
        buf_u64(&a->data, bits);
    }
    return addr;
}

void rt_init(Asm *a, Runtime *rt) {
    rt->write = asm_label(a);
    rt->write_all = asm_label(a);
    rt->flush = asm_label(a);
    rt->fmt_int = asm_label(a);
    rt->print_int = asm_label(a);
    rt->print_bool = asm_label(a);
    rt->print_str = asm_label(a);
    rt->exit = asm_label(a);
    rt->error = asm_label(a);
    rt->alloc = asm_label(a);
    rt->strlen = asm_label(a);
    rt->copy_str = asm_label(a);
    rt->str_int = asm_label(a);
    rt->str_bool = asm_label(a);
    rt->concat = asm_label(a);
    rt->strcmp = asm_label(a);
    rt->fmt_float = asm_label(a);
    rt->print_float = asm_label(a);
    rt->str_float = asm_label(a);
    rt->out_len = asm_bss(a, 8);
    rt->depth = asm_bss(a, 8);
    rt->heap_ptr = asm_bss(a, 8);
    rt->heap_end = asm_bss(a, 8);
    rt->num_buf = asm_bss(a, 128);
    rt->out_buf = asm_bss(a, OUT_BUF_SIZE);
    rt->text_true = rt_literal(a, "true", 4);
    rt->text_false = rt_literal(a, "false", 5);
    rt->pow10 = pow10_table(a);
}

uint64_t rt_literal(Asm *a, const char *text, int len) {
    uint64_t addr = asm_data(a, text, len);
    buf_byte(&a->data, 0);
    return addr;
}

void rt_print_text(Asm *a, Runtime *rt, const char *text) {
    int len = (int)strlen(text);
    x_mov_ri(a, RSI, asm_data(a, text, len));
    x_mov_ri(a, RDX, len);
    x_call(a, rt->write);
}

void rt_fail(Asm *a, Runtime *rt, int line, const char *msg) {
    int len = (int)strlen(msg);
    x_mov_ri(a, RSI, asm_data(a, msg, len));
    x_mov_ri(a, RDX, len);
    x_mov_ri(a, RAX, line);
    x_call(a, rt->error);
}

/* write_all: escribe rdx bytes desde rsi en la salida estándar.
 * write puede escribir menos bytes de los pedidos; por eso se repite hasta
 * terminar. Si write falla (devuelve 0 o un negativo) se abandona.
 *
 *   ciclo:  test rdx, rdx
 *           je fin
 *           mov rax, 1          ; write
 *           mov rdi, 1          ; salida estándar
 *           syscall             ; rax = bytes escritos (rsi y rdx se conservan)
 *           test rax, rax
 *           jle fin
 *           add rsi, rax
 *           sub rdx, rax
 *           jmp ciclo
 *   fin:    ret
 */
static void emit_write_all(Asm *a, Runtime *rt) {
    Label ciclo = asm_label(a), fin = asm_label(a);
    asm_bind(a, rt->write_all);
    asm_bind(a, ciclo);
    x_test_rr(a, RDX, RDX);
    x_jcc(a, CC_E, fin);
    x_mov_ri(a, RAX, SYS_WRITE);
    x_mov_ri(a, RDI, 1);
    x_syscall(a);
    x_test_rr(a, RAX, RAX);
    x_jcc(a, CC_LE, fin);
    x_add_rr(a, RSI, RAX);
    x_sub_rr(a, RDX, RAX);
    x_jmp(a, ciclo);
    asm_bind(a, fin);
    x_ret(a);
}

/* flush: escribe lo acumulado en el buffer y lo deja vacío.
 *
 *           mov rsi, out_buf
 *           mov r8, out_len
 *           mov rdx, [r8]
 *           call write_all
 *           mov r8, out_len
 *           mov rax, 0
 *           mov [r8], rax
 *           ret
 */
static void emit_flush(Asm *a, Runtime *rt) {
    asm_bind(a, rt->flush);
    x_mov_ri(a, RSI, rt->out_buf);
    x_mov_ri(a, R8, rt->out_len);
    x_load(a, RDX, R8, 0);
    x_call(a, rt->write_all);
    x_mov_ri(a, R8, rt->out_len);
    x_mov_ri(a, RAX, 0);
    x_store(a, R8, 0, RAX);
    x_ret(a);
}

/* write: agrega rdx bytes desde rsi al buffer de salida.
 * Si no caben, primero vacía el buffer. Si ni así caben (texto de más de
 * 4096 bytes), los escribe directamente.
 *
 *           mov r8, out_len
 *           mov rax, [r8]
 *           mov rcx, rax
 *           add rcx, rdx            ; rcx = largo final del buffer
 *           mov r9, 4096
 *           cmp rcx, r9
 *           jle copiar              ; cabe
 *           push rsi
 *           push rdx
 *           call flush
 *           pop rdx
 *           pop rsi
 *           mov r9, 4096
 *           cmp rdx, r9
 *           jle copiar              ; ahora cabe en el buffer vacío
 *           jmp write_all           ; demasiado grande: escribir directamente
 *   copiar: mov r8, out_len
 *           mov rax, [r8]
 *           mov rdi, out_buf
 *           add rdi, rax            ; rdi = primera posición libre del buffer
 *           mov rcx, rdx
 *           rep movsb               ; copia rcx bytes de [rsi] a [rdi]
 *           add rax, rdx
 *           mov [r8], rax           ; out_len += rdx
 *           ret
 */
static void emit_write(Asm *a, Runtime *rt) {
    Label copiar = asm_label(a);
    asm_bind(a, rt->write);
    x_mov_ri(a, R8, rt->out_len);
    x_load(a, RAX, R8, 0);
    x_mov_rr(a, RCX, RAX);
    x_add_rr(a, RCX, RDX);
    x_mov_ri(a, R9, OUT_BUF_SIZE);
    x_cmp_rr(a, RCX, R9);
    x_jcc(a, CC_LE, copiar);
    x_push(a, RSI);
    x_push(a, RDX);
    x_call(a, rt->flush);
    x_pop(a, RDX);
    x_pop(a, RSI);
    x_mov_ri(a, R9, OUT_BUF_SIZE);
    x_cmp_rr(a, RDX, R9);
    x_jcc(a, CC_LE, copiar);
    x_jmp(a, rt->write_all);
    asm_bind(a, copiar);
    x_mov_ri(a, R8, rt->out_len);
    x_load(a, RAX, R8, 0);
    x_mov_ri(a, RDI, rt->out_buf);
    x_add_rr(a, RDI, RAX);
    x_mov_rr(a, RCX, RDX);
    x_rep_movsb(a);
    x_add_rr(a, RAX, RDX);
    x_store(a, R8, 0, RAX);
    x_ret(a);
}

/* fmt_int: escribe rax como entero decimal en el espacio temporal num_buf.
 * Devuelve rsi = dirección del primer carácter y rdx = cantidad.
 * Los dígitos se obtienen dividiendo entre 10: cada resto es un dígito,
 * del último al primero, así que se escriben de derecha a izquierda desde
 * num_buf + 32 (bastan 20 dígitos y el signo). Un negativo se niega y se
 * divide SIN signo: así también funciona con -9223372036854775808, cuyo
 * negado solo existe como número sin signo.
 *
 *           mov r9, 0               ; r9 = 1 si el número es negativo
 *           test rax, rax
 *           jge positivo
 *           mov r9, 1
 *           neg rax
 * positivo: mov rdi, num_buf + 32   ; rdi = fin del espacio
 *           mov rcx, 10
 *  digito:  mov rdx, 0
 *           div rcx                 ; rax = rax / 10, rdx = rax % 10
 *           add rdx, '0'            ; dígito -> carácter
 *           sub rdi, 1
 *           mov [rdi], dl
 *           test rax, rax
 *           jne digito
 *           test r9, r9
 *           je listo
 *           sub rdi, 1
 *           mov rdx, '-'
 *           mov [rdi], dl
 *  listo:   mov rsi, rdi            ; rsi = primer carácter
 *           mov rdx, num_buf + 32
 *           sub rdx, rdi            ; rdx = cantidad de caracteres
 *           ret
 */
static void emit_fmt_int(Asm *a, Runtime *rt) {
    Label positivo = asm_label(a), digito = asm_label(a), listo = asm_label(a);
    asm_bind(a, rt->fmt_int);
    x_mov_ri(a, R9, 0);
    x_test_rr(a, RAX, RAX);
    x_jcc(a, CC_GE, positivo);
    x_mov_ri(a, R9, 1);
    x_neg(a, RAX);
    asm_bind(a, positivo);
    x_mov_ri(a, RDI, rt->num_buf + 32);
    x_mov_ri(a, RCX, 10);
    asm_bind(a, digito);
    x_mov_ri(a, RDX, 0);
    x_div(a, RCX);
    x_add_ri(a, RDX, '0');
    x_sub_ri(a, RDI, 1);
    x_store8(a, RDI, 0, RDX);
    x_test_rr(a, RAX, RAX);
    x_jcc(a, CC_NE, digito);
    x_test_rr(a, R9, R9);
    x_jcc(a, CC_E, listo);
    x_sub_ri(a, RDI, 1);
    x_mov_ri(a, RDX, '-');
    x_store8(a, RDI, 0, RDX);
    asm_bind(a, listo);
    x_mov_rr(a, RSI, RDI);
    x_mov_ri(a, RDX, rt->num_buf + 32);
    x_sub_rr(a, RDX, RDI);
    x_ret(a);
}

/* print_int: imprime rax como entero decimal.
 *           call fmt_int
 *           jmp write               ; el ret de write vuelve a quien llamó
 */
static void emit_print_int(Asm *a, Runtime *rt) {
    asm_bind(a, rt->print_int);
    x_call(a, rt->fmt_int);
    x_jmp(a, rt->write);
}

/* print_bool: imprime "true" o "false" según rax.
 * Termina saltando a write en lugar de llamarla: el ret de write vuelve
 * directamente a quien llamó a print_bool.
 *
 *           test rax, rax
 *           je falso
 *           mov rsi, "true"
 *           mov rdx, 4
 *           jmp write
 *   falso:  mov rsi, "false"
 *           mov rdx, 5
 *           jmp write
 */
static void emit_print_bool(Asm *a, Runtime *rt) {
    Label falso = asm_label(a);
    asm_bind(a, rt->print_bool);
    x_test_rr(a, RAX, RAX);
    x_jcc(a, CC_E, falso);
    x_mov_ri(a, RSI, rt->text_true);
    x_mov_ri(a, RDX, 4);
    x_jmp(a, rt->write);
    asm_bind(a, falso);
    x_mov_ri(a, RSI, rt->text_false);
    x_mov_ri(a, RDX, 5);
    x_jmp(a, rt->write);
}

/* exit: vacía el buffer y termina el proceso con el código rdi.
 *
 *           push rdi
 *           call flush
 *           pop rdi
 *           mov rax, 60             ; exit
 *           syscall
 */
static void emit_exit(Asm *a, Runtime *rt) {
    asm_bind(a, rt->exit);
    x_push(a, RDI);
    x_call(a, rt->flush);
    x_pop(a, RDI);
    x_mov_ri(a, RAX, SYS_EXIT);
    x_syscall(a);
}

/* error: reporta un error de ejecución y termina con código 1.
 * Entrada: rax = línea del programa fuente, rsi y rdx = mensaje.
 * El mensaje se guarda en la pila mientras se imprime el encabezado,
 * porque las otras rutinas modifican rsi y rdx.
 *
 *           push rsi
 *           push rdx
 *           push rax
 *           (imprimir "[Runtime error] line ")
 *           pop rax
 *           call print_int
 *           (imprimir ": ")
 *           pop rdx
 *           pop rsi
 *           call write
 *           (imprimir "\n")
 *           mov rdi, 1
 *           jmp exit
 */
static void emit_error(Asm *a, Runtime *rt) {
    asm_bind(a, rt->error);
    x_push(a, RSI);
    x_push(a, RDX);
    x_push(a, RAX);
    rt_print_text(a, rt, "[Runtime error] line ");
    x_pop(a, RAX);
    x_call(a, rt->print_int);
    rt_print_text(a, rt, ": ");
    x_pop(a, RDX);
    x_pop(a, RSI);
    x_call(a, rt->write);
    rt_print_text(a, rt, "\n");
    x_mov_ri(a, RDI, 1);
    x_jmp(a, rt->exit);
}

/* ===================== Heap ===================== */

/* alloc: reserva rdi bytes y devuelve su dirección en rax.
 * Avanza heap_ptr dentro del bloque actual; si no alcanza, pide al núcleo
 * un bloque nuevo de al menos HEAP_CHUNK bytes con mmap. mmap devuelve la
 * dirección del bloque, o un número negativo (-errno) si falla.
 *
 *           add rdi, 7
 *           mov rcx, -8
 *           and rdi, rcx            ; tamaño redondeado a múltiplo de 8
 *           mov r8, heap_ptr
 *           mov rax, [r8]           ; rax = dirección a entregar
 *           mov rcx, rax
 *           add rcx, rdi            ; rcx = nuevo heap_ptr
 *           mov r9, heap_end
 *           mov rdx, [r9]
 *           cmp rcx, rdx
 *           jbe listo               ; cabe en el bloque actual
 *           mov rsi, HEAP_CHUNK     ; tamaño del bloque nuevo: max(rdi, HEAP_CHUNK)
 *           cmp rdi, rsi
 *           jbe pedir
 *           mov rsi, rdi
 *  pedir:   push rdi
 *           push rsi
 *           mov rax, 9              ; mmap(0, rsi, PROT_READ | PROT_WRITE,
 *           mov rdi, 0              ;      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
 *           mov rdx, 3
 *           mov r10, 0x22
 *           mov r8, -1
 *           mov r9, 0
 *           syscall
 *           pop rsi
 *           pop rdi
 *           test rax, rax
 *           jl sin_memoria
 *           mov rcx, rax
 *           add rcx, rsi
 *           mov r9, heap_end
 *           mov [r9], rcx           ; heap_end = inicio + tamaño del bloque
 *           mov rcx, rax
 *           add rcx, rdi            ; nuevo heap_ptr
 *  listo:   mov r8, heap_ptr
 *           mov [r8], rcx
 *           ret
 *  sin_memoria:
 *           (imprimir "[Runtime error] out of memory")
 *           mov rdi, 1
 *           jmp exit
 */
static void emit_alloc(Asm *a, Runtime *rt) {
    Label pedir = asm_label(a), listo = asm_label(a), sin_memoria = asm_label(a);
    asm_bind(a, rt->alloc);
    x_add_ri(a, RDI, 7);
    x_mov_ri(a, RCX, (uint64_t)-8);
    x_and_rr(a, RDI, RCX);
    x_mov_ri(a, R8, rt->heap_ptr);
    x_load(a, RAX, R8, 0);
    x_mov_rr(a, RCX, RAX);
    x_add_rr(a, RCX, RDI);
    x_mov_ri(a, R9, rt->heap_end);
    x_load(a, RDX, R9, 0);
    x_cmp_rr(a, RCX, RDX);
    x_jcc(a, CC_BE, listo);
    x_mov_ri(a, RSI, HEAP_CHUNK);
    x_cmp_rr(a, RDI, RSI);
    x_jcc(a, CC_BE, pedir);
    x_mov_rr(a, RSI, RDI);
    asm_bind(a, pedir);
    x_push(a, RDI);
    x_push(a, RSI);
    x_mov_ri(a, RAX, SYS_MMAP);
    x_mov_ri(a, RDI, 0);
    x_mov_ri(a, RDX, 3);
    x_mov_ri(a, R10, 0x22);
    x_mov_ri(a, R8, (uint64_t)-1);
    x_mov_ri(a, R9, 0);
    x_syscall(a);
    x_pop(a, RSI);
    x_pop(a, RDI);
    x_test_rr(a, RAX, RAX);
    x_jcc(a, CC_L, sin_memoria);
    x_mov_rr(a, RCX, RAX);
    x_add_rr(a, RCX, RSI);
    x_mov_ri(a, R9, rt->heap_end);
    x_store(a, R9, 0, RCX);
    x_mov_rr(a, RCX, RAX);
    x_add_rr(a, RCX, RDI);
    asm_bind(a, listo);
    x_mov_ri(a, R8, rt->heap_ptr);
    x_store(a, R8, 0, RCX);
    x_ret(a);
    asm_bind(a, sin_memoria);
    rt_print_text(a, rt, "[Runtime error] out of memory\n");
    x_mov_ri(a, RDI, 1);
    x_jmp(a, rt->exit);
}

/* ===================== Cadenas ===================== */

/* strlen: rdx = cantidad de bytes de la cadena rsi antes del 0 final.
 *           mov rcx, rsi
 *  ciclo:   movzx rax, byte [rcx]
 *           test rax, rax
 *           je fin
 *           add rcx, 1
 *           jmp ciclo
 *  fin:     mov rdx, rcx
 *           sub rdx, rsi
 *           ret
 */
static void emit_strlen(Asm *a, Runtime *rt) {
    Label ciclo = asm_label(a), fin = asm_label(a);
    asm_bind(a, rt->strlen);
    x_mov_rr(a, RCX, RSI);
    asm_bind(a, ciclo);
    x_load8(a, RAX, RCX, 0);
    x_test_rr(a, RAX, RAX);
    x_jcc(a, CC_E, fin);
    x_add_ri(a, RCX, 1);
    x_jmp(a, ciclo);
    asm_bind(a, fin);
    x_mov_rr(a, RDX, RCX);
    x_sub_rr(a, RDX, RSI);
    x_ret(a);
}

/* print_str: imprime la cadena rax.
 *           mov rsi, rax
 *           call strlen
 *           jmp write
 */
static void emit_print_str(Asm *a, Runtime *rt) {
    asm_bind(a, rt->print_str);
    x_mov_rr(a, RSI, RAX);
    x_call(a, rt->strlen);
    x_jmp(a, rt->write);
}

/* copy_str: rax = cadena nueva en el heap con los rdx bytes desde rsi y
 * un 0 final.
 *           push rsi
 *           push rdx
 *           mov rdi, rdx
 *           add rdi, 1
 *           call alloc
 *           pop rcx                 ; rcx = largo
 *           pop rsi
 *           mov rdi, rax
 *           rep movsb               ; rdi queda después del último byte
 *           mov rdx, 0
 *           mov [rdi], dl
 *           ret                     ; rax sigue siendo la cadena nueva
 */
static void emit_copy_str(Asm *a, Runtime *rt) {
    asm_bind(a, rt->copy_str);
    x_push(a, RSI);
    x_push(a, RDX);
    x_mov_rr(a, RDI, RDX);
    x_add_ri(a, RDI, 1);
    x_call(a, rt->alloc);
    x_pop(a, RCX);
    x_pop(a, RSI);
    x_mov_rr(a, RDI, RAX);
    x_rep_movsb(a);
    x_mov_ri(a, RDX, 0);
    x_store8(a, RDI, 0, RDX);
    x_ret(a);
}

/* str_int: rax = cadena con el entero rax.
 *           call fmt_int
 *           jmp copy_str
 * str_bool: rax = "true" o "false" (literales: no hace falta copiarlos).
 *           test rax, rax
 *           je falso
 *           mov rax, "true"
 *           ret
 *  falso:   mov rax, "false"
 *           ret
 */
static void emit_str_int_bool(Asm *a, Runtime *rt) {
    Label falso = asm_label(a);
    asm_bind(a, rt->str_int);
    x_call(a, rt->fmt_int);
    x_jmp(a, rt->copy_str);

    asm_bind(a, rt->str_bool);
    x_test_rr(a, RAX, RAX);
    x_jcc(a, CC_E, falso);
    x_mov_ri(a, RAX, rt->text_true);
    x_ret(a);
    asm_bind(a, falso);
    x_mov_ri(a, RAX, rt->text_false);
    x_ret(a);
}

/* concat: rax = cadena nueva con la cadena rax seguida de la cadena rcx.
 * Los largos y las direcciones se guardan en la pila porque las rutinas
 * que se llaman modifican los registros.
 *           push rax                ; a
 *           push rcx                ; b
 *           mov rsi, rax
 *           call strlen
 *           push rdx                ; la
 *           mov rsi, [rsp+8]
 *           call strlen
 *           push rdx                ; lb   (pila: lb, la, b, a)
 *           mov rdi, [rsp+8]
 *           add rdi, rdx
 *           add rdi, 1              ; la + lb + 1
 *           call alloc
 *           mov rdi, rax
 *           mov rsi, [rsp+24]
 *           mov rcx, [rsp+8]
 *           rep movsb               ; copia a
 *           mov rsi, [rsp+16]
 *           mov rcx, [rsp]
 *           add rcx, 1
 *           rep movsb               ; copia b con su 0 final
 *           add rsp, 32
 *           ret
 */
static void emit_concat(Asm *a, Runtime *rt) {
    asm_bind(a, rt->concat);
    x_push(a, RAX);
    x_push(a, RCX);
    x_mov_rr(a, RSI, RAX);
    x_call(a, rt->strlen);
    x_push(a, RDX);
    x_load(a, RSI, RSP, 8);
    x_call(a, rt->strlen);
    x_push(a, RDX);
    x_load(a, RDI, RSP, 8);
    x_add_rr(a, RDI, RDX);
    x_add_ri(a, RDI, 1);
    x_call(a, rt->alloc);
    x_mov_rr(a, RDI, RAX);
    x_load(a, RSI, RSP, 24);
    x_load(a, RCX, RSP, 8);
    x_rep_movsb(a);
    x_load(a, RSI, RSP, 16);
    x_load(a, RCX, RSP, 0);
    x_add_ri(a, RCX, 1);
    x_rep_movsb(a);
    x_add_ri(a, RSP, 32);
    x_ret(a);
}

/* strcmp: compara las cadenas rax y rcx byte a byte, como strcmp de C
 * (los bytes se comparan sin signo). rax = -1, 0 o 1.
 *  ciclo:   movzx rdx, byte [rax]
 *           movzx r8, byte [rcx]
 *           cmp rdx, r8
 *           jne distintos
 *           test rdx, rdx
 *           je iguales              ; las dos terminaron
 *           add rax, 1
 *           add rcx, 1
 *           jmp ciclo
 *  distintos:
 *           cmp rdx, r8
 *           jl menor
 *           mov rax, 1
 *           ret
 *  menor:   mov rax, -1
 *           ret
 *  iguales: mov rax, 0
 *           ret
 */
static void emit_strcmp(Asm *a, Runtime *rt) {
    Label ciclo = asm_label(a), distintos = asm_label(a), menor = asm_label(a), iguales = asm_label(a);
    asm_bind(a, rt->strcmp);
    asm_bind(a, ciclo);
    x_load8(a, RDX, RAX, 0);
    x_load8(a, R8, RCX, 0);
    x_cmp_rr(a, RDX, R8);
    x_jcc(a, CC_NE, distintos);
    x_test_rr(a, RDX, RDX);
    x_jcc(a, CC_E, iguales);
    x_add_ri(a, RAX, 1);
    x_add_ri(a, RCX, 1);
    x_jmp(a, ciclo);
    asm_bind(a, distintos);
    x_cmp_rr(a, RDX, R8);
    x_jcc(a, CC_L, menor);
    x_mov_ri(a, RAX, 1);
    x_ret(a);
    asm_bind(a, menor);
    x_mov_ri(a, RAX, (uint64_t)-1);
    x_ret(a);
    asm_bind(a, iguales);
    x_mov_ri(a, RAX, 0);
    x_ret(a);
}

/* ===================== Reales a texto =====================
 *
 * fmt_float reproduce lo que hacía la versión anterior:
 *     snprintf(buf, "%.15g", x) y, si el texto no tiene '.', 'e' ni 'n',
 *     agregar ".0"            (5 -> "5.0", 6.28318 -> "6.28318", 1e+20)
 *
 * Algoritmo, con x > 0 (el signo se escribe aparte):
 *   1. E = exponente decimal: 10^E <= x < 10^(E+1). Se estima a partir del
 *      exponente binario e2 del double, E ~ e2 * log10(2) (con enteros:
 *      e2 * 78913 / 2^18), y se ajusta comparando x con la tabla pow10.
 *   2. N = redondear(x * 10^(14 - E)): los 15 dígitos significativos.
 *      Para 10^k con k <= 22 la tabla es exacta y hay un solo redondeo.
 *      Si el redondeo da 10^15 (por ejemplo 9.9999999999999996), E sube
 *      y se recalcula; si da menos de 10^14, E baja.
 *   3. Se escriben los 15 dígitos y se descartan los ceros finales: quedan
 *      k dígitos significativos.
 *   4. Como %g: notación normal si -4 <= E < 15; si no, científica
 *      (d.ddde+XX, con al menos dos dígitos de exponente).
 *
 * Registros: rdi = siguiente posición del texto, xmm0 = x, r8 = E,
 * r9 = N, r10 = k. El texto se arma en num_buf + 32 y los 15 dígitos de
 * N en num_buf + 96.
 */
#define FLOAT_TEXT 32
#define FLOAT_DIGITS 96

/* mov rdx, c / mov [rdi], dl / add rdi, 1 */
static void put_char(Asm *a, int c) {
    x_mov_ri(a, RDX, c);
    x_store8(a, RDI, 0, RDX);
    x_add_ri(a, RDI, 1);
}

/* xmm = pow10[idx] (idx en un registro; usa rcx y rdx)
 *           mov rcx, idx
 *           add rcx, 324
 *           shl rcx, 3              ; 8 bytes por entrada
 *           mov rdx, pow10
 *           add rdx, rcx
 *           mov rdx, [rdx]
 *           movq xmm, rdx
 */
static void load_pow10(Asm *a, Runtime *rt, Xmm x, Reg idx) {
    x_mov_rr(a, RCX, idx);
    x_add_ri(a, RCX, -POW10_MIN);
    x_shl_ri(a, RCX, 3);
    x_mov_ri(a, RDX, rt->pow10);
    x_add_rr(a, RDX, RCX);
    x_load(a, RDX, RDX, 0);
    x_movq_xr(a, x, RDX);
}

/* xmm = el double v (pasando por rcx) */
static void load_double(Asm *a, Xmm x, double v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof bits);
    x_mov_ri(a, RCX, bits);
    x_movq_xr(a, x, RCX);
}

/* xmm7 = error exacto del producto A * B, es decir A * B - P, donde P es
 * el producto ya redondeado (algoritmo TwoProduct de Dekker, con la
 * partición de Veltkamp). Cada factor se parte en dos mitades de 26 bits
 * (alto + bajo); los productos de las mitades son exactos, y sumados en
 * este orden dan el error sin redondeo. Usa xmm1, xmm4, xmm5, xmm6, xmm7
 * y rcx; A, B y P no pueden ser ninguno de ellos.
 *   C = 2^27 + 1
 *   c = A * C;  ah = c - (c - A);  al = A - ah        (xmm4, xmm5)
 *   c = B * C;  bh = c - (c - B);  bl = B - bh        (xmm6, xmm1)
 *   error = ((ah * bh - P) + ah * bl + al * bh) + al * bl
 */
static void two_product_error(Asm *a, Xmm A, Xmm B, Xmm P) {
    load_double(a, XMM1, 134217729.0);      /* 2^27 + 1 */
    x_movsd_rr(a, XMM4, A);
    x_mulsd(a, XMM4, XMM1);
    x_movsd_rr(a, XMM6, XMM4);
    x_subsd(a, XMM6, A);
    x_subsd(a, XMM4, XMM6);                 /* ah */
    x_movsd_rr(a, XMM5, A);
    x_subsd(a, XMM5, XMM4);                 /* al */
    x_movsd_rr(a, XMM6, B);
    x_mulsd(a, XMM6, XMM1);
    x_movsd_rr(a, XMM7, XMM6);
    x_subsd(a, XMM7, B);
    x_subsd(a, XMM6, XMM7);                 /* bh */
    x_movsd_rr(a, XMM1, B);
    x_subsd(a, XMM1, XMM6);                 /* bl */
    x_movsd_rr(a, XMM7, XMM4);
    x_mulsd(a, XMM7, XMM6);
    x_subsd(a, XMM7, P);                    /* ah*bh - P */
    x_mulsd(a, XMM4, XMM1);
    x_addsd(a, XMM7, XMM4);                 /* + ah*bl */
    x_mulsd(a, XMM6, XMM5);
    x_addsd(a, XMM7, XMM6);                 /* + al*bh */
    x_mulsd(a, XMM5, XMM1);
    x_addsd(a, XMM7, XMM5);                 /* + al*bl */
}

/* Copia rcx bytes desde rsi al texto (rdi avanza) */
static void copy_digits(Asm *a) {
    x_rep_movsb(a);
}

static void emit_fmt_float(Asm *a, Runtime *rt) {
    Label positivo = asm_label(a), finito = asm_label(a), es_nan = asm_label(a);
    Label no_cero = asm_label(a), bajar = asm_label(a), subir = asm_label(a);
    Label exponente_listo = asm_label(a), calcular = asm_label(a);
    Label dividir = asm_label(a), redondear = asm_label(a), no_grande = asm_label(a);
    Label paso = asm_label(a), mult_ok = asm_label(a), div_ok = asm_label(a);
    Label no_mitad_arriba = asm_label(a), ajustado = asm_label(a);
    Label n_listo = asm_label(a), digito = asm_label(a), contar = asm_label(a);
    Label contado = asm_label(a), cientifico = asm_label(a), fijo_negativo = asm_label(a);
    Label ceros = asm_label(a), ceros_listos = asm_label(a), un_digito = asm_label(a);
    Label exp_positivo = asm_label(a), exp_digitos = asm_label(a), exp_escribir = asm_label(a);
    Label con_sufijo = asm_label(a), terminar = asm_label(a);
    uint64_t text = rt->num_buf + FLOAT_TEXT, digits = rt->num_buf + FLOAT_DIGITS;

    asm_bind(a, rt->fmt_float);
    x_mov_ri(a, RDI, text);

    /* ---- Signo: escribir '-' y quitar el bit de signo ---- */
    x_mov_ri(a, RCX, 0x8000000000000000ULL);
    x_test_rr(a, RAX, RCX);
    x_jcc(a, CC_E, positivo);
    put_char(a, '-');
    x_xor_rr(a, RAX, RCX);
    asm_bind(a, positivo);

    /* ---- inf y nan: exponente con todos los bits en 1 ---- */
    x_mov_ri(a, RCX, 0x7FF0000000000000ULL);
    x_mov_rr(a, RDX, RAX);
    x_and_rr(a, RDX, RCX);
    x_cmp_rr(a, RDX, RCX);
    x_jcc(a, CC_NE, finito);
    x_mov_ri(a, RCX, 0x000FFFFFFFFFFFFFULL);    /* mantisa distinta de 0: nan */
    x_test_rr(a, RAX, RCX);
    x_jcc(a, CC_NE, es_nan);
    put_char(a, 'i'); put_char(a, 'n'); put_char(a, 'f');
    x_jmp(a, terminar);                         /* tiene 'n': sin ".0" */
    asm_bind(a, es_nan);
    put_char(a, 'n'); put_char(a, 'a'); put_char(a, 'n');
    x_jmp(a, terminar);

    /* ---- Cero ---- */
    asm_bind(a, finito);
    x_test_rr(a, RAX, RAX);
    x_jcc(a, CC_NE, no_cero);
    put_char(a, '0');
    x_jmp(a, con_sufijo);                       /* "0" -> "0.0" */

    /* ---- 1. Exponente decimal ----
     *           mov r8, rax
     *           shr r8, 52
     *           sub r8, 1023            ; e2: exponente binario
     *           mov rcx, 78913          ; 78913 / 2^18 ~ log10(2)
     *           imul r8, rcx
     *           sar r8, 18              ; E estimado
     *           movq xmm0, rax          ; xmm0 = x
     * bajar:    cmp r8, -324            ; no salir de la tabla
     *           jle subir
     *           xmm1 = pow10[E]
     *           ucomisd xmm0, xmm1
     *           jae subir               ; x >= 10^E
     *           sub r8, 1
     *           jmp bajar
     * subir:    cmp r8, 308             ; ningún double finito llega a 10^309
     *           jge listo
     *           xmm1 = pow10[E + 1]
     *           ucomisd xmm0, xmm1
     *           jb listo                ; x < 10^(E+1)
     *           add r8, 1
     *           jmp subir
     */
    asm_bind(a, no_cero);
    x_mov_rr(a, R8, RAX);
    x_shr_ri(a, R8, 52);
    x_sub_ri(a, R8, 1023);
    x_mov_ri(a, RCX, 78913);
    x_imul_rr(a, R8, RCX);
    x_sar_ri(a, R8, 18);
    x_movq_xr(a, XMM0, RAX);
    asm_bind(a, bajar);
    x_mov_ri(a, RCX, (uint64_t)POW10_MIN);
    x_cmp_rr(a, R8, RCX);
    x_jcc(a, CC_LE, subir);
    load_pow10(a, rt, XMM1, R8);
    x_ucomisd(a, XMM0, XMM1);
    x_jcc(a, CC_AE, subir);
    x_sub_ri(a, R8, 1);
    x_jmp(a, bajar);
    asm_bind(a, subir);
    x_mov_ri(a, RCX, POW10_MAX);
    x_cmp_rr(a, R8, RCX);
    x_jcc(a, CC_GE, exponente_listo);
    x_mov_rr(a, R11, R8);
    x_add_ri(a, R11, 1);
    load_pow10(a, rt, XMM1, R11);
    x_ucomisd(a, XMM0, XMM1);
    x_jcc(a, CC_B, exponente_listo);
    x_add_ri(a, R8, 1);
    x_jmp(a, subir);
    asm_bind(a, exponente_listo);

    /* ---- 2. N = redondear(x * 10^k), con k = 14 - E ----
     * El problema es el doble redondeo: si x * 10^k se calcula como un solo
     * double, el valor exacto ...428.54 puede quedar en ...428.5 (un double
     * cerca de 10^15 avanza de a 0.125); cvtsd2si ve un empate y redondea
     * al par, ...428, mientras que printf, que redondea el valor exacto, da
     * ...429.
     *
     * Por eso el producto se lleva como "double-double": un par (hi, lo)
     * cuya suma representa el valor con unos 106 bits de precisión. Se
     * aplica 10^k en pasos de a lo sumo 10^22, que es la mayor potencia de
     * 10 exacta en un double:
     *   multiplicar por P:  hi' = hi * P
     *                       lo' = (error exacto de hi * P) + lo * P
     *   dividir entre P:    hi' = hi / P
     *                       p = hi' * P,  e = error exacto de hi' * P
     *                       lo' = ((hi - p) - e + lo) / P
     * El error exacto de un producto lo da el algoritmo TwoProduct, y
     * hi - p es exacto porque p está muy cerca de hi. Después de cada paso
     * se renormaliza el par (s = hi' + lo', lo = lo' - (s - hi'), hi = s)
     * para que lo sea menor que medio ulp de hi.
     *
     * Al final N = redondear(hi). Como |lo| < 0.5, la única duda es un
     * empate (hi - N = +-0.5): ahí el signo de lo dice de qué lado estaba
     * el valor exacto.
     *
     * Registros: xmm2 = hi, r10 = bits de lo, r11 = potencia que falta
     * aplicar, rsi = exponente del paso, xmm3 = 10^rsi. xmm0 (x) se usa
     * como temporal y se recupera de rax, que conserva los bits de x.
     *
     * calcular: r11 = 14 - E
     *           xmm2 = x
     *           r10 = 0                 ; lo = 0.0
     * paso:     si r11 == 0: ir a redondear
     *           si r11 < 0: ir a dividir
     *           rsi = min(r11, 22); r11 -= rsi; xmm3 = pow10[rsi]
     *           xmm0 = xmm2 * xmm3                    ; hi'
     *           xmm7 = TwoProduct(xmm2, xmm3, xmm0)   ; error de hi * P
     *           xmm7 += lo * xmm3                     ; lo'
     *           renormalizar (xmm0, xmm7) en (xmm2, r10)
     *           ir a paso
     * dividir:  rsi = min(-r11, 22); r11 += rsi; xmm3 = pow10[rsi]
     *           r9 = xmm2                             ; guardar hi
     *           xmm0 = xmm2 / xmm3                    ; hi'
     *           xmm2 = xmm0 * xmm3                    ; p   (calculados con hi'/2 y
     *           xmm7 = TwoProduct(xmm0, xmm3, xmm2)   ; e    duplicados, ver abajo)
     *           xmm1 = ((hi - p) - e + lo) / xmm3     ; lo'
     *           renormalizar (xmm0, xmm1) en (xmm2, r10)
     *           ir a paso
     * redondear:
     *           xmm0 = x; xmm7 = lo
     *           cvtsd2si r9, xmm2                     ; N
     *           xmm6 = hi - N
     *           si xmm6 == 0.5 y lo > 0: N += 1
     *           si xmm6 == -0.5 y lo < 0: N -= 1
     *           si N >= 10^15: E += 1, ir a calcular
     *           si N < 10^14: E -= 1, ir a calcular
     */
    asm_bind(a, calcular);
    x_mov_ri(a, R11, 14);
    x_sub_rr(a, R11, R8);
    x_movsd_rr(a, XMM2, XMM0);
    x_mov_ri(a, R10, 0);

    asm_bind(a, paso);
    x_test_rr(a, R11, R11);
    x_jcc(a, CC_E, redondear);
    x_jcc(a, CC_L, dividir);
    x_mov_rr(a, RSI, R11);
    x_mov_ri(a, RCX, 22);
    x_cmp_rr(a, RSI, RCX);
    x_jcc(a, CC_LE, mult_ok);
    x_mov_ri(a, RSI, 22);
    asm_bind(a, mult_ok);
    x_sub_rr(a, R11, RSI);
    load_pow10(a, rt, XMM3, RSI);
    x_movsd_rr(a, XMM0, XMM2);
    x_mulsd(a, XMM0, XMM3);                     /* hi' = hi * P */
    two_product_error(a, XMM2, XMM3, XMM0);     /* xmm7 = error de hi * P */
    x_movq_xr(a, XMM1, R10);
    x_mulsd(a, XMM1, XMM3);
    x_addsd(a, XMM7, XMM1);                     /* lo' = error + lo * P */
    x_movsd_rr(a, XMM2, XMM0);
    x_addsd(a, XMM2, XMM7);                     /* s = hi' + lo' */
    x_movsd_rr(a, XMM1, XMM2);
    x_subsd(a, XMM1, XMM0);
    x_subsd(a, XMM7, XMM1);                     /* lo = lo' - (s - hi') */
    x_movq_rx(a, R10, XMM7);
    x_jmp(a, paso);

    asm_bind(a, dividir);
    x_mov_rr(a, RSI, R11);
    x_neg(a, RSI);
    x_mov_ri(a, RCX, 22);
    x_cmp_rr(a, RSI, RCX);
    x_jcc(a, CC_LE, div_ok);
    x_mov_ri(a, RSI, 22);
    asm_bind(a, div_ok);
    x_add_rr(a, R11, RSI);
    load_pow10(a, rt, XMM3, RSI);
    x_movq_rx(a, R9, XMM2);                     /* guardar hi */
    x_movsd_rr(a, XMM0, XMM2);
    x_divsd(a, XMM0, XMM3);                     /* hi' = hi / P */
    /* p = hi' * P y su error se calculan con hi' / 2 y se duplican (ambas
     * operaciones son exactas): cerca del mayor double, los productos
     * parciales de TwoProduct con hi' completo pueden desbordar a inf. */
    x_movq_rx(a, RDX, XMM0);                    /* guardar hi' */
    load_double(a, XMM1, 0.5);
    x_mulsd(a, XMM0, XMM1);                     /* hi' / 2 */
    x_movsd_rr(a, XMM2, XMM0);
    x_mulsd(a, XMM2, XMM3);                     /* p / 2 */
    two_product_error(a, XMM0, XMM3, XMM2);     /* xmm7 = e / 2 */
    x_addsd(a, XMM2, XMM2);                     /* p */
    x_addsd(a, XMM7, XMM7);                     /* e */
    x_movq_xr(a, XMM0, RDX);                    /* hi' */
    x_movq_xr(a, XMM1, R9);
    x_subsd(a, XMM1, XMM2);                     /* hi - p (exacto) */
    x_subsd(a, XMM1, XMM7);                     /* - e: resto exacto */
    x_movq_xr(a, XMM7, R10);
    x_addsd(a, XMM1, XMM7);                     /* + lo */
    x_divsd(a, XMM1, XMM3);                     /* lo' */
    x_movsd_rr(a, XMM2, XMM0);
    x_addsd(a, XMM2, XMM1);                     /* s = hi' + lo' */
    x_movsd_rr(a, XMM7, XMM2);
    x_subsd(a, XMM7, XMM0);
    x_subsd(a, XMM1, XMM7);                     /* lo = lo' - (s - hi') */
    x_movq_rx(a, R10, XMM1);
    x_jmp(a, paso);

    asm_bind(a, redondear);
    x_movq_xr(a, XMM0, RAX);                    /* recuperar x */
    x_movq_xr(a, XMM7, R10);                    /* lo */
    x_cvtsd2si(a, R9, XMM2);
    x_cvtsi2sd(a, XMM5, R9);
    x_movsd_rr(a, XMM6, XMM2);
    x_subsd(a, XMM6, XMM5);                     /* hi - N */
    load_double(a, XMM5, 0.0);
    load_double(a, XMM4, 0.5);
    x_ucomisd(a, XMM6, XMM4);
    x_jcc(a, CC_NE, no_mitad_arriba);
    x_ucomisd(a, XMM7, XMM5);
    x_jcc(a, CC_BE, ajustado);                  /* lo <= 0: queda N */
    x_add_ri(a, R9, 1);
    x_jmp(a, ajustado);
    asm_bind(a, no_mitad_arriba);
    load_double(a, XMM4, -0.5);
    x_ucomisd(a, XMM6, XMM4);
    x_jcc(a, CC_NE, ajustado);
    x_ucomisd(a, XMM7, XMM5);
    x_jcc(a, CC_AE, ajustado);                  /* lo >= 0: queda N */
    x_sub_ri(a, R9, 1);
    asm_bind(a, ajustado);

    x_mov_ri(a, RCX, 1000000000000000ULL);
    x_cmp_rr(a, R9, RCX);
    x_jcc(a, CC_L, no_grande);
    x_add_ri(a, R8, 1);
    x_jmp(a, calcular);
    asm_bind(a, no_grande);
    x_mov_ri(a, RCX, 100000000000000ULL);
    x_cmp_rr(a, R9, RCX);
    x_jcc(a, CC_GE, n_listo);
    x_sub_ri(a, R8, 1);
    x_jmp(a, calcular);
    asm_bind(a, n_listo);

    /* ---- 3. Los 15 dígitos de N, y k = cuántos quedan sin los ceros finales ----
     *           mov rax, r9
     *           mov r10, digits + 15
     *           mov rcx, 10
     * digito:   mov rdx, 0
     *           div rcx
     *           add rdx, '0'
     *           sub r10, 1
     *           mov [r10], dl
     *           mov r11, digits
     *           cmp r10, r11
     *           jne digito
     *           mov r10, 15
     * contar:   cmp r10, 1
     *           je contado
     *           mov rcx, digits
     *           add rcx, r10
     *           movzx rdx, byte [rcx-1] ; último dígito que queda
     *           cmp rdx, '0'
     *           jne contado
     *           sub r10, 1
     *           jmp contar
     */
    x_mov_rr(a, RAX, R9);
    x_mov_ri(a, R10, digits + 15);
    x_mov_ri(a, RCX, 10);
    asm_bind(a, digito);
    x_mov_ri(a, RDX, 0);
    x_div(a, RCX);
    x_add_ri(a, RDX, '0');
    x_sub_ri(a, R10, 1);
    x_store8(a, R10, 0, RDX);
    x_mov_ri(a, R11, digits);
    x_cmp_rr(a, R10, R11);
    x_jcc(a, CC_NE, digito);
    x_mov_ri(a, R10, 15);
    asm_bind(a, contar);
    x_mov_ri(a, RCX, 1);
    x_cmp_rr(a, R10, RCX);
    x_jcc(a, CC_E, contado);
    x_mov_ri(a, RCX, digits);
    x_add_rr(a, RCX, R10);
    x_load8(a, RDX, RCX, -1);
    x_mov_ri(a, RCX, '0');
    x_cmp_rr(a, RDX, RCX);
    x_jcc(a, CC_NE, contado);
    x_sub_ri(a, R10, 1);
    x_jmp(a, contar);
    asm_bind(a, contado);

    /* ---- 4. Formato ----
     * Notación normal si -4 <= E < 15:
     *   E >= 0: los primeros E + 1 dígitos; si quedan, '.' y el resto.
     *           Sin parte fraccionaria se agrega ".0".
     *   E < 0:  "0." seguido de -E - 1 ceros y los k dígitos.
     * Científica en otro caso: d[.ddd]e(+|-)XX */
    x_mov_ri(a, RCX, (uint64_t)-4);
    x_cmp_rr(a, R8, RCX);
    x_jcc(a, CC_L, cientifico);
    x_mov_ri(a, RCX, 15);
    x_cmp_rr(a, R8, RCX);
    x_jcc(a, CC_GE, cientifico);
    x_test_rr(a, R8, R8);
    x_jcc(a, CC_L, fijo_negativo);

    /*           mov rsi, digits
     *           mov rcx, r8
     *           add rcx, 1
     *           rep movsb               ; parte entera
     *           mov rcx, r10
     *           sub rcx, r8
     *           sub rcx, 1              ; dígitos que quedan: k - E - 1
     *           test rcx, rcx
     *           jle con_sufijo
     *           (escribir '.')
     *           rep movsb               ; parte fraccionaria
     *           jmp terminar
     */
    x_mov_ri(a, RSI, digits);
    x_mov_rr(a, RCX, R8);
    x_add_ri(a, RCX, 1);
    copy_digits(a);
    x_mov_rr(a, RCX, R10);
    x_sub_rr(a, RCX, R8);
    x_sub_ri(a, RCX, 1);
    x_test_rr(a, RCX, RCX);
    x_jcc(a, CC_LE, con_sufijo);
    put_char(a, '.');
    copy_digits(a);
    x_jmp(a, terminar);

    /* fijo_negativo:
     *           (escribir "0.")
     *           mov rcx, -1
     *           sub rcx, r8             ; -E - 1 ceros
     * ceros:    test rcx, rcx
     *           je ceros_listos
     *           (escribir '0')
     *           sub rcx, 1
     *           jmp ceros
     * ceros_listos:
     *           mov rsi, digits
     *           mov rcx, r10
     *           rep movsb
     *           jmp terminar
     */
    asm_bind(a, fijo_negativo);
    put_char(a, '0');
    put_char(a, '.');
    x_mov_ri(a, RCX, (uint64_t)-1);
    x_sub_rr(a, RCX, R8);
    asm_bind(a, ceros);
    x_test_rr(a, RCX, RCX);
    x_jcc(a, CC_E, ceros_listos);
    put_char(a, '0');
    x_sub_ri(a, RCX, 1);
    x_jmp(a, ceros);
    asm_bind(a, ceros_listos);
    x_mov_ri(a, RSI, digits);
    x_mov_rr(a, RCX, R10);
    copy_digits(a);
    x_jmp(a, terminar);

    /* cientifico:
     *           mov rsi, digits
     *           mov rcx, 1
     *           rep movsb               ; primer dígito
     *           cmp r10, 1
     *           je un_digito
     *           (escribir '.')
     *           mov rcx, r10
     *           sub rcx, 1
     *           rep movsb               ; los demás dígitos
     * un_digito:
     *           (escribir 'e')
     *           test r8, r8
     *           jge exp_positivo
     *           (escribir '-')
     *           neg r8
     *           jmp exp_digitos
     * exp_positivo:
     *           (escribir '+')
     * exp_digitos:
     *           cmp r8, 10
     *           jge exp_escribir
     *           (escribir '0')          ; al menos dos dígitos: e+05
     * exp_escribir:
     *           push rdi
     *           mov rax, r8
     *           call fmt_int            ; usa num_buf[0..32), no choca con el texto
     *           pop rdi
     *           mov rcx, rdx
     *           rep movsb
     *           jmp terminar
     */
    asm_bind(a, cientifico);
    x_mov_ri(a, RSI, digits);
    x_mov_ri(a, RCX, 1);
    copy_digits(a);
    x_mov_ri(a, RCX, 1);
    x_cmp_rr(a, R10, RCX);
    x_jcc(a, CC_E, un_digito);
    put_char(a, '.');
    x_mov_rr(a, RCX, R10);
    x_sub_ri(a, RCX, 1);
    copy_digits(a);
    asm_bind(a, un_digito);
    put_char(a, 'e');
    x_test_rr(a, R8, R8);
    x_jcc(a, CC_GE, exp_positivo);
    put_char(a, '-');
    x_neg(a, R8);
    x_jmp(a, exp_digitos);
    asm_bind(a, exp_positivo);
    put_char(a, '+');
    asm_bind(a, exp_digitos);
    x_mov_ri(a, RCX, 10);
    x_cmp_rr(a, R8, RCX);
    x_jcc(a, CC_GE, exp_escribir);
    put_char(a, '0');
    asm_bind(a, exp_escribir);
    x_push(a, RDI);
    x_mov_rr(a, RAX, R8);
    x_call(a, rt->fmt_int);
    x_pop(a, RDI);
    x_mov_rr(a, RCX, RDX);
    copy_digits(a);
    x_jmp(a, terminar);

    /* con_sufijo: (escribir ".0")
     * terminar:   mov rsi, text
     *             mov rdx, rdi
     *             sub rdx, rsi
     *             ret
     */
    asm_bind(a, con_sufijo);
    put_char(a, '.');
    put_char(a, '0');
    asm_bind(a, terminar);
    x_mov_ri(a, RSI, text);
    x_mov_rr(a, RDX, RDI);
    x_sub_rr(a, RDX, RSI);
    x_ret(a);
}

/* print_float: imprime el double con bits rax.
 *           call fmt_float
 *           jmp write
 * str_float: rax = cadena con el double con bits rax.
 *           call fmt_float
 *           jmp copy_str
 */
static void emit_print_str_float(Asm *a, Runtime *rt) {
    asm_bind(a, rt->print_float);
    x_call(a, rt->fmt_float);
    x_jmp(a, rt->write);
    asm_bind(a, rt->str_float);
    x_call(a, rt->fmt_float);
    x_jmp(a, rt->copy_str);
}

void rt_emit(Asm *a, Runtime *rt) {
    emit_write_all(a, rt);
    emit_flush(a, rt);
    emit_write(a, rt);
    emit_fmt_int(a, rt);
    emit_print_int(a, rt);
    emit_print_bool(a, rt);
    emit_print_str(a, rt);
    emit_exit(a, rt);
    emit_error(a, rt);
    emit_alloc(a, rt);
    emit_strlen(a, rt);
    emit_copy_str(a, rt);
    emit_str_int_bool(a, rt);
    emit_concat(a, rt);
    emit_strcmp(a, rt);
    emit_fmt_float(a, rt);
    emit_print_str_float(a, rt);
}
