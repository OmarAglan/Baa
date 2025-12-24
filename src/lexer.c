#include "baa.h"
#include <ctype.h>

void lexer_init(Lexer* l, const char* src) {
    l->src = src;
    l->pos = 0;
    l->len = strlen(src);
    l->line = 1;
    
    // FIX: Check for UTF-8 Byte Order Mark (BOM)
    // The sequence is 0xEF, 0xBB, 0xBF
    if (l->len >= 3 && (unsigned char)src[0] == 0xEF && (unsigned char)src[1] == 0xBB && (unsigned char)src[2] == 0xBF) l->pos = 3;
}
// Helper to check for Arabic Digit (UTF-8: 0xD9 0xA0 to 0xD9 0xA9)
int is_arabic_start_byte(char c) {
    unsigned char ub = (unsigned char)c;
    return (ub >= 0xD8 && ub <= 0xDB); // Common Arabic range starts
}

int is_arabic_digit(const char* c) {
    unsigned char b1 = (unsigned char)c[0];
    unsigned char b2 = (unsigned char)c[1];
    return (b1 == 0xD9 && b2 >= 0xA0 && b2 <= 0xA9);
}

Token lexer_next_token(Lexer* l) {
    Token token = {0};
    
    while (l->pos < l->len && isspace(l->src[l->pos])) {
        if (l->src[l->pos] == '\n') l->line++;
        l->pos++;
    }

    if (l->pos >= l->len) { token.type = TOKEN_EOF; return token; }

    const char* current = l->src + l->pos;

    // Symbols
    if (*current == '.') { token.type = TOKEN_DOT; l->pos++; return token; }
    if (*current == '+') { token.type = TOKEN_PLUS; l->pos++; return token; }
    if (*current == '-') { token.type = TOKEN_MINUS; l->pos++; return token; }
    if (*current == '=') { token.type = TOKEN_ASSIGN; l->pos++; return token; }

    // Numbers
    if (isdigit(*current) || is_arabic_digit(current)) {
        token.type = TOKEN_INT;
        char buffer[64] = {0}; 
        int buf_idx = 0;
        while (l->pos < l->len) {
            const char* c = l->src + l->pos;
            if (isdigit(*c)) { buffer[buf_idx++] = *c; l->pos++; } 
            else if (is_arabic_digit(c)) {
                buffer[buf_idx++] = ((unsigned char)c[1] - 0xA0) + '0';
                l->pos += 2;
            } else { break; }
        }
        token.value = strdup(buffer);
        return token;
    }

    // Identifiers and Keywords (Arabic)
    if (is_arabic_start_byte(*current)) {
        // Find end of word
        size_t start = l->pos;
        while (l->pos < l->len && !isspace(l->src[l->pos]) && 
               l->src[l->pos] != '.' && l->src[l->pos] != '+' && 
               l->src[l->pos] != '-' && l->src[l->pos] != '=') {
            l->pos++;
        }
        
        size_t len = l->pos - start;
        char* word = malloc(len + 1);
        strncpy(word, l->src + start, len);
        word[len] = '\0';

        // Check Keywords
        if (strcmp(word, "إرجع") == 0) token.type = TOKEN_RETURN;
        else if (strcmp(word, "اطبع") == 0) token.type = TOKEN_PRINT;
        else if (strcmp(word, "رقم") == 0) token.type = TOKEN_KEYWORD_INT;
        else {
            // Not a keyword? It's a variable name.
            token.type = TOKEN_IDENTIFIER;
            token.value = word; // Keep the name
            return token;
        }
        free(word); // Free if it was a keyword
        return token;
    }

    token.type = TOKEN_INVALID;
    printf("Lexer Error: Unknown byte 0x%02X at line %d\n", (unsigned char)*current, l->line);
    exit(1);
    return token;
}