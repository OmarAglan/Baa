#include <stdio.h>
#include <wchar.h>
#include <locale.h>
#include "include/baa/lexer/lexer.h"

void test_enhanced_error_context()
{
    // Set locale for wide character support
    setlocale(LC_ALL, "");
    
    printf("Testing Enhanced Error Context System...\n");
    
    // Test 1: Unterminated string with context
    const wchar_t *test_source1 = L"متغير نص = \"هذا نص غير منته";
    BaaLexer *lexer1 = baa_create_lexer(test_source1);
    
    printf("\nTest 1: Unterminated String\n");
    printf("Source: ");
    wprintf(L"%ls\n", test_source1);
    
    BaaToken *token1;
    while ((token1 = baa_lexer_next_token(lexer1)) && token1->type != BAA_TOKEN_EOF)
    {
        if (baa_token_is_error(token1->type))
        {
            printf("Error Token Found:\n");
            printf("  Type: %d\n", token1->type);
            printf("  Message: ");
            wprintf(L"%ls\n", token1->lexeme);
            
            if (token1->error)
            {
                printf("  Error Code: %u\n", token1->error->error_code);
                printf("  Category: %s\n", token1->error->category);
                
                if (token1->error->suggestion)
                {
                    printf("  Suggestion: ");
                    wprintf(L"%ls\n", token1->error->suggestion);
                }
                
                if (token1->error->context_before)
                {
                    printf("  Context Before: ");
                    wprintf(L"%ls\n", token1->error->context_before);
                }
                
                if (token1->error->context_after)
                {
                    printf("  Context After: ");
                    wprintf(L"%ls\n", token1->error->context_after);
                }
            }
            break;
        }
        baa_free_token(token1);
    }
    
    baa_free_token(token1);
    baa_free_lexer(lexer1);
    
    // Test 2: Invalid escape sequence
    const wchar_t *test_source2 = L"نص = \"مرحبا\\x بالعالم\"";
    BaaLexer *lexer2 = baa_create_lexer(test_source2);
    
    printf("\nTest 2: Invalid Escape Sequence\n");
    printf("Source: ");
    wprintf(L"%ls\n", test_source2);
    
    BaaToken *token2;
    while ((token2 = baa_lexer_next_token(lexer2)) && token2->type != BAA_TOKEN_EOF)
    {
        if (baa_token_is_error(token2->type))
        {
            printf("Error Token Found:\n");
            printf("  Type: %d\n", token2->type);
            printf("  Message: ");
            wprintf(L"%ls\n", token2->lexeme);
            
            if (token2->error && token2->error->suggestion)
            {
                printf("  Smart Suggestion: ");
                wprintf(L"%ls\n", token2->error->suggestion);
            }
            break;
        }
        baa_free_token(token2);
    }
    
    baa_free_token(token2);
    baa_free_lexer(lexer2);
    
    // Test 3: Invalid number format
    const wchar_t *test_source3 = L"رقم = 123.45.67";
    BaaLexer *lexer3 = baa_create_lexer(test_source3);
    
    printf("\nTest 3: Invalid Number Format\n");
    printf("Source: ");
    wprintf(L"%ls\n", test_source3);
    
    BaaToken *token3;
    while ((token3 = baa_lexer_next_token(lexer3)) && token3->type != BAA_TOKEN_EOF)
    {
        if (baa_token_is_error(token3->type))
        {
            printf("Error Token Found:\n");
            printf("  Type: %d\n", token3->type);
            printf("  Message: ");
            wprintf(L"%ls\n", token3->lexeme);
            
            if (token3->error && token3->error->suggestion)
            {
                printf("  Smart Suggestion: ");
                wprintf(L"%ls\n", token3->error->suggestion);
            }
            break;
        }
        baa_free_token(token3);
    }
    
    baa_free_token(token3);
    baa_free_lexer(lexer3);
    
    printf("\nEnhanced Error Context System Test Complete!\n");
}

int main()
{
    test_enhanced_error_context();
    return 0;
}
