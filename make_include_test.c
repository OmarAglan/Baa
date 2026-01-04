#include <stdio.h>
#include <stdlib.h>

void write_file(const char* filename, const char* content) {
    FILE* f = fopen(filename, "w");
    if (!f) { printf("Error creating %s\n", filename); exit(1); }
    fprintf(f, "%s", content);
    fclose(f);
    printf("Created %s\n", filename);
}

int main() {
    // 1. Header with prototype
    write_file("math.baahd", 
        "صحيح جمع(صحيح أ, صحيح ب).\n"
    );

    // 2. Implementation
    write_file("math.baa", 
        "صحيح جمع(صحيح أ, صحيح ب) {\n"
        "    إرجع أ + ب.\n"
        "}\n"
    );

    // 3. Main file using include
    write_file("main.baa", 
        "#تضمين \"math.baahd\"\n"
        "\n"
        "صحيح الرئيسية() {\n"
        "    صحيح س = جمع(١٠, ٢٠).\n"
        "    اطبع س.\n"
        "    إرجع ٠.\n"
        "}\n"
    );

    return 0;
}
