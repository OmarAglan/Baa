/**
 * @file make_test.c
 * @brief Generates a test.baa file with Preprocessor directives including Undefine.
 * @version 0.2.6 Test Suite
 */

#include <stdio.h>

void w(FILE* f, const unsigned char* b, size_t s) { fwrite(b, 1, s, f); }

int main() {
    FILE* f = fopen("test.baa", "wb");
    if (!f) return 1;

    // --- UTF-8 Arabic Fragments ---
    
    // Directives
    unsigned char hash[] = {0x23}; // #
    unsigned char sp[] = {0x20};   // space
    unsigned char nl[] = {0x0A};   // newline
    
    // Keywords
    unsigned char kw_def[] = {0xD8, 0xAA, 0xD8, 0xB9, 0xD8, 0xB1, 0xD9, 0x8A, 0xD9, 0x81, 0x20}; // تعريف 
    unsigned char kw_ifdef[] = {0xD8, 0xA5, 0xD8, 0xB0, 0xD8, 0xA7, 0x5F, 0xD8, 0xB9, 0xD8, 0xB1, 0xD9, 0x81, 0x20}; // إذا_عرف 
    unsigned char kw_else[] = {0xD9, 0x88, 0xD8, 0xA5, 0xD9, 0x84, 0xD8, 0xA7, 0x0A}; // وإلا 
    unsigned char kw_end[] = {0xD9, 0x86, 0xD9, 0x87, 0xD8, 0xA7, 0xD9, 0x8A, 0xD8, 0xA9, 0x0A}; // نهاية 
    
    // #الغاء_تعريف (Undefine)
    unsigned char kw_undef[] = {
        0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xBA, 0xD8, 0xA7, 0xD8, 0xA1, // الغاء
        0x5F, // _
        0xD8, 0xAA, 0xD8, 0xB9, 0xD8, 0xB1, 0xD9, 0x8A, 0xD9, 0x81, // تعريف
        0x20 // space
    };

    // Names
    unsigned char name_val[] = {0xD9, 0x82, 0xD9, 0x8A, 0xD9, 0x85, 0xD8, 0xA9}; // قيمة (Value)
    unsigned char name_x[] = {0xD8, 0xB3, 0x20}; // س (Variable x)
    
    // Values
    unsigned char val_100[] = {0x31, 0x30, 0x30, 0x0A}; // 100\n
    unsigned char n0[] = {0x30}; // 0
    unsigned char n1[] = {0x31}; // 1
    unsigned char n2[] = {0x32}; // 2
    
    // Language Keywords
    unsigned char kw_int[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; // صحيح
    unsigned char kw_print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20}; // اطبع
    unsigned char kw_ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20}; // إرجع
    unsigned char name_main[] = {0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB1, 0xD8, 0xA6, 0xD9, 0x8A, 0xD8, 0xB3, 0xD9, 0x8A, 0xD8, 0xA9}; // الرئيسية

    // Symbols
    unsigned char eq[] = {0x3D, 0x20}; // = 
    unsigned char dot[] = {0x2E, 0x0A}; // . \n
    unsigned char lp[] = {0x28, 0x20}; // ( 
    unsigned char rp[] = {0x29, 0x20}; // ) 
    unsigned char lb[] = {0x7B, 0x0A}; // { \n
    unsigned char rb[] = {0x7D, 0x0A}; // } \n

    // --- Code Generation ---

    // 1. #تعريف قيمة 100
    w(f, hash, 1); w(f, kw_def, sizeof(kw_def)); w(f, name_val, sizeof(name_val)); w(f, sp, 1); w(f, val_100, sizeof(val_100));

    // صحيح الرئيسية() {
    w(f, kw_int, sizeof(kw_int)); w(f, name_main, sizeof(name_main)); w(f, lp, sizeof(lp)); w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));

        // 2. Variable Substitution Test
        // صحيح س = قيمة. -> صحيح س = 100.
        w(f, kw_int, sizeof(kw_int)); w(f, name_x, sizeof(name_x)); w(f, eq, sizeof(eq)); w(f, name_val, sizeof(name_val)); w(f, dot, sizeof(dot));
        w(f, kw_print, sizeof(kw_print)); w(f, name_x, sizeof(name_x)); w(f, dot, sizeof(dot));

        // 3. Positive Conditional Test
        // #إذا_عرف قيمة
        //   اطبع 1.
        // #نهاية
        w(f, hash, 1); w(f, kw_ifdef, sizeof(kw_ifdef)); w(f, name_val, sizeof(name_val)); w(f, nl, 1);
        w(f, kw_print, sizeof(kw_print)); w(f, n1, 1); w(f, dot, sizeof(dot));
        w(f, hash, 1); w(f, kw_end, sizeof(kw_end));

        // 4. UNDEFINE Test
        // #الغاء_تعريف قيمة
        w(f, hash, 1); w(f, kw_undef, sizeof(kw_undef)); w(f, name_val, sizeof(name_val)); w(f, nl, 1);

        // 5. Negative Conditional Test (after undefine)
        // #إذا_عرف قيمة
        //   اطبع 0. (Should be skipped)
        // #وإلا
        //   اطبع 2. (Should be executed)
        // #نهاية
        w(f, hash, 1); w(f, kw_ifdef, sizeof(kw_ifdef)); w(f, name_val, sizeof(name_val)); w(f, nl, 1);
        w(f, kw_print, sizeof(kw_print)); w(f, n0, 1); w(f, dot, sizeof(dot));
        w(f, hash, 1); w(f, kw_else, sizeof(kw_else));
        w(f, kw_print, sizeof(kw_print)); w(f, n2, 1); w(f, dot, sizeof(dot));
        w(f, hash, 1); w(f, kw_end, sizeof(kw_end));

        // إرجع ٠.
        w(f, kw_ret, sizeof(kw_ret)); w(f, n0, 1); w(f, dot, sizeof(dot));

    w(f, rb, sizeof(rb)); // }

    fclose(f);
    printf("Generated test.baa with #undef test.\n");
    return 0;
}