/* ==========================================================================
 * elf.c - Escritura del archivo ejecutable (el "enlazador" de vortexc)
 *
 * Linux ejecuta archivos en formato ELF. Un ejecutable estático mínimo tiene
 * tres partes:
 *
 *   1. Cabecera ELF (64 bytes): identifica el archivo como ELF de 64 bits
 *      para x86-64 e indica el punto de entrada.
 *   2. Tabla de cabeceras de programa (56 bytes por entrada): le dice al
 *      cargador del sistema qué partes del archivo copiar a memoria, en qué
 *      dirección y con qué permisos. Cada entrada describe un segmento.
 *   3. El contenido de los segmentos.
 *
 * Distribución del archivo que escribe vortexc:
 *
 *   desplazamiento 0      cabecera ELF
 *                  64     3 cabeceras de programa
 *                  232    código                     <- punto de entrada
 *                  ...    relleno hasta múltiplo de 4096
 *                  N      datos
 *
 *   Segmento 1 (lectura y ejecución): bytes 0 .. fin del código,
 *              cargado en CODE_BASE. Incluye las cabeceras, lo que es
 *              habitual y no estorba.
 *   Segmento 2 (lectura y escritura): los datos, cargados en DATA_BASE.
 *   Segmento 3 (lectura y escritura): memoria sin inicializar en BSS_BASE.
 *              Ocupa 0 bytes en el archivo (p_filesz = 0) y bss_len bytes
 *              en memoria; el núcleo la entrega llena de ceros.
 *
 * El cargador exige que la dirección de un segmento y su desplazamiento en
 * el archivo coincidan módulo el tamaño de página (4096). Por eso los datos
 * empiezan en un desplazamiento múltiplo de 4096, igual que DATA_BASE.
 *
 * No hay tabla de secciones ni de símbolos: el núcleo no las necesita para
 * ejecutar el programa. Referencia: TIS, "ELF Specification", v1.2.
 * ========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "x86.h"

#define EHDR_SIZE  64
#define PHDR_SIZE  56
#define PHDR_COUNT 3
#define PAGE       4096

/* Dirección en memoria del primer byte de código */
#define CODE_START (CODE_BASE + EHDR_SIZE + PHDR_COUNT * PHDR_SIZE)

static void put16(Buf *b, uint16_t v) { buf_byte(b, v & 0xFF); buf_byte(b, v >> 8); }

/* Una entrada de la tabla de cabeceras de programa (Elf64_Phdr) */
static void program_header(Buf *b, uint32_t flags, uint64_t offset, uint64_t vaddr,
                           uint64_t filesz, uint64_t memsz) {
    buf_u32(b, 1);          /* p_type = PT_LOAD: segmento que se carga en memoria */
    buf_u32(b, flags);      /* p_flags: 4 = lectura, 2 = escritura, 1 = ejecución */
    buf_u64(b, offset);     /* p_offset: dónde empieza en el archivo */
    buf_u64(b, vaddr);      /* p_vaddr: dónde se carga en memoria */
    buf_u64(b, vaddr);      /* p_paddr: no se usa en Linux */
    buf_u64(b, filesz);     /* p_filesz: bytes que se copian del archivo */
    buf_u64(b, memsz);      /* p_memsz: bytes en memoria; el resto se llena con ceros */
    buf_u64(b, PAGE);       /* p_align */
}

/* Escribe el ejecutable. entry es la posición, dentro del código, de la
 * primera instrucción que debe ejecutarse. Devuelve 0 si todo salió bien. */
int elf_write(const char *path, const Asm *a, int entry) {
    Buf out = {0};
    uint64_t code_end = EHDR_SIZE + PHDR_COUNT * PHDR_SIZE + a->code.len;
    uint64_t data_off = (code_end + PAGE - 1) / PAGE * PAGE;
    uint64_t data_mem = a->data.len > 0 ? a->data.len : 1;   /* nunca un segmento vacío */
    uint64_t bss_mem = a->bss_len > 0 ? a->bss_len : 1;
    static const unsigned char ident[16] = {
        0x7F, 'E', 'L', 'F',    /* número mágico */
        2,                      /* EI_CLASS = ELFCLASS64 */
        1,                      /* EI_DATA = little endian */
        1,                      /* EI_VERSION */
        0,                      /* EI_OSABI = System V */
    };
    FILE *f;

    /* ---- 1. Cabecera ELF (Elf64_Ehdr) ---- */
    buf_bytes(&out, ident, 16);
    put16(&out, 2);                     /* e_type = ET_EXEC: ejecutable */
    put16(&out, 62);                    /* e_machine = EM_X86_64 */
    buf_u32(&out, 1);                   /* e_version */
    buf_u64(&out, CODE_START + entry);  /* e_entry: punto de entrada */
    buf_u64(&out, EHDR_SIZE);           /* e_phoff: tabla de cabeceras de programa */
    buf_u64(&out, 0);                   /* e_shoff: sin tabla de secciones */
    buf_u32(&out, 0);                   /* e_flags */
    put16(&out, EHDR_SIZE);             /* e_ehsize */
    put16(&out, PHDR_SIZE);             /* e_phentsize */
    put16(&out, PHDR_COUNT);            /* e_phnum */
    put16(&out, 64);                    /* e_shentsize */
    put16(&out, 0);                     /* e_shnum */
    put16(&out, 0);                     /* e_shstrndx */

    /* ---- 2. Cabeceras de programa ---- */
    program_header(&out, 4 | 1, 0, CODE_BASE, code_end, code_end);
    program_header(&out, 4 | 2, data_off, DATA_BASE, a->data.len, data_mem);
    program_header(&out, 4 | 2, 0, BSS_BASE, 0, bss_mem);

    /* ---- 3. Segmentos ---- */
    buf_bytes(&out, a->code.bytes, a->code.len);
    while ((uint64_t)out.len < data_off) buf_byte(&out, 0);
    buf_bytes(&out, a->data.bytes, a->data.len);

    f = fopen(path, "wb");
    if (f == NULL) { free(out.bytes); return -1; }
    fwrite(out.bytes, 1, out.len, f);
    fclose(f);
    free(out.bytes);

    /* rwxr-xr-x: el archivo queda marcado como ejecutable */
    return chmod(path, 0755);
}
