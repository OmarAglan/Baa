#include <stdio.h>

void w(FILE* f, const unsigned char* b, size_t s) { fwrite(b, 1, s, f); }

int main() {
    FILE* f = fopen("test.b", "wb");
    if (!f) return 1;

    // Helpers
    unsigned char kw_int[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; 
    unsigned char kw_ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20};
    unsigned char kw_print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20};
    unsigned char name_main[] = {0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB1, 0xD8, 0xA6, 0xD9, 0x8A, 0xD8, 0xB3, 0xD9, 0x8A, 0xD8, 0xA9};
    unsigned char var_s[] = {0xD8, 0xB3, 0x20}; 
    unsigned char eq[] = {0x3D, 0x20};
    unsigned char minus[] = {0x2D};
    unsigned char n10[] = {0xD9, 0xA1, 0xD9, 0xA0};
    unsigned char n0[] = {0xD9, 0xA0};
    unsigned char dot[] = {0x2E, 0x0A};
    unsigned char lp[] = {0x28, 0x20};
    unsigned char rp[] = {0x29, 0x20};
    unsigned char lb[] = {0x7B, 0x0A};
    unsigned char rb[] = {0x7D, 0x0A};
    unsigned char quote[] = {0x22};

    // String: "Hello World" (Arabic)
    // مرحبا بالعالم
    unsigned char str_hello[] = {0xD9, 0x85, 0xD8, 0xB1, 0xD8, 0xAD, 0xD8, 0xA8, 0xD8, 0xA7, 0x20, 
                                 0xD8, 0xA8, 0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB9, 0xD8, 0xA7, 0xD9, 0x84, 0xD9, 0x85};
    // String: "s = "
    unsigned char str_s_eq[] = {0x73, 0x20, 0x3D, 0x20};

    // 1. Function Main
    w(f, kw_int, sizeof(kw_int)); w(f, name_main, sizeof(name_main));
    w(f, lp, sizeof(lp)); w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));

    // 2. Print "Hello World"
    w(f, kw_print, sizeof(kw_print)); 
    w(f, quote, 1); w(f, str_hello, sizeof(str_hello)); w(f, quote, 1);
    w(f, dot, sizeof(dot));

    // 3. int s = -10
    w(f, kw_int, sizeof(kw_int)); w(f, var_s, sizeof(var_s)); w(f, eq, sizeof(eq));
    w(f, minus, 1); w(f, n10, sizeof(n10)); // Unary minus
    w(f, dot, sizeof(dot));

    // 4. Print "s = "
    w(f, kw_print, sizeof(kw_print));
    w(f, quote, 1); w(f, str_s_eq, sizeof(str_s_eq)); w(f, quote, 1);
    w(f, dot, sizeof(dot));

    // 5. Print s
    w(f, kw_print, sizeof(kw_print)); w(f, var_s, sizeof(var_s));
    w(f, dot, sizeof(dot));

    // 6. Return 0
    w(f, kw_ret, sizeof(kw_ret)); w(f, n0, sizeof(n0)); w(f, dot, sizeof(dot));
    w(f, rb, sizeof(rb));

    fclose(f);
    return 0;
}