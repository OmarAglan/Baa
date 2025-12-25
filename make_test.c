#include <stdio.h>

void w(FILE* f, const unsigned char* b, size_t s) { fwrite(b, 1, s, f); }

int main() {
    FILE* f = fopen("test.b", "wb");
    if (!f) return 1;

    // --- Helpers ---
    unsigned char kw_int[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; 
    unsigned char kw_ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20};
    unsigned char kw_print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20};
    unsigned char kw_if[] = {0xD8, 0xA5, 0xD8, 0xB0, 0xD8, 0xA7, 0x20};
    
    unsigned char name_main[] = {0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB1, 0xD8, 0xA6, 0xD9, 0x8A, 0xD8, 0xB3, 0xD9, 0x8A, 0xD8, 0xA9};
    unsigned char var_s[] = {0xD8, 0xB3, 0x20}; // s
    unsigned char var_y[] = {0xD8, 0xB5, 0x20}; // y (sad)
    unsigned char var_z[] = {0xD8, 0xB9, 0x20}; // z (ain)

    unsigned char op_add[] = {0x2B, 0x20};
    unsigned char op_mul[] = {0x2A, 0x20};
    unsigned char op_div[] = {0x2F, 0x20};
    unsigned char op_mod[] = {0x25, 0x20};
    unsigned char op_gt[]  = {0x3E, 0x20}; // >
    
    unsigned char eq[] = {0x3D, 0x20};
    unsigned char dot[] = {0x2E, 0x0A};
    unsigned char lp[] = {0x28, 0x20};
    unsigned char rp[] = {0x29, 0x20};
    unsigned char lb[] = {0x7B, 0x0A};
    unsigned char rb[] = {0x7D, 0x0A};
    
    unsigned char n10[] = {0xD9, 0xA1, 0xD9, 0xA0, 0x20};
    unsigned char n5[] = {0xD9, 0xA5, 0x20};
    unsigned char n2[] = {0xD9, 0xA2};
    unsigned char n20[] = {0xD9, 0xA2, 0xD9, 0xA0, 0x20};
    unsigned char n3[] = {0xD9, 0xA3};
    unsigned char n1[] = {0xD9, 0xA1};
    unsigned char n0[] = {0xD9, 0xA0};

    // 1. Main Function
    w(f, kw_int, sizeof(kw_int)); w(f, name_main, sizeof(name_main)); 
    w(f, lp, sizeof(lp)); w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));

    // 2. int s = 10 + 5 * 2;
    w(f, kw_int, sizeof(kw_int)); w(f, var_s, sizeof(var_s)); w(f, eq, sizeof(eq));
    w(f, n10, sizeof(n10)); w(f, op_add, sizeof(op_add));
    w(f, n5, sizeof(n5)); w(f, op_mul, sizeof(op_mul)); w(f, n2, sizeof(n2));
    w(f, dot, sizeof(dot));
    // print s
    w(f, kw_print, sizeof(kw_print)); w(f, var_s, sizeof(var_s)); w(f, dot, sizeof(dot));

    // 3. int y = 20 / 3;
    w(f, kw_int, sizeof(kw_int)); w(f, var_y, sizeof(var_y)); w(f, eq, sizeof(eq));
    w(f, n20, sizeof(n20)); w(f, op_div, sizeof(op_div)); w(f, n3, sizeof(n3));
    w(f, dot, sizeof(dot));
    // print y
    w(f, kw_print, sizeof(kw_print)); w(f, var_y, sizeof(var_y)); w(f, dot, sizeof(dot));

    // 4. int z = 20 % 3;
    w(f, kw_int, sizeof(kw_int)); w(f, var_z, sizeof(var_z)); w(f, eq, sizeof(eq));
    w(f, n20, sizeof(n20)); w(f, op_mod, sizeof(op_mod)); w(f, n3, sizeof(n3));
    w(f, dot, sizeof(dot));
    // print z
    w(f, kw_print, sizeof(kw_print)); w(f, var_z, sizeof(var_z)); w(f, dot, sizeof(dot));

    // 5. if (s > y) print 1;
    w(f, kw_if, sizeof(kw_if)); w(f, lp, sizeof(lp));
    w(f, var_s, sizeof(var_s)); w(f, op_gt, sizeof(op_gt)); w(f, var_y, sizeof(var_y));
    w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));
    w(f, kw_print, sizeof(kw_print)); w(f, n1, sizeof(n1)); w(f, dot, sizeof(dot));
    w(f, rb, sizeof(rb));

    // Return 0
    w(f, kw_ret, sizeof(kw_ret)); w(f, n0, sizeof(n0)); w(f, dot, sizeof(dot));
    w(f, rb, sizeof(rb));

    fclose(f);
    return 0;
}