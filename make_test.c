#include <stdio.h>

int main() {
    FILE* f = fopen("test.b", "wb");
    if (!f) return 1;

    // Line 1: // comment
    // "//" = 2F 2F, space, "comment" (ASCII)
    unsigned char comment[] = {0x2F, 0x2F, 0x20, 0x63, 0x6F, 0x6D, 0x6D, 0x65, 0x6E, 0x74, 0x0A};

    // Line 2: صحيح س = ٤٢.
    // "صحيح" (D8 B5 D8 AD D9 8A D8 AD)
    unsigned char type[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20};
    
    // "س"
    unsigned char var[] = {0xD8, 0xB3, 0x20};
    // "="
    unsigned char assign[] = {0x3D, 0x20};
    // "٤٢"
    unsigned char n42[] = {0xD9, 0xA4, 0xD9, 0xA2};
    // "." + NL
    unsigned char end[] = {0x2E, 0x0A};

    // Line 3: اطبع س.
    unsigned char print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20};
    unsigned char var_usage[] = {0xD8, 0xB3}; // "س" NO space after

    // Line 4: إرجع ٠.
    unsigned char ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20};
    unsigned char n0[] = {0xD9, 0xA0};

    // Write
    fwrite(comment, 1, sizeof(comment), f);
    
    fwrite(type, 1, sizeof(type), f);
    fwrite(var, 1, sizeof(var), f);
    fwrite(assign, 1, sizeof(assign), f);
    fwrite(n42, 1, sizeof(n42), f);
    fwrite(end, 1, sizeof(end), f);

    fwrite(print, 1, sizeof(print), f);
    fwrite(var_usage, 1, sizeof(var_usage), f);
    fwrite(end, 1, sizeof(end), f);

    fwrite(ret, 1, sizeof(ret), f);
    fwrite(n0, 1, sizeof(n0), f);
    fwrite(end, 1, 1, f); // Just dot

    fclose(f);
    return 0;
}