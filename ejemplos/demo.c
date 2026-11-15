/* Generado por vortexc, el compilador de Vortex. No editar. */
#include "vx_runtime.h"

int main(void) {
    auto long long f_fact_1(long long);
    long long f_fact_1(long long v_n_2) {
        if ((v_n_2 <= 1LL)) {
            { long long vx_r = 1LL; vx_depth--; return vx_r; }
        }
        { long long vx_r = (v_n_2 * (vx_enter("fact", 4), f_fact_1((v_n_2 - 1LL)))); vx_depth--; return vx_r; }
        vx_no_return("fact", "int", 2);
    }
    VxArr v_v_3 = vx_vec(3, (VxVal[]){ { .i = 1LL }, { .i = 2LL }, { .i = 3LL } });
    vx_try_begin(8);
    if (setjmp(vx_handlers[vx_try_top - 1].jump) == 0) {
        {
            {
                const char * vx_p_1 = "fact(5) =";
                long long vx_p_2 = (vx_enter("fact", 9), f_fact_1(5LL));
                const char * vx_p_3 = "| sum =";
                long long vx_p_4 = vx_reduce(VX_SUM, vx_arr_scalar(VX_MUL, v_v_3, (VxVal){ .i = 2LL }, VX_INT, 0, 9), VX_INT, 9).i;
                vx_print_s(vx_p_1); putchar(' ');
                vx_print_i(vx_p_2); putchar(' ');
                vx_print_s(vx_p_3); putchar(' ');
                vx_print_i(vx_p_4);
                putchar('\n');
            }
            {
                long long vx_p_5 = vx_get(v_v_3, 3LL, 10, "vector<int>").i;
                vx_print_i(vx_p_5);
                putchar('\n');
            }
        }
        vx_try_top--;
    } else {
        const char *v_e_4 = vx_exc_text();
        {
            {
                const char * vx_p_6 = "caught:";
                const char * vx_p_7 = v_e_4;
                vx_print_s(vx_p_6); putchar(' ');
                vx_print_s(vx_p_7);
                putchar('\n');
            }
        }
    }
    return 0;
}
