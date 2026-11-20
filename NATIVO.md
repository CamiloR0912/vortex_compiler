# Backend nativo de vortexc

Este documento sigue la construcción del backend que reemplaza a GCC. El objetivo es que `vortexc` escriba por sí mismo un ejecutable ELF para Linux x86-64, sin compilador, ensamblador ni enlazador externos, y sin la biblioteca estándar de C en el programa generado.

El compilador se reconstruye por etapas: cada rama contiene solo lo que existe hasta esa etapa, de modo que cada avance se puede compilar y probar por separado. La versión anterior, que traducía a C y usaba GCC, está en la rama `main`; de ella se recuperan el analizador léxico, el sintáctico y el semántico cuando una etapa los necesita, y sus 33 programas de prueba son la referencia de la salida correcta.

## Etapas

| Etapa | Rama | Alcance | Estado |
|---|---|---|---|
| 1 | `etapa-1/ejecutable-minimo` | Codificador de instrucciones y escritor ELF | Terminada |
| 2 | `etapa-2/enteros-y-print` | Enteros, booleanos, variables, aritmética y `print` | Terminada |
| 3 | `etapa-3/control-de-flujo` | `if`, `while`, `for`, `switch`, `break` y `continue` | Terminada |
| 4 | `etapa-4/funciones` | Funciones, recursión, sobrecarga y funciones anidadas | Terminada |
| 5 | `etapa-5/cadenas-y-reales` | `string` y `float` | Pendiente |
| 6 | `etapa-6/excepciones` | `try`-`catch`, `throw`, `assert` y errores de ejecución | Pendiente |
| 7 | `etapa-7/vectores-y-matrices` | Arreglos, operaciones elemento a elemento y reducciones | Pendiente |

## Etapa 1: ejecutable mínimo

### Archivos

| Archivo | Hace el trabajo de | Contenido |
|---|---|---|
| `x86.h` | | Registros, condiciones, la estructura `Asm` y la interfaz del codificador |
| `x86enc.c` | Ensamblador | Convierte cada instrucción en bytes y resuelve las etiquetas |
| `elf.c` | Enlazador | Escribe la cabecera ELF, las cabeceras de programa y los segmentos |
| `tests/nativo/` | | Generadores de prueba y `run.sh` |

### Cómo se codifica una instrucción

Una instrucción x86-64 tiene la forma `[REX] opcode [ModR/M] [SIB] [desplazamiento] [inmediato]`. Ejemplos tomados de los ejecutables de prueba (desensamblados con `objdump`):

| Instrucción | Bytes | Explicación |
|---|---|---|
| `mov eax, 1` | `b8 01 00 00 00` | `B8 + 0` (rax es el registro 0) y el valor en 32 bits little endian |
| `mov esi, 0x10000000` | `be 00 00 00 10` | `B8 + 6` (rsi) y la dirección del mensaje en el segmento de datos |
| `syscall` | `0f 05` | |
| `dec rbx` | `48 ff cb` | REX.W (64 bits), opcode `FF`, ModR/M `11 001 011`: registro, operación 1 (dec), rbx |
| `jne ciclo` | `0f 85 e8 ff ff ff` | Salto condicional; `0xffffffe8` = -24 bytes, hacia atrás |
| `call imprimir` | `e8 29 00 00 00` | Llamada 41 bytes hacia adelante |

Escribir `mov eax, 1` en lugar de `mov rax, 1` es correcto: al escribir un registro de 32 bits, el procesador pone en cero los 32 bits altos, y la instrucción ocupa 5 bytes en lugar de 10.

### Etiquetas y relleno posterior

Un salto hacia una etiqueta que todavía no tiene posición (por ejemplo, la llamada a una rutina definida más abajo) se emite con un desplazamiento provisional de cero, y su posición queda anotada en la lista `fixups`. Al terminar, `asm_finish` recorre la lista y escribe en cada salto el desplazamiento real:

```
rel32 = posición de la etiqueta - (posición del campo rel32 + 4)
```

El desplazamiento se mide desde el final de la instrucción, que es donde termina el campo de 4 bytes.

### Estructura del ejecutable

```
desplazamiento 0      cabecera ELF (64 bytes)
               64     3 cabeceras de programa (56 bytes cada una)
               232    código               cargado en 0x4000E8: punto de entrada
               ...    relleno hasta 4096
               4096   datos                cargados en 0x10000000
```

