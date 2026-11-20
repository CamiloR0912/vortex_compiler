/* ==========================================================================
 * runtime.c - Código máquina de las rutinas de ejecución
 *
 * Cada rutina se presenta primero en ensamblador (sintaxis Intel) y debajo
 * las llamadas al codificador que la producen, una por instrucción.
 * ========================================================================== */
#include <string.h>
#include "runtime.h"

void rt_init(Asm *a, Runtime *rt) {
    rt->write = asm_label(a);
    rt->write_all = asm_label(a);
    rt->flush = asm_label(a);
    rt->print_int = asm_label(a);
    rt->print_bool = asm_label(a);
    rt->exit = asm_label(a);
    rt->error = asm_label(a);
    rt->out_len = asm_bss(a, 8);
    rt->depth = asm_bss(a, 8);
    rt->out_buf = asm_bss(a, OUT_BUF_SIZE);
    rt->text_true = asm_data(a, "true", 4);
    rt->text_false = asm_data(a, "false", 5);
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

/* print_int: imprime rax como entero decimal.
 * Los dígitos se obtienen dividiendo entre 10: cada resto es un dígito,
 * del último al primero, así que se escriben de derecha a izquierda en un
 * espacio de 32 bytes reservado en la pila (bastan 20 dígitos y el signo).
 * Un negativo se niega y se divide SIN signo: así también funciona con
 * -9223372036854775808, cuyo negado solo existe como número sin signo.
 *
 *           sub rsp, 32
 *           mov r9, 0               ; r9 = 1 si el número es negativo
 *           test rax, rax
 *           jge positivo
 *           mov r9, 1
 *           neg rax
 * positivo: mov rdi, rsp
 *           add rdi, 32             ; rdi = fin del espacio
 *           mov rcx, 10
 *  digito:  mov rdx, 0
 *           div rcx                 ; rax = rax / 10, rdx = rax % 10
 *           add rdx, '0'            ; dígito -> carácter
 *           sub rdi, 1
 *           mov [rdi], dl
 *           test rax, rax
 *           jne digito
 *           test r9, r9
 *           je escribir
 *           sub rdi, 1
 *           mov rdx, '-'
 *           mov [rdi], dl
 * escribir: mov rsi, rdi            ; rsi = primer carácter
 *           mov rdx, rsp
 *           add rdx, 32
 *           sub rdx, rdi            ; rdx = cantidad de caracteres
 *           call write
 *           add rsp, 32
 *           ret
 */
static void emit_print_int(Asm *a, Runtime *rt) {
    Label positivo = asm_label(a), digito = asm_label(a), escribir = asm_label(a);
    asm_bind(a, rt->print_int);
    x_sub_ri(a, RSP, 32);
    x_mov_ri(a, R9, 0);
    x_test_rr(a, RAX, RAX);
    x_jcc(a, CC_GE, positivo);
    x_mov_ri(a, R9, 1);
    x_neg(a, RAX);
    asm_bind(a, positivo);
    x_mov_rr(a, RDI, RSP);
    x_add_ri(a, RDI, 32);
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
    x_jcc(a, CC_E, escribir);
    x_sub_ri(a, RDI, 1);
    x_mov_ri(a, RDX, '-');
    x_store8(a, RDI, 0, RDX);
    asm_bind(a, escribir);
    x_mov_rr(a, RSI, RDI);
    x_mov_rr(a, RDX, RSP);
    x_add_ri(a, RDX, 32);
    x_sub_rr(a, RDX, RDI);
    x_call(a, rt->write);
    x_add_ri(a, RSP, 32);
    x_ret(a);
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

void rt_emit(Asm *a, Runtime *rt) {
    emit_write_all(a, rt);
    emit_flush(a, rt);
    emit_write(a, rt);
    emit_print_int(a, rt);
    emit_print_bool(a, rt);
    emit_exit(a, rt);
    emit_error(a, rt);
}
