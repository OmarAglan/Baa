/**
 * @file make_test.c
 * @brief Generates a test.baa file with v0.2.9 (Input & UX Polish) tests.
 * @version 0.2.9 Test Suite
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
    unsigned char n1[] = {0x31}; // 1
    unsigned char n10[] = {0x31, 0x30}; // 10

    // Keywords
    unsigned char kw_int[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; // صحيح
    unsigned char kw_bool[] = {0xD9, 0x85, 0xD9, 0x86, 0xD8, 0xB7, 0xD9, 0x82, 0xD9, 0x8A, 0x20}; // منطقي
    unsigned char kw_print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20}; // اطبع
    unsigned char kw_read[] = {0xD8, 0xA7, 0xD9, 0x82, 0xD8, 0xB1, 0xD8, 0xA3, 0x20}; // اقرأ
    unsigned char kw_ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20}; // إرجع
    unsigned char kw_if[] = {0xD8, 0xA5, 0xD8, 0xB0, 0xD8, 0xA7, 0x20}; // إذا
    unsigned char kw_else[] = {0xD9, 0x88, 0xD8, 0xA5, 0xD9, 0x84, 0xD8, 0xA7, 0x20}; // وإلا
    unsigned char name_main[] = {0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB1, 0xD8, 0xA6, 0xD9, 0x8A, 0xD8, 0xB3, 0xD9, 0x8A, 0xD8, 0xA9}; // الرئيسية

    // Boolean literals
    unsigned char kw_true[] = {0xD8, 0xB5, 0xD9, 0x88, 0xD8, 0xA7, 0xD8, 0xA8}; // صواب
    unsigned char kw_false[] = {0xD8, 0xAE, 0xD8, 0xB7, 0xD8, 0xA3}; // خطأ

    // Variable names
    unsigned char name_x[] = {0xD8, 0xB3}; // س (x)
    unsigned char name_b[] = {0xD8, 0xA8}; // ب (b - for bool)
    unsigned char name_result[] = {0xD9, 0x86, 0xD8, 0xAA, 0xD9, 0x8A, 0xD8, 0xAC, 0xD8, 0xA9}; // نتيجة

    // Comment texts
    unsigned char txt_bool_test[] = "Test: Boolean type and literals";
    unsigned char txt_input_test[] = "Test: Input statement (read)";
    unsigned char txt_cond_test[] = "Test: Boolean in conditions";

    // --- Code Generation ---

    // Comment: Test Suite for v0.2.9
    w(f, comment, sizeof(comment));
    fprintf(f, "v0.2.9 Input & Boolean Test Suite\n\n");

    // صحيح الرئيسية() {
    w(f, kw_int, sizeof(kw_int));
    w(f, name_main, sizeof(name_main));
    w(f, lp, sizeof(lp));
    w(f, lb, sizeof(lb));

    // --- Test 1: Boolean Type and Literals ---
    // // Test: Boolean type and literals
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, comment, sizeof(comment));
    fwrite(txt_bool_test, 1, sizeof(txt_bool_test)-1, f);
    w(f, nl, 1);

    // منطقي ب = صواب.
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_bool, sizeof(kw_bool));
    w(f, name_b, sizeof(name_b));
    w(f, sp, 1);
    w(f, eq, sizeof(eq));
    w(f, kw_true, sizeof(kw_true));
    w(f, dot, sizeof(dot));

    // --- Test 2: Boolean in conditions ---
    // // Test: Boolean in conditions
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, comment, sizeof(comment));
    fwrite(txt_cond_test, 1, sizeof(txt_cond_test)-1, f);
    w(f, nl, 1);

    // إذا (ب) {
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_if, sizeof(kw_if));
    fprintf(f, "(");
    w(f, name_b, sizeof(name_b));
    fprintf(f, ") ");
    w(f, lb, sizeof(lb));

    //     اطبع 1.
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_print, sizeof(kw_print));
    w(f, n1, sizeof(n1));
    w(f, dot, sizeof(dot));

    // }
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, rb, sizeof(rb));
    w(f, nl, 1);

    // --- Test 3: Input Statement (commented out for automated testing) ---
    // // Test: Input statement (read) - uncomment to test interactively
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, comment, sizeof(comment));
    fwrite(txt_input_test, 1, sizeof(txt_input_test)-1, f);
    w(f, nl, 1);

    // صحيح س = 0.
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_int, sizeof(kw_int));
    w(f, name_x, sizeof(name_x));
    w(f, sp, 1);
    w(f, eq, sizeof(eq));
    w(f, n0, sizeof(n0));
    w(f, dot, sizeof(dot));

    // // اقرأ س.  (uncomment this line to test input)
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, comment, sizeof(comment));
    w(f, kw_read, sizeof(kw_read));
    w(f, name_x, sizeof(name_x));
    w(f, dot, sizeof(dot));

    // اطبع س.
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_print, sizeof(kw_print));
    w(f, name_x, sizeof(name_x));
    w(f, dot, sizeof(dot));

    // --- Test 4: Boolean false ---
    // منطقي نتيجة = خطأ.
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_bool, sizeof(kw_bool));
    w(f, name_result, sizeof(name_result));
    w(f, sp, 1);
    w(f, eq, sizeof(eq));
    w(f, kw_false, sizeof(kw_false));
    w(f, dot, sizeof(dot));

    // إذا (نتيجة) {
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_if, sizeof(kw_if));
    fprintf(f, "(");
    w(f, name_result, sizeof(name_result));
    fprintf(f, ") ");
    w(f, lb, sizeof(lb));

    //     اطبع 10.
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_print, sizeof(kw_print));
    w(f, n10, sizeof(n10));
    w(f, dot, sizeof(dot));

    // } وإلا {
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    fprintf(f, "} ");
    w(f, kw_else, sizeof(kw_else));
    w(f, lb, sizeof(lb));

    //     اطبع 0.
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_print, sizeof(kw_print));
    w(f, n0, sizeof(n0));
    w(f, dot, sizeof(dot));

    // }
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, rb, sizeof(rb));

    // إرجع 0.
    w(f, sp, 1); w(f, sp, 1); w(f, sp, 1); w(f, sp, 1);
    w(f, kw_ret, sizeof(kw_ret));
    w(f, n0, sizeof(n0));
    w(f, dot, sizeof(dot));

    w(f, rb, sizeof(rb)); // }

    fclose(f);
    
    printf("Generated test.baa with v0.2.9 tests (Input & Boolean).\n");
    printf("\n");
    printf("Features tested:\n");
    printf("  1. Boolean type (منطقي) with literals (صواب/خطأ)\n");
    printf("  2. Boolean variables in conditions\n");
    printf("  3. Input statement (اقرأ) - commented out for automated testing\n");
    printf("  4. Compile timing with -v flag\n");
    printf("\n");
    printf("Test commands:\n");
    printf("  baa test.baa -o test.exe         (compile)\n");
    printf("  baa -v test.baa -o test.exe      (compile with timing)\n");
    printf("  test.exe                          (run - should output: 1, 0, 0)\n");
    printf("\n");
    printf("Expected output:\n");
    printf("  1\n");
    printf("  0\n");
    printf("  0\n");
    printf("\n");
    printf("To test input (اقرأ):\n");
    printf("  1. Edit test.baa and uncomment the اقرأ line\n");
    printf("  2. Recompile and run, then enter a number when prompted\n");
    
    return 0;
}