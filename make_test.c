#include <stdio.h>

int main() {
    FILE* f = fopen("test.b", "wb");
    if (!f) return 1;

    // Line 1: اطبع ١٠٠ + ٢٣.
    // "اطبع" (D8 A7 D8 B7 D8 A8 D8 B9)
    unsigned char print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20};
    // "١٠٠" (100)
    unsigned char n100[] = {0xD9, 0xA1, 0xD9, 0xA0, 0xD9, 0xA0, 0x20};
    // "+"
    unsigned char plus[] = {0x2B, 0x20};
    // "٢٣" (23)
    unsigned char n23[] = {0xD9, 0xA2, 0xD9, 0xA3};
    // "." + Newline
    unsigned char dot_nl[] = {0x2E, 0x0A};

    // Line 2: إرجع ٠.
    // "إرجع"
    unsigned char ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20};
    // "٠"
    unsigned char n0[] = {0xD9, 0xA0};

    fwrite(print, 1, sizeof(print), f);
    fwrite(n100, 1, sizeof(n100), f);
    fwrite(plus, 1, sizeof(plus), f);
    fwrite(n23, 1, sizeof(n23), f);
    fwrite(dot_nl, 1, sizeof(dot_nl), f);
    
    fwrite(ret, 1, sizeof(ret), f);
    fwrite(n0, 1, sizeof(n0), f);
    fwrite(dot_nl, 1, 1, f); // Just the dot

    fclose(f);
    return 0;
}