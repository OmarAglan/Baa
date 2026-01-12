/**
 * @file make_test.c
 * @brief Generates a test.baa file with Constants & Immutability tests.
 * @version 0.2.7 Test Suite
 */

#include <stdio.h>

void w(FILE* f, const unsigned char* b, size_t s) { fwrite(b, 1, s, f); }

int main() {
    FILE* f = fopen("test.baa", "wb");
    if (!f) return 1;

    // --- UTF-8 Arabic Fragments ---
    
    // Symbols
    unsigned char sp[] = {0x20};   // space
    unsigned char nl[] = {0x0A};   // newline
    unsigned char eq[] = {0x3D, 0x20}; // = 
    unsigned char dot[] = {0x2E, 0x0A}; // . \n
    unsigned char lp[] = {0x28, 0x29, 0x20}; // () 
    unsigned char lb[] = {0x7B, 0x0A}; // { \n
    unsigned char rb[] = {0x7D, 0x0A}; // } \n

    // Values
    unsigned char n0[] = {0x30}; // 0
    unsigned char n100[] = {0x31, 0x30, 0x30}; // 100
    unsigned char n200[] = {0x32, 0x30, 0x30}; // 200

    // Keywords
    unsigned char kw_const[] = {0xD8, 0xAB, 0xD8, 0xA7, 0xD8, 0xA8, 0xD8, 0xAA, 0x20}; // ثابت 
    unsigned char kw_int[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; // صحيح
    unsigned char kw_print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20}; // اطبع
    unsigned char kw_ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20}; // إرجع
    unsigned char name_main[] = {0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB1, 0xD8, 0xA6, 0xD9, 0x8A, 0xD8, 0xB3, 0xD9, 0x8A, 0xD8, 0xA9}; // الرئيسية

    // Variable names
    unsigned char name_limit[] = {0xD8, 0xAD, 0xD8, 0xAF}; // حد (limit)
    unsigned char name_x[] = {0xD8, 0xB3}; // س (x)

    // --- Code Generation ---

    // Test 1: Global constant declaration
    // ثابت صحيح حد = 100.
    w(f, kw_const, sizeof(kw_const));
    w(f, kw_int, sizeof(kw_int));
    w(f, name_limit, sizeof(name_limit));
    w(f, sp, 1);
    w(f, eq, sizeof(eq));
    w(f, n100, sizeof(n100));
    w(f, dot, sizeof(dot));

    // صحيح الرئيسية() {
    w(f, kw_int, sizeof(kw_int));
    w(f, name_main, sizeof(name_main));
    w(f, lp, sizeof(lp));
    w(f, lb, sizeof(lb));

    // Test 2: Local constant declaration
    // ثابت صحيح س = 200.
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); // indent
    w(f, kw_const, sizeof(kw_const));
    w(f, kw_int, sizeof(kw_int));
    w(f, name_x, sizeof(name_x));
    w(f, sp, 1);
    w(f, eq, sizeof(eq));
    w(f, n200, sizeof(n200));
    w(f, dot, sizeof(dot));

    // Print the constants
    // اطبع حد.
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_print, sizeof(kw_print));
    w(f, name_limit, sizeof(name_limit));
    w(f, dot, sizeof(dot));

    // اطبع س.
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_print, sizeof(kw_print));
    w(f, name_x, sizeof(name_x));
    w(f, dot, sizeof(dot));

    // إرجع 0.
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_ret, sizeof(kw_ret));
    w(f, n0, 1);
    w(f, dot, sizeof(dot));

    w(f, rb, sizeof(rb)); // }

    fclose(f);
    printf("Generated test.baa with constant declaration test.\n");
    printf("Expected output: 100, 200\n");
    return 0;
}