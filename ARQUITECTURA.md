# Compilador de Vortex

`vortexc` compila un programa Vortex (`.vx`) a un ejecutable nativo. Toma el analizador léxico y la gramática del punto 4 y les agrega tres etapas: construcción de un árbol de sintaxis abstracta, análisis semántico y generación de código. El código generado es C, y GCC lo convierte en el ejecutable.

## 1. Uso

```
bash run                             # construye vortexc
./vortexc programa.vx                # genera programa.c y el ejecutable programa
./vortexc programa.vx -o salida      # elige el nombre del ejecutable
./vortexc programa.vx --run          # compila en una carpeta temporal y ejecuta
bash tests/run_tests.sh              # corre las 33 pruebas
```

`vortexc` necesita `gcc` instalado y que `vx_runtime.h` esté en su misma carpeta.

## 2. Etapas

| # | Etapa | Archivo | Entrada → salida | Errores que reporta |
|---|---|---|---|---|
| 1 | Análisis léxico | `lexer.l` (Flex) | caracteres → tokens | `[Lexical error]` |
| 2 | Análisis sintáctico | `parser.y` (Bison) | tokens → AST | `[Syntax error]`, `[Invalid construct]` |
| 3 | Análisis semántico | `semantic.c` | AST → AST anotado con tipos y símbolos | `[Semantic error]` |
| 4 | Generación de código | `codegen.c` | AST anotado → programa en C | ninguno |
| 5 | Compilación nativa | GCC | programa en C + `vx_runtime.h` → ejecutable | ninguno |
| 6 | Ejecución | el ejecutable | | `[Runtime error]`, `[Assertion error]`, `[Uncaught exception]` |

`main.c` coordina las etapas. Si las etapas 1 a 3 encuentran errores, se reportan todos con su línea, se imprime `N error(s) found; program not compiled.` y no se genera ningún archivo. El análisis semántico solo se ejecuta si no hubo errores léxicos ni sintácticos, porque necesita un árbol completo.

`vortex.h` declara las estructuras que comparten las etapas (tipos, nodos del AST, símbolos) y `ast.c` implementa su construcción y las reglas de compatibilidad entre tipos.

## 3. Árbol de sintaxis abstracta

En el punto 4 las acciones de la gramática solo validaban. Aquí cada regla construye un nodo (`Node`) con su clase, su línea en el programa fuente, un texto (nombre o lexema) y hasta cuatro hijos; las listas (sentencias de un bloque, argumentos, elementos de un literal) se encadenan por el campo `next`. La gramática, la precedencia por estratificación y las reglas de error son las mismas del punto 4, y Bison la procesa sin conflictos.

Los paréntesis no generan nodo: la estructura del árbol ya expresa el orden de evaluación. Para `a + b * c` el árbol es `+(a, *(b, c))`.

## 4. Análisis semántico

`semantic.c` recorre el árbol una vez y verifica lo que una gramática libre de contexto no puede expresar.

### 4.1 Ámbitos y shadowing

Cada bloque, función, ciclo `for` y `catch` abre un ámbito con su propia tabla de símbolos, enlazada al ámbito que lo contiene. La búsqueda de un nombre va del ámbito actual hacia afuera, de modo que una declaración interna oculta a la externa sin modificarla. Declarar dos veces el mismo nombre en el mismo ámbito es un error.

En `int x = x * 10;` el valor inicial se analiza antes de registrar la nueva variable, así que la `x` de la derecha es la del ámbito externo.

Cada símbolo recibe un número único. El generador lo usa para nombrar la variable en C (`v_x_3`), lo que hace que el shadowing no dependa de las reglas de nombres de C.

### 4.2 Sistema de tipos

El lenguaje es de tipado estático: el tipo de toda expresión se conoce al compilar.

