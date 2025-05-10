#include "baa/lexer/lexer.h"
// #include "baa/parser/parser.h" // Removed as parser is being removed
#include "baa/codegen/codegen.h"
#include "baa/utils/utils.h"
#include "baa/preprocessor/preprocessor.h"
#include "baa/compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

/**
 * @brief Print usage information.
 *
 * This function prints the correct usage of the program to the console.
 *
 * @param argv The argument vector containing the program name and arguments.
 */
static void print_usage(const char *program_name) {
    printf("الاستخدام خاطئ\n");
    printf("الرجاء استخدام الأمر بالشكل الصحيح\n");
    printf("%s \n<المسار_إلي_الملف_المراد_تجميعه>", program_name);
}

/**
 * @brief The main entry point of the program.
 *
 * This function checks the number of command-line arguments and processes the input file.
 *
 * @param argc The argument count.
 * @param argv The argument vector.
 * @return An integer representing the exit status of the program.
 */
int main(int argc, char **argv) {
    setlocale(LC_ALL, "");  // Set locale for proper wide char handling

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // Call the centralized compilation function
    int result = compile_baa_file(argv[1]);

    return result;
}
