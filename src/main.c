#include "baa.h"
#include <string.h>

char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("Could not open file %s\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buffer = malloc(length + 1);
    fread(buffer, 1, length, f);
    buffer[length] = '\0';
    fclose(f);
    return buffer;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: baa <file.b>\n");
        return 1;
    }

    // 1. Read
    char* source = read_file(argv[1]);

    // 2. Init Lexer
    Lexer lexer;
    lexer_init(&lexer, source);

    // 3. Parse
    Node* ast = parse(&lexer);

    // 4. Codegen (Generate Assembly)
    // We output to 'out.s' (Assembly file)
    FILE* asm_file = fopen("out.s", "w");
    if (!asm_file) {
        perror("Failed to open out.s");
        return 1;
    }
    codegen(ast, asm_file);
    fclose(asm_file);

    // 5. Compile Assembly to Exe using GCC
    // This command tells GCC to take the assembly file and make an executable
    int result = system("gcc out.s -o out.exe");
    
    if (result == 0) {
        printf("Compilation successful. Created out.exe\n");
    } else {
        printf("Assembler error.\n");
    }
    
    free(source);
    // Cleanup AST would go here
    return 0;
}