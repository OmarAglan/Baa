#include <stdio.h>

int main() {
    FILE* f = fopen("test.b", "wb");
    if (!f) return 1;

    // Line 1: رقم س = ٥٠.
    // "رقم" (D8 B1 D9 82 D9 85)
    unsigned char decl[] = {0xD8, 0xB1, 0xD9, 0x82, 0xD9, 0x85, 0x20};
    // "س" (D8 B3) - Variable name
    unsigned char var[] = {0xD8, 0xB3, 0x20};
    // "=" (3D)
    unsigned char assign[] = {0x3D, 0x20};
    // "٥٠" (50)
    unsigned char n50[] = {0xD9, 0xA5, 0xD9, 0xA0};
    // "." + NL
    unsigned char end[] = {0x2E, 0x0A};

    // Line 2: اطبع س + ٥.
    unsigned char print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20};
    unsigned char var_usage[] = {0xD8, 0xB3, 0x20}; // "س"
    unsigned char plus[] = {0x2B, 0x20};
    unsigned char n5[] = {0xD9, 0xA5};

    // Line 3: إرجع ٠.
    unsigned char ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20};
    unsigned char n0[] = {0xD9, 0xA0};

    // Write Line 1
    fwrite(decl, 1, sizeof(decl), f);
    fwrite(var, 1, sizeof(var), f);
    fwrite(assign, 1, sizeof(assign), f);
    fwrite(n50, 1, sizeof(n50), f);
    fwrite(end, 1, sizeof(end), f);

    // Write Line 2
    fwrite(print, 1, sizeof(print), f);
    fwrite(var_usage, 1, sizeof(var_usage), f);
    fwrite(plus, 1, sizeof(plus), f);
    fwrite(n5, 1, sizeof(n5), f);
    fwrite(end, 1, sizeof(end), f);

    // Write Line 3
    fwrite(ret, 1, sizeof(ret), f);
    fwrite(n0, 1, sizeof(n0), f);
    fwrite(end, 1, 1, f); // Dot

    fclose(f);
    return 0;
}