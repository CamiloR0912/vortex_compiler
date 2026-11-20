/* Valores de prueba de reales.c, uno por cada camino de fmt_float */
#include <math.h>

static const double valores[] = {
    /* Casos especiales: cero con signo, infinitos y NaN */
    0.0, -0.0, INFINITY, -INFINITY, NAN, -NAN,
    /* Enteros: se agrega ".0" */
    5.0, -7.0, 100.0, 123456789012345.0, 999999999999999.0,
    /* Notación normal con decimales */
    2 * 3.14159, 0.1 + 0.2, 1.0 / 3.0, 2.0 / 3.0, -2.5, 0.5, 3.5,
    /* Doble redondeo: el producto cae en un empate y decide el error exacto */
    -446.0 / 7.0, -442.0 / 7.0, 40.0 / 7.0, 44.0 / 7.0,
    /* Límites de la notación normal: E = -4 y E = 14 */
    0.0001, 0.0001234, 0.00099999999999999, 123456789012345.6,
    /* Notación científica: E < -4 o E >= 15 */
    0.00001, 0.00001234, 1e15, 1e16, 9999999999999998.0, 1e22, 1e23, 1e100,
    -2.5e-200, 1.7976931348623157e308, 2.2250738585072014e-308,
    /* El redondeo a 15 dígitos agrega un dígito: 9.99...96 -> 1e+01 */
    9.9999999999999996, 99999999999999.996,
    /* Subnormales */
    4.9406564584124654e-324, 1e-320,
};
