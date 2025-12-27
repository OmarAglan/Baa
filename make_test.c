#include <stdio.h>

void w(FILE* f, const unsigned char* b, size_t s) { fwrite(b, 1, s, f); }

int main() {
    FILE* f = fopen("test.b", "wb");
    if (!f) return 1;

    // --- Keywords ---
    unsigned char kw_int[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; // صحيح
    unsigned char kw_for[] = {0xD9, 0x84, 0xD9, 0x83, 0xD9, 0x84, 0x20}; // لكل
    unsigned char kw_if[] = {0xD8, 0xA5, 0xD8, 0xB0, 0xD8, 0xA7, 0x20}; // إذا
    unsigned char kw_ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20}; // إرجع
    unsigned char kw_print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20}; // اطبع
    
    // New Keywords
    unsigned char kw_break[] = {0xD8, 0xAA, 0xD9, 0x88, 0xD9, 0x82, 0xD9, 0x81, 0x20}; // توقف
    unsigned char kw_continue[] = {0xD8, 0xA7, 0xD8, 0xB3, 0xD8, 0xAA, 0xD9, 0x85, 0xD8, 0xB1, 0x20}; // استمر
    
    // Identifiers
    unsigned char name_main[] = {0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB1, 0xD8, 0xA6, 0xD9, 0x8A, 0xD8, 0xB3, 0xD9, 0x8A, 0xD8, 0xA9}; // الرئيسية
    unsigned char var_s[] = {0xD8, 0xB3, 0x20}; // س (iterator)
    
    // Symbols
    unsigned char eq[] = {0x3D, 0x3D, 0x20}; // ==
    unsigned char assign[] = {0x3D, 0x20}; // =
    unsigned char lt[] = {0x3C, 0x20}; // <
    unsigned char inc[] = {0x2B, 0x2B, 0x20}; // ++
    unsigned char dot[] = {0x2E, 0x0A}; // . \n
    unsigned char semi[] = {0xD8, 0x9B, 0x20}; // ؛
    
    unsigned char lp[] = {0x28, 0x20}; // (
    unsigned char rp[] = {0x29, 0x20}; // )
    unsigned char lb[] = {0x7B, 0x0A}; // { \n
    unsigned char rb[] = {0x7D, 0x0A}; // } \n

    // Numbers
    unsigned char n0[] = {0xD9, 0xA0}; // ٠
    unsigned char n5[] = {0xD9, 0xA5}; // ٥
    unsigned char n8[] = {0xD9, 0xA8}; // ٨
    unsigned char n10[] = {0xD9, 0xA1, 0xD9, 0xA0}; // ١٠

    // --- Code Construction ---

    // صحيح الرئيسية() {
    w(f, kw_int, sizeof(kw_int)); w(f, name_main, sizeof(name_main)); 
    w(f, lp, sizeof(lp)); w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));

        // لكل (صحيح س = ٠؛ س < ١٠؛ س++) {
        w(f, kw_for, sizeof(kw_for)); w(f, lp, sizeof(lp));
        w(f, kw_int, sizeof(kw_int)); w(f, var_s, sizeof(var_s)); w(f, assign, sizeof(assign)); w(f, n0, sizeof(n0)); w(f, semi, sizeof(semi));
        w(f, var_s, sizeof(var_s)); w(f, lt, sizeof(lt)); w(f, n10, sizeof(n10)); w(f, semi, sizeof(semi));
        w(f, var_s, sizeof(var_s)); w(f, inc, sizeof(inc));
        w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));

            // إذا (س == ٥) { استمر. }
            w(f, kw_if, sizeof(kw_if)); w(f, lp, sizeof(lp));
            w(f, var_s, sizeof(var_s)); w(f, eq, sizeof(eq)); w(f, n5, sizeof(n5));
            w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));
                w(f, kw_continue, sizeof(kw_continue)); w(f, dot, sizeof(dot));
            w(f, rb, sizeof(rb));

            // إذا (س == ٨) { توقف. }
            w(f, kw_if, sizeof(kw_if)); w(f, lp, sizeof(lp));
            w(f, var_s, sizeof(var_s)); w(f, eq, sizeof(eq)); w(f, n8, sizeof(n8));
            w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));
                w(f, kw_break, sizeof(kw_break)); w(f, dot, sizeof(dot));
            w(f, rb, sizeof(rb));

            // اطبع س.
            w(f, kw_print, sizeof(kw_print)); w(f, var_s, sizeof(var_s)); w(f, dot, sizeof(dot));

        w(f, rb, sizeof(rb)); // } Loop

        // إرجع ٠.
        w(f, kw_ret, sizeof(kw_ret)); w(f, n0, sizeof(n0)); w(f, dot, sizeof(dot));
    
    w(f, rb, sizeof(rb)); // } Main

    fclose(f);
    printf("Generated test.b with Break/Continue.\n");
    return 0;
}