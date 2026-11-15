/* Generado por vortexc, el compilador de Vortex. No editar. */
#include "vx_runtime.h"

int main(void) {
    long long v_a_1 = 7LL;
    long long v_b_2 = 2LL;
    double v_pi_3 = 3.14159;
    int v_ok_4 = 1;
    const char * v_name_5 = "Vortex";
    {
        long long vx_p_1 = (v_a_1 + (v_b_2 * 3LL));
        vx_print_i(vx_p_1);
        putchar('\n');
    }
    {
        long long vx_p_2 = ((v_a_1 + v_b_2) * 3LL);
        vx_print_i(vx_p_2);
        putchar('\n');
    }
    {
        long long vx_p_3 = vx_idiv(v_a_1, v_b_2, 10);
        long long vx_p_4 = vx_imod(v_a_1, v_b_2, 10);
        vx_print_i(vx_p_3); putchar(' ');
        vx_print_i(vx_p_4);
        putchar('\n');
    }
    {
        double vx_p_5 = vx_fdiv(((double)(v_a_1)), 2.0, 11);
        vx_print_f(vx_p_5);
        putchar('\n');
    }
    {
        long long vx_p_6 = ((-v_a_1) + 10LL);
        vx_print_i(vx_p_6);
        putchar('\n');
    }
    {
        double vx_p_7 = (((double)(2LL)) * v_pi_3);
        vx_print_f(vx_p_7);
        putchar('\n');
    }
    {
        int vx_p_8 = (((v_a_1 > v_b_2) && (!v_ok_4)) || (v_a_1 == 7LL));
        vx_print_b(vx_p_8);
        putchar('\n');
    }
    {
        const char * vx_p_9 = vx_cat(vx_cat(vx_cat("Hello ", v_name_5), " "), vx_str_i(v_a_1));
        vx_print_s(vx_p_9);
        putchar('\n');
    }
    double v_f_6 = ((double)(5LL));
    {
        double vx_p_10 = v_f_6;
        vx_print_f(vx_p_10);
        putchar('\n');
    }
    v_a_1 = (v_a_1 + 1LL);
    {
        long long vx_p_11 = v_a_1;
        vx_print_i(vx_p_11);
        putchar('\n');
    }
    return 0;
}