El segmento de código tiene permisos de lectura y ejecución y el de datos de lectura y escritura. El tercer segmento, agregado en la etapa 2, es memoria sin inicializar (bss) en `0x20000000`: no ocupa bytes en el archivo y el núcleo la entrega llena de ceros. Ahí van el buffer de salida y otras variables del entorno de ejecución.

Como cada segmento empieza en una dirección fija, la dirección de un dato se conoce en cuanto se crea y se escribe directamente en la instrucción que lo usa, sin un paso de reubicación.

### Pruebas

Desde WSL, en la carpeta del proyecto:

```
bash run                      # construye vortexc (no ejecuta pruebas)
bash tests/nativo/run.sh      # pruebas del backend nativo
bash tests/run_tests.sh       # programas Vortex (necesita vortexc construido)
```

| Prueba | Qué demuestra | Salida esperada |
|---|---|---|
| `hola.c` | Un ejecutable escrito sin GCC que el núcleo carga y ejecuta | Un mensaje y código de salida 0 |
| `saltos.c` | Saltos hacia atrás (ciclo), hacia adelante (`jmp` que omite código) y `call`/`ret` | Tres líneas y código de salida 7 |

`file` reporta los ejecutables generados como `ELF 64-bit LSB executable, x86-64, version 1 (SYSV), statically linked, no section header`.

Los generadores de prueba se compilan con GCC porque son programas en C que usan el codificador, igual que `vortexc`. Los ejecutables que producen no pasan por GCC.

## Etapa 2: enteros, booleanos y print

En esta etapa `vortexc` compila por primera vez un programa Vortex a un ejecutable sin ningún programa externo.

```
./vortexc programa.vx            # genera el ejecutable programa
./vortexc programa.vx --run      # lo genera en una carpeta temporal y lo ejecuta
```

### Archivos

| Archivo | Contenido |
|---|---|
| `lexer.l`, `parser.y`, `ast.c`, `semantic.c`, `vortex.h` | Análisis léxico, sintáctico y semántico, recuperados de `main` sin cambios (salvo quitar las referencias al generador de C) |
| `x86gen.c` | Generador de código: recorre el AST anotado y emite las instrucciones |
| `runtime.c`, `runtime.h` | Rutinas incluidas en cada ejecutable: salida con buffer, `print_int`, `print_bool`, `exit` y `error` |
| `main.c` | Coordina las etapas; sin llamadas a GCC |

### Modelo de máquina de pila

El resultado de toda expresión queda en `rax`. Para un operador binario se calcula el operando izquierdo y se apila; se calcula el derecho, se pasa a `rcx` y se desapila el izquierdo. El generador no asigna registros: cada valor intermedio vive en la pila. Para `print(a + b * 3)`, con `a` en `[rbp-8]` y `b` en `[rbp-16]`, el código generado es (desensamblado con `objdump`):

```
push rbp                        ; prólogo: marco de 16 bytes
mov  rbp, rsp
sub  rsp, 0x10
mov  eax, 7                     ; int a = 7
mov  [rbp-0x8], rax
mov  eax, 2                     ; int b = 2
mov  [rbp-0x10], rax
mov  rax, [rbp-0x8]             ; a
push rax
mov  rax, [rbp-0x10]            ;     b
push rax
mov  eax, 3                     ;         3
mov  rcx, rax
pop  rax
imul rax, rcx                   ;     b * 3
mov  rcx, rax
pop  rax
add  rax, rcx                   ; a + (b * 3)
push rax                        ; print: primero se calculan y apilan los valores
mov  rax, [rsp+0x0]
call print_int                  ; luego se imprimen
mov  esi, "\n"                  ; salto de línea final
mov  edx, 1
call write
add  rsp, 0x8
mov  edi, 0                     ; fin del programa: exit(0)
call exit
```

La precedencia no aparece en el código: ya está en la forma del árbol (`+(a, *(b, 3))`), que el generador recorre en postorden.

### Decisiones