| Regla | Detalle |
|---|---|
| Promoción implícita | `int` → `float`, `vector<int>` → `vector<float>`, `matrix<int>` → `matrix<float>`. No hay conversión en sentido contrario |
| Aritmética `+ - * /` | `int` con `int` da `int` (división entera); si interviene un `float`, da `float` |
| Residuo `%` | Solo entre enteros |
| Concatenación | `string + x` y `x + string`, con `x` de cualquier tipo básico, dan `string` |
| Arreglos | Las operaciones aritméticas entre dos arreglos, o entre un arreglo y un escalar, se aplican elemento a elemento |
| Comparación | `< > <= >=` entre números o entre cadenas; `== !=` entre valores del mismo tipo, incluidos vectores y matrices |
| Condiciones | `if`, `while`, `for`, `assert`, `&&`, `\|\|` y `!` exigen `bool`; un entero no es una condición |
| Literales | `[1, 2.5]` es `vector<float>`; `[[1, 2], [3, 4]]` es `matrix<int>`; `[]` es compatible con cualquier vector o matriz |

### 4.3 Funciones y sobrecarga

Antes de analizar las sentencias de un bloque se registran todas las funciones definidas en él. Por eso una función puede llamarse a sí misma o llamar a otra definida más abajo (recursión mutua).

La convención de sobrecarga es: dos funciones pueden compartir nombre si difieren en la cantidad o en el tipo de sus parámetros. Para cada llamada se procede así:

1. Se reúnen las versiones visibles con ese nombre.
2. Para cada versión se ubican los argumentos en los parámetros: los posicionales en orden, los nombrados por su nombre, y los faltantes con su valor por defecto. Si un argumento no encaja, la versión se descarta.
3. Se cuenta cuántos argumentos necesitan promoción y se elige la versión que necesita menos.
4. Si ninguna versión encaja se reporta `no version of 'f' matches the call f(bool)`; si dos empatan, `ambiguous call f(int, int)`.

El resultado queda en el nodo de la llamada: la función elegida y los argumentos ya ordenados según sus parámetros.

### 4.4 Otras verificaciones

`break` y `continue` solo dentro de un ciclo; `return` solo dentro de una función y con un valor del tipo declarado; el destino de una asignación debe ser una variable, `v[i]`, `m[i]` o `m[i][j]`; los `case` deben ser del tipo del `switch`.

## 5. Generación de código

`codegen.c` recorre el árbol anotado y escribe un programa en C.

### 5.1 Correspondencia entre Vortex y C

| Vortex | C generado |
|---|---|
| `int`, `float`, `bool`, `string` | `long long`, `double`, `int`, `const char *` |
| `vector<T>`, `matrix<T>` | `VxArr`: cantidad de elementos o filas, columnas y un arreglo de `VxVal` |
| Programa completo | El cuerpo de `main` |
| Variable `x` | `v_x_N`, con `N` único |
| Función `f` | Función anidada `f_f_N`; cada versión sobrecargada tiene su propio `N` |
| `a / b`, `a % b` | `vx_idiv`, `vx_imod`, `vx_fdiv`: verifican el divisor cero |
| `v[i]` | `vx_get` o `vx_at`: verifican el índice |
| `f(x)` | `(vx_enter("f", línea), f_f_N(x))`: cuenta la profundidad de llamadas |
| `switch` | Cadena `if / else if`, sin paso entre ramas |
| `try { } catch (e) { }` | `setjmp` sobre una pila de manejadores; el error hace `longjmp` |
| `print(a, b)` | Se calculan todos los valores y luego se imprimen separados por un espacio |

### 5.2 Funciones anidadas

Todo el programa Vortex se genera dentro de `main`, y cada función de Vortex como una función anidada de C, una extensión de GCC. Una función anidada ve las variables de la función que la contiene, que es justamente la semántica de las funciones anidadas de Vortex; las funciones del nivel superior quedan anidadas en `main` y ven las variables globales del programa de la misma manera. Al inicio de cada bloque se emiten los prototipos (`auto`) de sus funciones para permitir la recursión mutua.

### 5.3 Valores por copia

Los vectores y matrices se asignan, se pasan y se retornan por valor. Cuando el origen es una variable, el generador emite `vx_copy` para que el destino tenga su propia memoria; las demás expresiones ya producen un arreglo nuevo.

### 5.4 Excepciones

Cada `try` registra un manejador (`vx_try_begin`) y llama a `setjmp`. Un error de ejecución, un `assert` fallido o un `throw` llaman a `vx_raise`: si hay un manejador activo, lo retira y salta a su `catch` con `longjmp`; si no hay ninguno, el programa termina mostrando la clase de error y la línea.

Salir de un `try` con `break`, `continue` o `return` dejaría su manejador registrado. El generador lleva la cuenta de los `try` abiertos y, antes de cada salto, emite el código que retira los que quedan atrás.

