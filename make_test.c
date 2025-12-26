#include <stdio.h>

void w(FILE* f, const unsigned char* b, size_t s) { fwrite(b, 1, s, f); }

int main() {
    FILE* f = fopen("test.b", "wb");
    if (!f) return 1;

    // Helpers
    unsigned char kw_int[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; 
    unsigned char kw_ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20};
    unsigned char kw_print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20};
    unsigned char kw_if[] = {0xD8, 0xA5, 0xD8, 0xB0, 0xD8, 0xA7, 0x20};
    unsigned char main_name[] = {0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB1, 0xD8, 0xA6, 0xD9, 0x8A, 0xD8, 0xB3, 0xD9, 0x8A, 0xD8, 0xA9};
    
    unsigned char dot[] = {0x2E, 0x0A};
    unsigned char lp[] = {0x28, 0x20};
    unsigned char rp[] = {0x29, 0x20};
    unsigned char lb[] = {0x7B, 0x0A};
    unsigned char rb[] = {0x7D, 0x0A};
    unsigned char quote[] = {0x22};
    unsigned char n1[] = {0xD9, 0xA1};
    unsigned char n0[] = {0xD9, 0xA0};
    
    // && (0x26 0x26)
    unsigned char and[] = {0x26, 0x26, 0x20};
    // || (0x7C 0x7C)
    unsigned char or[] = {0x7C, 0x7C, 0x20};
    // ! (0x21)
    unsigned char not[] = {0x21};

    // Strings
    unsigned char msg_and[] = {0x41, 0x4E, 0x44, 0x20, 0x57, 0x6F, 0x72, 0x6B, 0x73}; // "AND Works"
    unsigned char msg_or[] = {0x4F, 0x52, 0x20, 0x57, 0x6F, 0x72, 0x6B, 0x73}; // "OR Works"

    // 1. Main
    w(f, kw_int, sizeof(kw_int)); w(f, main_name, sizeof(main_name)); 
    w(f, lp, sizeof(lp)); w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));

    // 2. if (1 && 1) print "AND Works"
    w(f, kw_if, sizeof(kw_if)); w(f, lp, sizeof(lp));
    w(f, n1, sizeof(n1)); w(f, and, sizeof(and)); w(f, n1, sizeof(n1));
    w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));
    w(f, kw_print, sizeof(kw_print)); w(f, quote, 1); w(f, msg_and, sizeof(msg_and)); w(f, quote, 1); w(f, dot, sizeof(dot));
    w(f, rb, sizeof(rb));

    // 3. if (0 || 1) print "OR Works"
    w(f, kw_if, sizeof(kw_if)); w(f, lp, sizeof(lp));
    w(f, n0, sizeof(n0)); w(f, or, sizeof(or)); w(f, n1, sizeof(n1));
    w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));
    w(f, kw_print, sizeof(kw_print)); w(f, quote, 1); w(f, msg_or, sizeof(msg_or)); w(f, quote, 1); w(f, dot, sizeof(dot));
    w(f, rb, sizeof(rb));

    // Return 0
    w(f, kw_ret, sizeof(kw_ret)); w(f, n0, sizeof(n0)); w(f, dot, sizeof(dot));
    w(f, rb, sizeof(rb));

    fclose(f);
    return 0;
}