| Tema | Decisión |
|---|---|
| Variables | Cada símbolo ocupa 8 bytes en `[rbp - 8k]`. Como `semantic.c` da un número único a cada símbolo, las variables con el mismo nombre en ámbitos distintos (shadowing) quedan en posiciones distintas sin tratamiento especial |
| Tamaño del marco | Se conoce al terminar de generar; `sub rsp, N` se emite con 0 y se completa con `asm_patch_u32` |
| `bool` | Entero 0 o 1. Las comparaciones usan `cmp`, `set<cc>` y `movzx` |
| `&&`, `\|\|` | Cortocircuito: si el operando izquierdo decide el resultado, el derecho no se evalúa |
| `/`, `%` | Divisor cero: `[Runtime error] line N: division by zero` y código de salida 1. Divisor -1: se calcula aparte (`a / -1 = -a`, `a % -1 = 0`), porque `idiv` falla con el entero más negativo |
| `print` | Calcula y apila todos los valores antes de imprimir el primero, como la versión anterior: si un valor falla, la línea no queda impresa a medias |
| Construcciones de etapas futuras | Se reportan como `[Native backend] line N: ... is not supported yet` y no se genera el ejecutable |

### Pruebas

| Prueba | Qué demuestra |
|---|---|
| `tests/nativo/aritmetica.c` | Memoria, pila, aritmética, comparaciones, `div`, escritura de un byte y `rep movsb` |
| `tests/nativo/imprimir.c` | Enteros extremos, booleanos, salida más grande que el buffer y `exit` con texto pendiente |
| `tests/nativo/error.c` | Formato del error de ejecución y código de salida 1 |
| `tests/29_native_integers.vx` | Programa Vortex completo de la etapa: precedencia, división con negativos, cortocircuito, 64 bits, shadowing, `print()` vacío y división entre cero |
| `tests/*` de errores | Las 18 pruebas de errores léxicos, sintácticos y semánticos de `main` |

La salida esperada de `29_native_integers.vx` es la que produce el compilador de `main` (traducción a C y GCC) con el mismo programa. El backend nativo la reproduce byte por byte.

## Etapa 3: control de flujo

Todas las estructuras de control se traducen a comparaciones y saltos hacia etiquetas. Las etiquetas se crean antes de conocer su posición y el relleno posterior de la etapa 1 completa los saltos al final.

| Estructura | Esquema |
|---|---|
| `if` / `else if` / `else` | Cada condición se evalúa en `rax`; si es falsa (`test rax, rax` y `je`) salta a la siguiente rama. Al terminar una rama salta al final de la cadena |
| `while` | Etiqueta de inicio, condición con salto de salida, cuerpo y salto de vuelta al inicio |
| `for` | Inicialización antes del ciclo; después del cuerpo, el paso y el salto de vuelta. Sin condición es un ciclo infinito |
| `break`, `continue` | El generador lleva una pila de ciclos abiertos con dos etiquetas cada uno: salida y continuación. En `while`, `continue` vuelve a la condición; en `for` va al paso |
| `switch` | Cadena de comparaciones sin fall-through, como en la versión anterior. El valor se calcula una vez y se guarda en una posición propia del marco; cada `case` compara su expresión con él. El `switch` no es un ciclo: un `break` dentro de él sale del ciclo que lo contiene |

Código generado para un `for` con `continue` (desensamblado con `objdump`):

```
for (int i = 0; i < 3; i = i + 1) {
    if (i == 1) { continue; }
    print(i);
}
```

```
      mov  eax, 0                   ; int i = 0
      mov  [rbp-0x8], rax
ciclo:
      mov  rax, [rbp-0x8]           ; i < 3
      push rax
      mov  eax, 3
      mov  rcx, rax
      pop  rax
      cmp  rax, rcx
      setl al
      movzx rax, al
      test rax, rax
      je   fin                      ; condición falsa: salir del ciclo
      ...                           ; i == 1, igual que arriba con sete
      test rax, rax
      je   sigue                    ; if falso: saltar su bloque
      jmp  paso                     ; continue: ir al paso, no a la condición
sigue:
      ...                           ; print(i)
paso:
      mov  rax, [rbp-0x8]           ; i = i + 1
      ...
      jmp  ciclo
fin:
```

El programa imprime `0` y `2`: con `i == 1`, `continue` salta al paso y la variable sigue avanzando.

### Pruebas

| Prueba | Qué demuestra |
|---|---|
| `tests/02_scopes.vx` | Recuperada de `main`: bloques anidados y shadowing |
| `tests/30_native_control_flow.vx` | Las tres ramas del `if`, `if` sin `else` y anidados, `while` y `for` anidados, `for` sin paso y sin condición, primos con `while` dentro de `for`, `break` y `continue` en ambos ciclos, y `switch` con expresiones, `default`, `bool`, anidado y con `break` |