La variable del `catch` es un `string`. Para los errores del lenguaje contiene el mensaje y la línea (`division by zero (line 8)`); para `throw`, exactamente el valor lanzado.

### 5.5 Ejemplo

Para este programa (`ejemplos/demo.vx`):

```
func int fact(int n) {
    if (n <= 1) { return 1; }
    return n * fact(n - 1);
}
```

el compilador genera:

```c
int main(void) {
    auto long long f_fact_1(long long);
    long long f_fact_1(long long v_n_2) {
        if ((v_n_2 <= 1LL)) {
            { long long vx_r = 1LL; vx_depth--; return vx_r; }
        }
        { long long vx_r = (v_n_2 * (vx_enter("fact", 4), f_fact_1((v_n_2 - 1LL)))); vx_depth--; return vx_r; }
        vx_no_return("fact", "int", 2);
    }
    return 0;
}
```

## 6. Biblioteca de ejecución

`vx_runtime.h` se incluye en cada programa generado. Contiene lo que el lenguaje hace en tiempo de ejecución y que C no resuelve por sí mismo: vectores y matrices con índices verificados, aritmética elemento a elemento, reducciones (`sum`, `prod`, `max`, `min`), cadenas, impresión, excepciones y el control de profundidad de llamadas.

Errores de ejecución que detecta, todos con su línea:

| Error | Mensaje |
|---|---|
| División o residuo entre cero | `division by zero` |
| Índice fuera de rango | `index 3 out of range for vector<int> of length 3` |
| Filas de distinto largo | `matrix rows must have the same length (row 0 has 2, row 1 has 1)` |
| Recursión sin fin | `maximum recursion depth (1000) exceeded in 'f'` |
| Función que termina sin `return` | `function 'f' must return a value of type int` |
| Operación entre arreglos de distinta forma | `vectors of different length (5 and 3) in an element-wise operation` |
| `max` o `min` de un arreglo vacío | `max of an empty vector` |

## 7. Decisiones y limitaciones

- **Destino C en lugar de ensamblador.** Generar C permite cubrir todo el lenguaje con un generador de unas 500 líneas y delega en GCC la selección de instrucciones. El ejecutable resultante es nativo.
- **Dependencia de GCC.** Las funciones anidadas son una extensión de GCC; el código generado no compila con otros compiladores de C.
- **Sin optimización (`-O0`).** Garantiza que las variables locales conserven su valor cuando `longjmp` regresa a un `catch`.
- **Memoria.** Las cadenas y los arreglos creados durante la ejecución no se liberan; el sistema operativo recupera la memoria al terminar el programa. Es adecuado para scripts, no para procesos de larga duración.
- **Profundidad de llamadas.** Se limita a 1000 para reportar una recursión infinita como error del lenguaje y no como un fallo del sistema.
- **Verificación del `return`.** Que una función con tipo termine sin `return` se detecta al ejecutar, no al compilar.
- **Funciones y variables capturadas.** Una función puede llamarse antes de su definición, pero si usa una variable externa, esa variable debe haberse inicializado antes de la llamada; el compilador no lo verifica.

## 8. Pruebas

`tests/` contiene 33 programas; cada `.vx` tiene un `.out` con la salida exacta esperada, y `tests/run_tests.sh` compila, ejecuta y compara.

| Grupo | Archivos | Qué demuestra |
|---|---|---|
| Programas que se ejecutan | 01, 02, 06, 09, 16, 21, 26, 28 | Tipos, ámbitos, control de flujo, funciones, arreglos, excepciones, un script científico completo y casos límite del código generado |
| Errores de ejecución | 04, 12, 13, 17, 19, 22, 23 | El programa corre hasta el error y lo reporta con su línea |
| Errores semánticos | 05, 08, 10, 11, 15, 20, 27 | Se detectan al compilar; no se genera ejecutable |
| Errores sintácticos | 03, 07, 14, 18, 24, 25 | Los mismos mensajes del punto 4 |
| Errores léxicos | `lexical errors/01`–`05` | Los mismos mensajes del punto 4 |

Los programas de los dos primeros grupos producen la misma salida que la especificación original del lenguaje (`punto_4/tests/out_interprete/`).
