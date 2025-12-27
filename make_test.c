#include <stdio.h>

void w(FILE* f, const unsigned char* b, size_t s) { fwrite(b, 1, s, f); }

int main() {
    FILE* f = fopen("test.b", "wb");
    if (!f) return 1;

    // --- Keywords ---
    unsigned char kw_int[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; // صحيح
    unsigned char kw_str[] = {0xD9, 0x86, 0xD8, 0xB5, 0x20}; // نص (New)
    unsigned char kw_ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20}; // إرجع
    unsigned char kw_print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20}; // اطبع
    
    // Identifiers
    unsigned char name_main[] = {0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB1, 0xD8, 0xA6, 0xD9, 0x8A, 0xD8, 0xB3, 0xD9, 0x8A, 0xD8, 0xA9}; // الرئيسية
    unsigned char name_msg[] = {0xD8, 0xB1, 0xD8, 0xB3, 0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xA9, 0x20}; // رسالة
    
    // Symbols
    unsigned char eq[] = {0x3D, 0x20}; // =
    unsigned char dot[] = {0x2E, 0x0A}; // . \n
    unsigned char lp[] = {0x28, 0x20}; // (
    unsigned char rp[] = {0x29, 0x20}; // )
    unsigned char lb[] = {0x7B, 0x0A}; // { \n
    unsigned char rb[] = {0x7D, 0x0A}; // } \n

    // Strings & Numbers
    // "مرحبا"
    unsigned char str1[] = {0x22, 0xD9, 0x85, 0xD8, 0xB1, 0xD8, 0xAD, 0xD8, 0xA8, 0xD8, 0xA7, 0x22, 0x20}; 
    // "العالم"
    unsigned char str2[] = {0x22, 0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB9, 0xD8, 0xA7, 0xD9, 0x84, 0xD9, 0x85, 0x22, 0x20}; 
    
    unsigned char n0[] = {0xD9, 0xA0}; // ٠

    // --- Code Construction ---

    // صحيح الرئيسية() {
    w(f, kw_int, sizeof(kw_int)); w(f, name_main, sizeof(name_main)); 
    w(f, lp, sizeof(lp)); w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));

        // نص رسالة = "مرحبا".
        w(f, kw_str, sizeof(kw_str)); w(f, name_msg, sizeof(name_msg));
        w(f, eq, sizeof(eq)); w(f, str1, sizeof(str1));
        w(f, dot, sizeof(dot));

        // اطبع رسالة.
        w(f, kw_print, sizeof(kw_print)); w(f, name_msg, sizeof(name_msg)); w(f, dot, sizeof(dot));

        // رسالة = "العالم".
        w(f, name_msg, sizeof(name_msg)); w(f, eq, sizeof(eq)); w(f, str2, sizeof(str2));
        w(f, dot, sizeof(dot));

        // اطبع رسالة.
        w(f, kw_print, sizeof(kw_print)); w(f, name_msg, sizeof(name_msg)); w(f, dot, sizeof(dot));

        // إرجع ٠.
        w(f, kw_ret, sizeof(kw_ret)); w(f, n0, sizeof(n0)); w(f, dot, sizeof(dot));
    
    w(f, rb, sizeof(rb)); // } Main

    fclose(f);
    printf("Generated test.b with String Variables.\n");
    return 0;
}