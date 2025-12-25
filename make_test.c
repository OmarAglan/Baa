#include <stdio.h>

void write_bytes(FILE* f, const unsigned char* bytes, size_t size) {
    fwrite(bytes, 1, size, f);
}

int main() {
    FILE* f = fopen("test.b", "wb");
    if (!f) return 1;

    // --- UTF-8 Tokens ---
    unsigned char int_kw[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; // صحيح 
    unsigned char return_kw[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20}; // إرجع 
    unsigned char print_kw[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20}; // اطبع 
    
    // Identifiers
    unsigned char global_var[] = {0xD8, 0xB9, 0xD8, 0xA7, 0xD9, 0x85, 0xD9, 0x84, 0x20}; // عامل (factor)
    unsigned char func_name[] = {0xD8, 0xB6, 0xD8, 0xA7, 0xD8, 0xB9, 0xD9, 0x81}; // ضاعف (double) NO SPACE
    unsigned char main_name[] = {0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB1, 0xD8, 0xA6, 0xD9, 0x8A, 0xD8, 0xB3, 0xD9, 0x8A, 0xD8, 0xA9}; // الرئيسية (main)
    unsigned char var_s[] = {0xD8, 0xB3}; // س (param)
    unsigned char var_n[] = {0xD9, 0x86, 0x20}; // ن (local 1)
    unsigned char var_m[] = {0xD9, 0x85, 0x20}; // م (local 2)

    // Symbols / Literals
    unsigned char eq[] = {0x3D, 0x20}; 
    unsigned char plus[] = {0x2B, 0x20}; 
    unsigned char lp[] = {0x28, 0x20}; 
    unsigned char rp[] = {0x29, 0x20}; 
    unsigned char lb[] = {0x7B, 0x0A}; 
    unsigned char rb[] = {0x7D, 0x0A}; 
    unsigned char dot[] = {0x2E, 0x0A};
    unsigned char sp[] = {0x20};
    
    unsigned char n2[] = {0xD9, 0xA2};
    unsigned char n10[] = {0xD9, 0xA1, 0xD9, 0xA0};
    unsigned char n0[] = {0xD9, 0xA0};

    // --- Code Generation ---

    // 1. Global: صحيح عامل = ٢.
    write_bytes(f, int_kw, sizeof(int_kw));
    write_bytes(f, global_var, sizeof(global_var));
    write_bytes(f, eq, sizeof(eq));
    write_bytes(f, n2, sizeof(n2));
    write_bytes(f, dot, sizeof(dot));

    // 2. Func: صحيح ضاعف(صحيح س) {
    write_bytes(f, int_kw, sizeof(int_kw));
    write_bytes(f, func_name, sizeof(func_name));
    write_bytes(f, sp, 1);
    write_bytes(f, lp, sizeof(lp));
    write_bytes(f, int_kw, sizeof(int_kw));
    write_bytes(f, var_s, sizeof(var_s));
    write_bytes(f, rp, sizeof(rp));
    write_bytes(f, lb, sizeof(lb));

    //    إرجع س + س.
    write_bytes(f, return_kw, sizeof(return_kw));
    write_bytes(f, var_s, sizeof(var_s));
    write_bytes(f, sp, 1);
    write_bytes(f, plus, sizeof(plus));
    write_bytes(f, var_s, sizeof(var_s));
    write_bytes(f, dot, sizeof(dot));

    // }
    write_bytes(f, rb, sizeof(rb));

    // 3. Main: صحيح الرئيسية() {
    write_bytes(f, int_kw, sizeof(int_kw));
    write_bytes(f, main_name, sizeof(main_name));
    write_bytes(f, sp, 1);
    write_bytes(f, lp, sizeof(lp));
    write_bytes(f, rp, sizeof(rp));
    write_bytes(f, lb, sizeof(lb));

    //    صحيح ن = ١٠.
    write_bytes(f, int_kw, sizeof(int_kw));
    write_bytes(f, var_n, sizeof(var_n));
    write_bytes(f, eq, sizeof(eq));
    write_bytes(f, n10, sizeof(n10));
    write_bytes(f, dot, sizeof(dot));

    //    صحيح م = ضاعف(ن).
    write_bytes(f, int_kw, sizeof(int_kw));
    write_bytes(f, var_m, sizeof(var_m));
    write_bytes(f, eq, sizeof(eq));
    write_bytes(f, func_name, sizeof(func_name));
    write_bytes(f, sp, 1);
    write_bytes(f, lp, sizeof(lp));
    write_bytes(f, var_n, sizeof(var_n));
    write_bytes(f, rp, sizeof(rp));
    write_bytes(f, dot, sizeof(dot));

    //    اطبع م + عامل.
    write_bytes(f, print_kw, sizeof(print_kw));
    write_bytes(f, var_m, sizeof(var_m));
    write_bytes(f, plus, sizeof(plus));
    write_bytes(f, global_var, sizeof(global_var));
    write_bytes(f, dot, sizeof(dot));

    //    إرجع ٠.
    write_bytes(f, return_kw, sizeof(return_kw));
    write_bytes(f, n0, sizeof(n0));
    write_bytes(f, dot, sizeof(dot));

    // }
    write_bytes(f, rb, sizeof(rb));

    fclose(f);
    printf("Created test.b with Function Logic.\n");
    return 0;
}