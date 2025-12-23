#include "baa.h"
#include <string.h>
#include <ctype.h>

void lexer_init(Lexer* l, const char* src) {
    l->src = src;
    l->pos = 0;
    l->len = strlen(src);
    l->line = 1;
}

Token lexer_next_token(Lexer* l) {
    Token token = {0};
    
    // Skip whitespace
    while (l->pos < l->len && isspace(l->src[l->pos])) {
        if (l->src[l->pos] == '\n') l->line++;
        l->pos++;
    }

    if (l->pos >= l->len) {
        token.type = TOKEN_EOF;
        return token;
    }

    const char* current = l->src + l->pos;

    // Check for Dot (.)
    if (*current == '.') {
        token.type = TOKEN_DOT;
        l->pos++;
        return token;
    }

    // Check for Integers (0-9) - Simplified: No Arabic numerals yet
    if (isdigit(*current)) {
        token.type = TOKEN_INT;
        size_t start = l->pos;
        while (l->pos < l->len && isdigit(l->src[l->pos])) {
            l->pos++;
        }
        // Copy value
        size_t len = l->pos - start;
        token.value = malloc(len + 1);
        strncpy(token.value, l->src + start, len);
        token.value[len] = '\0';
        return token;
    }

    // Check for Keywords (UTF-8 handling)
    // "إرجع" takes 8 bytes in UTF-8
    if (strncmp(current, "إرجع", 8) == 0) {
        token.type = TOKEN_RETURN;
        l->pos += 8;
        return token;
    }

    token.type = TOKEN_INVALID;
    printf("Lexer Error: Unknown char at line %d\n", l->line);
    exit(1);
    return token;
}