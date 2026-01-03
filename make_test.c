#include <stdio.h>

void w(FILE* f, const unsigned char* b, size_t s) { fwrite(b, 1, s, f); }

int main() {
    FILE* f = fopen("test.baa", "wb");
    if (!f) return 1;

    // --- Keywords ---
    unsigned char kw_int[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; // صحيح
    unsigned char kw_print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20}; // اطبع
    unsigned char kw_ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20}; // إرجع
    
    // Identifiers
    unsigned char name_main[] = {0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB1, 0xD8, 0xA6, 0xD9, 0x8A, 0xD8, 0xB3, 0xD9, 0x8A, 0xD8, 0xA9}; // الرئيسية
    unsigned char var_s[] = {0xD8, 0xB3, 0x20}; // س
    unsigned char var_y[] = {0xD8, 0xB5, 0x20}; // ص
    
    // Symbols
    unsigned char eq[] = {0x3D, 0x20}; // =
    unsigned char plus[] = {0x2B, 0x20}; // +
    unsigned char minus[] = {0x2D, 0x20}; // -
    unsigned char mul[] = {0x2A, 0x20}; // *
    unsigned char div[] = {0x2F, 0x20}; // /
    unsigned char dot[] = {0x2E, 0x0A}; // . \n
    
    unsigned char lp[] = {0x28, 0x20}; // (
    unsigned char rp[] = {0x29, 0x20}; // )
    unsigned char lb[] = {0x7B, 0x0A}; // { \n
    unsigned char rb[] = {0x7D, 0x0A}; // } \n

    // Numbers
    unsigned char n0[] = {0xD9, 0xA0}; // ٠
    unsigned char n2[] = {0xD9, 0xA2}; // ٢
    unsigned char n3[] = {0xD9, 0xA3}; // ٣
    unsigned char n4[] = {0xD9, 0xA4}; // ٤
    unsigned char n10[] = {0xD9, 0xA1, 0xD9, 0xA0}; // ١٠
    unsigned char n100[] = {0xD9, 0xA1, 0xD9, 0xA0, 0xD9, 0xA0}; // ١٠٠

    // --- Code Construction ---

    // صحيح الرئيسية() {
    w(f, kw_int, sizeof(kw_int)); w(f, name_main, sizeof(name_main)); 
    w(f, lp, sizeof(lp)); w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));

        // صحيح س = ٢ * ٣ + ٤.
        w(f, kw_int, sizeof(kw_int)); w(f, var_s, sizeof(var_s)); 
        w(f, eq, sizeof(eq)); 
        w(f, n2, sizeof(n2)); w(f, mul, sizeof(mul)); w(f, n3, sizeof(n3)); w(f, plus, sizeof(plus)); w(f, n4, sizeof(n4));
        w(f, dot, sizeof(dot));

        // اطبع س.
        w(f, kw_print, sizeof(kw_print)); w(f, var_s, sizeof(var_s)); w(f, dot, sizeof(dot));

        // صحيح ص = ١٠٠ / ٢ - ١٠.
        w(f, kw_int, sizeof(kw_int)); w(f, var_y, sizeof(var_y));
        w(f, eq, sizeof(eq));
        w(f, n100, sizeof(n100)); w(f, div, sizeof(div)); w(f, n2, sizeof(n2)); w(f, minus, sizeof(minus)); w(f, n10, sizeof(n10));
        w(f, dot, sizeof(dot));

        // اطبع ص.
        w(f, kw_print, sizeof(kw_print)); w(f, var_y, sizeof(var_y)); w(f, dot, sizeof(dot));

        // إرجع ٠.
        w(f, kw_ret, sizeof(kw_ret)); w(f, n0, sizeof(n0)); w(f, dot, sizeof(dot));
    
    w(f, rb, sizeof(rb)); // } Main

    fclose(f);
    printf("Generated test.baa with Constant Folding Optimization.\n");
    return 0;
}