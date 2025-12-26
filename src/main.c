/**
 * @file main.c
 * @brief نقطة الدخول لمحوسب لغة "باء" (Baa Compiler).
 */

#include "baa.h"

/**
 * @brief دالة مساعدة لقراءة محتوى الملف بالكامل وتحميله في الذاكرة.
 */
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

/**
 * @brief الدالة الرئيسية التي تدير مراحل المعالجة (تحليل، إعراب، توليد، تجميع).
 */
int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: baa <file.b>\n");
        return 1;
    }

    // 1. القراءة: تحميل الكود المصدري
    char* source = read_file(argv[1]);

    // 2. التحليل اللفظي: تحويل النص إلى وحدات (Tokens)
    Lexer lexer;
    lexer_init(&lexer, source);

    // 3. التحليل القواعدي: بناء شجرة الإعراب (AST)
    Node* ast = parse(&lexer);

    // 4. توليد الكود: إنتاج ملف تجميع (Assembly)
    FILE* asm_file = fopen("out.s", "w");
    if (!asm_file) {
        perror("Failed to open out.s");
        return 1;
    }
    codegen(ast, asm_file);
    fclose(asm_file);

    // 5. التجميع والربط: استخدام GCC لإنتاج الملف التنفيذي
    int result = system("gcc out.s -o out.exe");
    
    free(source);
    
    if (result == 0) {
        printf("Build successful. Run ./out.exe\n");
        return 0;
    } else {
        printf("Build failed.\n");
        return 1;
    }
}