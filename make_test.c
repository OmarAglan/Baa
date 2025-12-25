#include <stdio.h>

void write_bytes(FILE* f, const unsigned char* bytes, size_t size) {
    fwrite(bytes, 1, size, f);
}

int main() {
    FILE* f = fopen("test.b", "wb");
    if (!f) return 1;

    // --- Definitions ---
    // Keywords
    unsigned char decl_int[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; // صحيح 
    unsigned char while_kw[] = {0xD8, 0xB7, 0xD8, 0xA7, 0xD9, 0x84, 0xD9, 0x85, 0xD8, 0xA7, 0x20}; // طالما 
    unsigned char print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20}; // اطبع 
    unsigned char ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20}; // إرجع 

    // Vars/Nums
    unsigned char var_s[] = {0xD8, 0xB3, 0x20}; // س 
    unsigned char n5[] = {0xD9, 0xA5}; // ٥
    unsigned char n1[] = {0xD9, 0xA1}; // ١
    unsigned char n0[] = {0xD9, 0xA0}; // ٠

    // Symbols
    unsigned char eq[] = {0x3D, 0x20}; // = 
    unsigned char neq[] = {0x21, 0x3D, 0x20}; // != 
    unsigned char sub[] = {0x2D, 0x20}; // - 
    unsigned char dot[] = {0x2E, 0x0A}; // . \n
    unsigned char lparen[] = {0x28, 0x20}; // (
    unsigned char rparen[] = {0x29, 0x20}; // )
    unsigned char lbrace[] = {0x7B, 0x0A}; // { \n
    unsigned char rbrace[] = {0x7D, 0x0A}; // } \n

    // --- Code Generation ---

    // 1. صحيح س = ٥.
    write_bytes(f, decl_int, sizeof(decl_int));
    write_bytes(f, var_s, sizeof(var_s));
    write_bytes(f, eq, sizeof(eq));
    write_bytes(f, n5, sizeof(n5));
    write_bytes(f, dot, sizeof(dot));

    // 2. طالما (س != ٠) {
    write_bytes(f, while_kw, sizeof(while_kw));
    write_bytes(f, lparen, sizeof(lparen));
    write_bytes(f, var_s, sizeof(var_s));
    write_bytes(f, neq, sizeof(neq));
    write_bytes(f, n0, sizeof(n0));
    write_bytes(f, rparen, sizeof(rparen));
    write_bytes(f, lbrace, sizeof(lbrace));

    //    اطبع س.
    write_bytes(f, print, sizeof(print));
    write_bytes(f, var_s, sizeof(var_s));
    write_bytes(f, dot, sizeof(dot));

    //    س = س - ١. (Re-assignment logic - Wait, we need to implement assignment to existing vars!)
    //    Current parser handles NODE_VAR_DECL (int x = 5) but not simple assignment (x = 5).
    //    Let's hack it using declaration shadowing for now to test the loop structure, 
    //    OR quickly add assignment node. 
    
    //    Let's pause and add Assignment Node in Parser/Codegen quickly below before running this test.
    //    Actually, let's just create an infinite loop printing 5 once to test the structure first.
    //    No wait, infinite loops are annoying to test.
    
    //    Okay, I will include the Assignment Logic in this step implicitly because loops are useless without it.
    
    //    س = س - ١.
    write_bytes(f, var_s, sizeof(var_s)); // Identifier
    write_bytes(f, eq, sizeof(eq));       // =
    write_bytes(f, var_s, sizeof(var_s)); // s
    write_bytes(f, sub, sizeof(sub));     // -
    write_bytes(f, n1, sizeof(n1));       // 1
    write_bytes(f, dot, sizeof(dot));

    // }
    write_bytes(f, rbrace, sizeof(rbrace));

    // 3. إرجع ٠.
    write_bytes(f, ret, sizeof(ret));
    write_bytes(f, n0, sizeof(n0));
    write_bytes(f, dot, sizeof(dot));

    fclose(f);
    return 0;
}