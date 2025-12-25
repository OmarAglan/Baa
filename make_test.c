#include <stdio.h>

void write_bytes(FILE* f, const unsigned char* bytes, size_t size) {
    fwrite(bytes, 1, size, f);
}

int main() {
    FILE* f = fopen("test.b", "wb");
    if (!f) return 1;

    // --- Helpers ---
    unsigned char decl_int[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; // صحيح 
    unsigned char var_s[] = {0xD8, 0xB3, 0x20}; // س 
    unsigned char var_s_no_sp[] = {0xD8, 0xB3}; // س
    unsigned char eq[] = {0x3D, 0x20}; // = 
    unsigned char eqeq[] = {0x3D, 0x3D, 0x20}; // == 
    unsigned char n10[] = {0xD9, 0xA1, 0xD9, 0xA0}; // ١٠
    unsigned char n20[] = {0xD9, 0xA2, 0xD9, 0xA0}; // ٢٠
    unsigned char n1[] = {0xD9, 0xA1}; // ١
    unsigned char n0[] = {0xD9, 0xA0}; // ٠
    unsigned char dot[] = {0x2E, 0x0A}; // . \n
    unsigned char if_kw[] = {0xD8, 0xA5, 0xD8, 0xB0, 0xD8, 0xA7, 0x20}; // إذا 
    unsigned char lparen[] = {0x28, 0x20}; // (
    unsigned char rparen[] = {0x29, 0x20}; // )
    unsigned char lbrace[] = {0x7B, 0x0A}; // { \n
    unsigned char rbrace[] = {0x7D, 0x0A}; // } \n
    unsigned char print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20}; // اطبع 
    unsigned char ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20}; // إرجع 

    // 1. صحيح س = ١٠.
    write_bytes(f, decl_int, sizeof(decl_int));
    write_bytes(f, var_s, sizeof(var_s));
    write_bytes(f, eq, sizeof(eq));
    write_bytes(f, n10, sizeof(n10));
    write_bytes(f, dot, sizeof(dot));

    // 2. إذا (س == ١٠) {
    write_bytes(f, if_kw, sizeof(if_kw));
    write_bytes(f, lparen, sizeof(lparen));
    write_bytes(f, var_s, sizeof(var_s)); // Variable reference has a space in our logic usually
    // Hack: Our lexer might need spacing around operators. We put spaces in helpers.
    write_bytes(f, eqeq, sizeof(eqeq));
    write_bytes(f, n10, sizeof(n10));
    write_bytes(f, rparen, sizeof(rparen));
    write_bytes(f, lbrace, sizeof(lbrace));

    //    اطبع ١.
    write_bytes(f, print, sizeof(print));
    write_bytes(f, n1, sizeof(n1));
    write_bytes(f, dot, sizeof(dot));

    // }
    write_bytes(f, rbrace, sizeof(rbrace));

    // 3. إذا (س == ٢٠) {
    write_bytes(f, if_kw, sizeof(if_kw));
    write_bytes(f, lparen, sizeof(lparen));
    write_bytes(f, var_s, sizeof(var_s));
    write_bytes(f, eqeq, sizeof(eqeq));
    write_bytes(f, n20, sizeof(n20));
    write_bytes(f, rparen, sizeof(rparen));
    write_bytes(f, lbrace, sizeof(lbrace));

    //    اطبع ٠.
    write_bytes(f, print, sizeof(print));
    write_bytes(f, n0, sizeof(n0));
    write_bytes(f, dot, sizeof(dot));

    // }
    write_bytes(f, rbrace, sizeof(rbrace));

    // 4. إرجع ٠.
    write_bytes(f, ret, sizeof(ret));
    write_bytes(f, n0, sizeof(n0));
    write_bytes(f, dot, sizeof(dot));

    fclose(f);
    return 0;
}