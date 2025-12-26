#include <stdio.h>

void w(FILE* f, const unsigned char* b, size_t s) { fwrite(b, 1, s, f); }

int main() {
    FILE* f = fopen("test.b", "wb");
    if (!f) return 1;

    // --- Keywords ---
    unsigned char kw_int[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; // صحيح
    unsigned char kw_if[] = {0xD8, 0xA5, 0xD8, 0xB0, 0xD8, 0xA7, 0x20}; // إذا
    unsigned char kw_ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20}; // إرجع
    unsigned char kw_print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20}; // اطبع
    
    // Identifiers
    unsigned char name_main[] = {0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB1, 0xD8, 0xA6, 0xD9, 0x8A, 0xD8, 0xB3, 0xD9, 0x8A, 0xD8, 0xA9}; // الرئيسية
    unsigned char name_fact[] = {0xD9, 0x85, 0xD8, 0xB6, 0xD8, 0xB1, 0xD9, 0x88, 0xD8, 0xA8}; // مضروب (factorial)
    unsigned char var_n[] = {0xD9, 0x86, 0x20}; // ن (n)

    // Symbols
    unsigned char lte[] = {0x3C, 0x3D, 0x20}; // <=
    unsigned char mul[] = {0x2A, 0x20}; // *
    unsigned char sub[] = {0x2D, 0x20}; // -
    unsigned char dot[] = {0x2E, 0x0A}; // . \n
    
    unsigned char lp[] = {0x28, 0x20}; // (
    unsigned char rp[] = {0x29, 0x20}; // )
    unsigned char lb[] = {0x7B, 0x0A}; // { \n
    unsigned char rb[] = {0x7D, 0x0A}; // } \n
    unsigned char comma[] = {0x2C, 0x20}; // ,

    // Numbers
    unsigned char n0[] = {0xD9, 0xA0}; // ٠
    unsigned char n1[] = {0xD9, 0xA1}; // ١
    unsigned char n5[] = {0xD9, 0xA5}; // ٥

    // --- Code Construction ---
    
    // Function: int factorial(int n) {
    // صحيح مضروب(صحيح ن) {
    w(f, kw_int, sizeof(kw_int)); w(f, name_fact, sizeof(name_fact));
    w(f, lp, sizeof(lp));
    w(f, kw_int, sizeof(kw_int)); w(f, var_n, sizeof(var_n));
    w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));

        // if (n <= 1) { return 1. }
        // إذا (ن <= ١) { إرجع ١. }
        w(f, kw_if, sizeof(kw_if)); w(f, lp, sizeof(lp));
        w(f, var_n, sizeof(var_n)); w(f, lte, sizeof(lte)); w(f, n1, sizeof(n1));
        w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));
            w(f, kw_ret, sizeof(kw_ret)); w(f, n1, sizeof(n1)); w(f, dot, sizeof(dot));
        w(f, rb, sizeof(rb));

        // return n * factorial(n - 1).
        // إرجع ن * مضروب(ن - ١).
        w(f, kw_ret, sizeof(kw_ret));
        w(f, var_n, sizeof(var_n)); w(f, mul, sizeof(mul));
        w(f, name_fact, sizeof(name_fact)); w(f, lp, sizeof(lp));
            w(f, var_n, sizeof(var_n)); w(f, sub, sizeof(sub)); w(f, n1, sizeof(n1));
        w(f, rp, sizeof(rp));
        w(f, dot, sizeof(dot));

    w(f, rb, sizeof(rb)); // }

    // Main: int main() {
    // صحيح الرئيسية() {
    w(f, kw_int, sizeof(kw_int)); w(f, name_main, sizeof(name_main)); 
    w(f, lp, sizeof(lp)); w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));

        // print factorial(5).
        // اطبع مضروب(٥).
        w(f, kw_print, sizeof(kw_print));
        w(f, name_fact, sizeof(name_fact)); w(f, lp, sizeof(lp)); w(f, n5, sizeof(n5)); w(f, rp, sizeof(rp));
        w(f, dot, sizeof(dot));

        // return 0.
        // إرجع ٠.
        w(f, kw_ret, sizeof(kw_ret)); w(f, n0, sizeof(n0)); w(f, dot, sizeof(dot));
    
    w(f, rb, sizeof(rb)); // } Main

    fclose(f);
    printf("Generated test.b with Recursive Factorial.\n");
    return 0;
}