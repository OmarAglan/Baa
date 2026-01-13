/**
 * @file make_test.c
 * @brief Generates a test.baa file with Warnings & Diagnostics tests.
 * @version 0.2.8 Test Suite
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
    unsigned char comment[] = {0x2F, 0x2F, 0x20}; // //

    // Values
    unsigned char n0[] = {0x30}; // 0
    unsigned char n10[] = {0x31, 0x30}; // 10
    unsigned char n42[] = {0x34, 0x32}; // 42
    unsigned char n100[] = {0x31, 0x30, 0x30}; // 100

    // Keywords
    unsigned char kw_int[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; // صحيح
    unsigned char kw_print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20}; // اطبع
    unsigned char kw_ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20}; // إرجع
    unsigned char name_main[] = {0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB1, 0xD8, 0xA6, 0xD9, 0x8A, 0xD8, 0xB3, 0xD9, 0x8A, 0xD8, 0xA9}; // الرئيسية

    // Variable names
    unsigned char name_x[] = {0xD8, 0xB3}; // س (x)
    unsigned char name_y[] = {0xD8, 0xB5}; // ص (y) - unused variable
    unsigned char name_z[] = {0xD8, 0xB9}; // ع (z) - used variable
    unsigned char name_unused[] = {0xD8, 0xBA, 0xD9, 0x8A, 0xD8, 0xB1, 0x5F, 0xD9, 0x85, 0xD8, 0xB3, 0xD8, 0xAA, 0xD8, 0xAE, 0xD8, 0xAF, 0xD9, 0x85}; // غير_مستخدم

    // Comment text (ASCII for simplicity)
    unsigned char txt_unused[] = "Test: Unused variable warning";
    unsigned char txt_dead[] = "Test: Dead code warning";
    unsigned char txt_used[] = "Test: Used variable (no warning)";

    // --- Code Generation ---

    // Comment: Test Suite for v0.2.8 Warnings
    w(f, comment, sizeof(comment));
    fprintf(f, "v0.2.8 Warning System Test\n");

    // صحيح الرئيسية() {
    w(f, kw_int, sizeof(kw_int));
    w(f, name_main, sizeof(name_main));
    w(f, lp, sizeof(lp));
    w(f, lb, sizeof(lb));

    // --- Test 1: Unused Variable Warning ---
    // // Test: Unused variable warning
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, comment, sizeof(comment));
    fwrite(txt_unused, 1, sizeof(txt_unused)-1, f);
    w(f, nl, 1);

    // صحيح ص = 42.  (unused - should trigger warning with -Wall)
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_int, sizeof(kw_int));
    w(f, name_y, sizeof(name_y));
    w(f, sp, 1);
    w(f, eq, sizeof(eq));
    w(f, n42, sizeof(n42));
    w(f, dot, sizeof(dot));

    // --- Test 2: Used Variable (No Warning) ---
    // // Test: Used variable (no warning)
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, comment, sizeof(comment));
    fwrite(txt_used, 1, sizeof(txt_used)-1, f);
    w(f, nl, 1);

    // صحيح ع = 100.
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_int, sizeof(kw_int));
    w(f, name_z, sizeof(name_z));
    w(f, sp, 1);
    w(f, eq, sizeof(eq));
    w(f, n100, sizeof(n100));
    w(f, dot, sizeof(dot));

    // اطبع ع.  (this uses ع)
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_print, sizeof(kw_print));
    w(f, name_z, sizeof(name_z));
    w(f, dot, sizeof(dot));

    // --- Test 3: Dead Code Warning ---
    // // Test: Dead code warning
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, comment, sizeof(comment));
    fwrite(txt_dead, 1, sizeof(txt_dead)-1, f);
    w(f, nl, 1);

    // إرجع 0.
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_ret, sizeof(kw_ret));
    w(f, n0, 1);
    w(f, dot, sizeof(dot));

    // صحيح س = 10.  (dead code - after return)
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_int, sizeof(kw_int));
    w(f, name_x, sizeof(name_x));
    w(f, sp, 1);
    w(f, eq, sizeof(eq));
    w(f, n10, sizeof(n10));
    w(f, dot, sizeof(dot));

    // اطبع س.  (also dead code)
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_print, sizeof(kw_print));
    w(f, name_x, sizeof(name_x));
    w(f, dot, sizeof(dot));

    w(f, rb, sizeof(rb)); // }

    fclose(f);
    
    printf("Generated test.baa with Warning System tests (v0.2.8).\n");
    printf("\n");
    printf("Expected behavior:\n");
    printf("  - Without -Wall: No warnings, compiles and runs normally.\n");
    printf("  - With -Wall: Shows warnings for:\n");
    printf("      1. Unused variable 'ص' (y)\n");
    printf("      2. Dead code after 'إرجع' (return)\n");
    printf("  - With -Wall -Werror: Compilation fails due to warnings.\n");
    printf("\n");
    printf("Test commands:\n");
    printf("  baa test.baa                    (no warnings)\n");
    printf("  baa -Wall test.baa              (show warnings)\n");
    printf("  baa -Wall -Werror test.baa      (fail on warnings)\n");
    printf("  baa -Wunused-variable test.baa  (only unused var warning)\n");
    printf("\n");
    printf("Expected output (when run): 100\n");
    
    return 0;
}