#include <stdio.h>

int main() {
    FILE* f = fopen("test.b", "wb");
    if (!f) return 1;

    // "إرجع"
    unsigned char ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20};
    
    // "٤٠" (Arabic 40)
    // 4 = 0xD9 0xA4, 0 = 0xD9 0xA0
    unsigned char n1[] = {0xD9, 0xA4, 0xD9, 0xA0, 0x20}; 
    
    // "+"
    unsigned char plus[] = {0x2B, 0x20};

    // "٢" (Arabic 2)
    // 2 = 0xD9 0xA2
    unsigned char n2[] = {0xD9, 0xA2};

    // "."
    unsigned char dot[] = {0x2E};

    fwrite(ret, 1, sizeof(ret), f);
    fwrite(n1, 1, sizeof(n1), f);
    fwrite(plus, 1, sizeof(plus), f);
    fwrite(n2, 1, sizeof(n2), f);
    fwrite(dot, 1, sizeof(dot), f);
    
    fclose(f);
    return 0;
}