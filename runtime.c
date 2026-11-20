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
    rt->out_len = asm_bss(a, 8);
    rt->depth = asm_bss(a, 8);
    rt->heap_ptr = asm_bss(a, 8);
    rt->heap_end = asm_bss(a, 8);
    rt->num_buf = asm_bss(a, 128);
    rt->out_buf = asm_bss(a, OUT_BUF_SIZE);
    rt->text_true = rt_literal(a, "true", 4);
    rt->text_false = rt_literal(a, "false", 5);
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
}
