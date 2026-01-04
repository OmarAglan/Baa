#include <stdio.h>
#include <stdlib.h>
// Use standard library just to write files

int main() {
    // 1. Create math.b (The library)
    // Contains the definition of 'add' and 'mult'
    FILE* f_math = fopen("math.b", "w");
    if (!f_math) return 1;
    
    // صحيح جمع(صحيح أ, صحيح ب) { إرجع أ + ب. }
    fprintf(f_math, "صحيح جمع(صحيح أ, صحيح ب) {\n");
    fprintf(f_math, "    إرجع أ + ب.\n");
    fprintf(f_math, "}\n\n");
    
    // صحيح ضرب(صحيح س, صحيح ص) { إرجع س * ص. }
    fprintf(f_math, "صحيح ضرب(صحيح س, صحيح ص) {\n");
    fprintf(f_math, "    إرجع س * ص.\n");
    fprintf(f_math, "}\n");
    fclose(f_math);
    printf("Created math.b\n");

    // 2. Create main.b (The consumer)
    // Uses prototypes to call functions from math.b
    FILE* f_main = fopen("main.b", "w");
    if (!f_main) return 1;

    // Prototypes:
    // صحيح جمع(صحيح أ, صحيح ب).
    fprintf(f_main, "صحيح جمع(صحيح أ, صحيح ب).\n");
    // صحيح ضرب(صحيح س, صحيح ص).
    fprintf(f_main, "صحيح ضرب(صحيح س, صحيح ص).\n\n");

    // Main function
    fprintf(f_main, "صحيح الرئيسية() {\n");
    fprintf(f_main, "    صحيح س = ١٠.\n");
    fprintf(f_main, "    صحيح ص = ٢٠.\n");
    fprintf(f_main, "    صحيح المجموع = جمع(س, ص).\n"); // Should be 30
    fprintf(f_main, "    صحيح الناتج = ضرب(المجموع, ٢).\n"); // Should be 60
    fprintf(f_main, "    اطبع المجموع.\n");
    fprintf(f_main, "    اطبع الناتج.\n");
    fprintf(f_main, "    إرجع ٠.\n");
    fprintf(f_main, "}\n");
    fclose(f_main);
    printf("Created main.b\n");

    return 0;
}
