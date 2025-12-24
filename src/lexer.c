#include "baa.h"
#include <string.h>
#include <ctype.h>

void lexer_init(Lexer* l, const char* src) {
    l->src = src;
    l->pos = 0;
    l->len = strlen(src);
    l->line = 1;

    // FIX: Check for UTF-8 Byte Order Mark (BOM)
    // The sequence is 0xEF, 0xBB, 0xBF
    if (l->len >= 3 && 
        (unsigned char)src[0] == 0xEF && 
        (unsigned char)src[1] == 0xBB && 
        (unsigned char)src[2] == 0xBF) {
        l->pos = 3; // Skip the first 3 bytes
    }
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
    unsigned char c1 = (unsigned char)l->src[l->pos];
    unsigned char c2 = (l->pos + 1 < l->len) ? (unsigned char)l->src[l->pos + 1] : 0;
    
    printf("Lexer Error at line %d.\n", l->line);
    printf("Found byte: 0x%02X", c1);
    if (c2) printf(" 0x%02X", c2);
    printf("\nExpected 'إ' (0xD8 0xA5). You might have a typo or encoding issue.\n");
    
    exit(1);
    return token;
}