`tests/06_control_flow.vx` de `main` imprime cadenas; se recupera en la etapa 5.

## Etapa 4: funciones

### Marco de una función

```
              ...
[rbp + 24 + 8*(n-1-k)]   parámetro k de n      (los apila quien llama, en orden)
[rbp + 16]               enlace estático       (marco de la función que contiene la definición)
[rbp + 8]                dirección de retorno  (la apila call)
[rbp]                    rbp de quien llamó    (lo apila el prólogo)
[rbp - 8], [rbp - 16]... variables locales
```

Una llamada calcula y apila los argumentos en el orden de los parámetros, apila el enlace estático, hace `call` y al volver quita los argumentos de la pila. El resultado queda en `rax`. `semantic.c` ya resolvió la sobrecarga (`n->sym` es la versión elegida, y cada versión tiene su propia etiqueta) y dejó los argumentos en el orden de los parámetros, con los valores por defecto en lugar de los que faltan. Por eso los parámetros nombrados y por defecto no necesitan nada en el generador. Un valor por defecto se calcula en cada llamada, como en la versión anterior.

### Enlace estático

Una función anidada puede leer y modificar las variables de las funciones que la contienen. Para eso cada marco guarda en `[rbp+16]` la dirección del marco de la función donde está escrita su definición (no de la que la llamó). El programa principal es el nivel 0, una función definida en él es de nivel 1, una definida dentro de esa es de nivel 2, y así. Para llegar a una variable de un nivel externo se sigue la cadena de enlaces tantos pasos como niveles separen a las dos funciones.

Ejemplo (desensamblado con `objdump`):

```
func int outer(int p) {
    func int inner(int r) { return p + r; }
    return inner(2);
}
print(outer(40));                   // 42
```

```
outer:  ...
        mov  eax, 2                 ; argumento r
        push rax
        mov  rax, rbp               ; enlace estático de inner: el marco de outer
        push rax
        call inner
        add  rsp, 0x10

inner:  push rbp
        mov  rbp, rsp
        mov  rax, [rbp+0x10]        ; enlace estático: marco de outer
        mov  rax, [rax+0x18]        ; p, el parámetro de outer
        push rax
        mov  rax, [rbp+0x18]        ; r, el parámetro propio
        mov  rcx, rax
        pop  rax
        add  rax, rcx               ; p + r
        mov  rsp, rbp
        pop  rbp
        ret
```

En la versión anterior esto lo resolvía GCC con su extensión de funciones anidadas; ahora es parte del generador.

### Otras decisiones

| Tema | Decisión |
|---|---|
| Posiciones de las variables | Una pasada previa asigna a cada variable su nivel y su posición antes de generar. Hace falta porque un valor por defecto se calcula en la llamada y puede nombrar una variable cuya declaración todavía no se generó |
| Orden del código | Las funciones se generan después del código que las contiene, desde una cola, para no tener que saltar por encima de ellas |
| Llamada antes de la definición y recursión mutua | La etiqueta de una función se crea la primera vez que se pide; el relleno posterior completa la llamada cuando se genera la función |
| Límite de recursión | Un contador en la bss lleva las llamadas en curso. Se verifica y se incrementa antes de calcular los argumentos, como `vx_enter` en la versión anterior. Con 1000 llamadas en curso, la siguiente reporta `maximum recursion depth (1000) exceeded in 'f'` en lugar de agotar la pila |
| Función sin `return` | Si una función con tipo llega al final sin `return`, se reporta `function 'f' must return a value of type T` con la línea de su definición |

### Pruebas

| Prueba | Qué demuestra |
|---|---|
| `tests/31_native_functions.vx` | Recursión simple y mutua, llamada antes de la definición, parámetros nombrados y por defecto, sobrecarga, tres niveles de funciones anidadas que modifican variables externas, `void` con `return` temprano, `return` dentro de ciclos, orden de evaluación con llamadas que tienen efectos (incluido que el valor de un `switch` se calcula una sola vez) y 1000 llamadas en curso |
| `tests/12_recursion_limit.vx` | Recuperada de `main`: recursión infinita detenida con un error |
| `tests/13_missing_return.vx` | Recuperada de `main`: función `int` que termina sin `return` |

`tests/09_functions.vx` de `main` usa `float` y cadenas; se recupera en la etapa 5.
