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
        l->pos = 3;
    }
}

// Helper to check for Arabic Digit (UTF-8: 0xD9 0xA0 to 0xD9 0xA9)
int is_arabic_digit(const char* c) {
    unsigned char b1 = (unsigned char)c[0];
    unsigned char b2 = (unsigned char)c[1];
    return (b1 == 0xD9 && b2 >= 0xA0 && b2 <= 0xA9);
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

    // Single char tokens
    if (*current == '.') { token.type = TOKEN_DOT; l->pos++; return token; }
    if (*current == '+') { token.type = TOKEN_PLUS; l->pos++; return token; }
    if (*current == '-') { token.type = TOKEN_MINUS; l->pos++; return token; }

    // Numbers (ASCII or Arabic)
    if (isdigit(*current) || is_arabic_digit(current)) {
        token.type = TOKEN_INT;
        
        // We will normalize everything to ASCII string for storage
        char buffer[64] = {0}; 
        int buf_idx = 0;

        while (l->pos < l->len) {
            const char* c = l->src + l->pos;
            if (isdigit(*c)) {
                buffer[buf_idx++] = *c;
                l->pos++;
            } 
            else if (is_arabic_digit(c)) {
                // Convert Arabic UTF-8 digit to ASCII char
                // 0xD9 0xA0 is 0. 0xA0 - 0xA0 = 0. + '0' = ASCII '0'
                unsigned char val = (unsigned char)c[1] - 0xA0;
                buffer[buf_idx++] = val + '0';
                l->pos += 2; // Arabic digits are 2 bytes
            } 
            else {
                break;
            }
        }
        
        token.value = strdup(buffer);
        return token;
    }

    // Keyword: إرجع (8 bytes)
    if (strncmp(current, "إرجع", 8) == 0) {
        token.type = TOKEN_RETURN;
        l->pos += 8;
        return token;
    }

    // Keyword: اطبع (8 bytes)
    // Hex: D8 A7 D8 B7 D8 A8 D8 B9
    if (strncmp(current, "اطبع", 8) == 0) {
        token.type = TOKEN_PRINT;
        l->pos += 8;
        return token;
    }

    token.type = TOKEN_INVALID;
    printf("Lexer Error: Unknown byte 0x%02X at line %d\n", (unsigned char)*current, l->line);
    exit(1);
    return token;
}