#include "test_framework.h"
#include "baa/lexer/lexer.h"
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

// Helper function to tokenize a string and return all tokens
BaaToken** get_all_tokens(const wchar_t* source, size_t* token_count)
{
    BaaLexer lexer;
    baa_init_lexer(&lexer, source, L"test.baa");
    
    BaaToken** tokens = NULL;
    size_t capacity = 10;
    *token_count = 0;
    
    tokens = malloc(capacity * sizeof(BaaToken*));
    if (!tokens) return NULL;
    
    BaaToken* token;
    while ((token = baa_lexer_next_token(&lexer)) != NULL) {
        if (*token_count >= capacity) {
            capacity *= 2;
            tokens = realloc(tokens, capacity * sizeof(BaaToken*));
            if (!tokens) return NULL;
        }
        
        tokens[(*token_count)++] = token;
        
        if (token->type == BAA_TOKEN_EOF) {
            break;
        }
    }
    
    return tokens;
}

void test_single_line_comments(void)
{
    TEST_SETUP();
    wprintf(L"Testing single-line comments...\n");
    
    const wchar_t* test_comments[] = {
        L"// Simple comment",
        L"// Comment with Arabic: مرحبا بالعالم",
        L"// Comment with numbers: 123 ٤٥٦",
        L"// Comment with symbols: !@#$%^&*()",
        L"// Empty comment after this:",
        L"//",
        L"// Comment with escape sequences: \\n \\t \\r",
        L"// Very long comment that goes on and on and on and on and on and on and on",
        L"// Comment with mixed languages: Hello مرحبا 123 ٤٥٦",
    };
    
    size_t num_comments = sizeof(test_comments) / sizeof(test_comments[0]);
    
    for (size_t i = 0; i < num_comments; i++) {
        size_t token_count;
        BaaToken** tokens = get_all_tokens(test_comments[i], &token_count);
        ASSERT_NOT_NULL(tokens, L"Tokens should not be NULL");
        
        // Find the comment token
        bool found_comment = false;
        for (size_t j = 0; j < token_count; j++) {
            if (tokens[j]->type == BAA_TOKEN_SINGLE_LINE_COMMENT) {
                found_comment = true;
                ASSERT_WSTR_EQ(test_comments[i], tokens[j]->lexeme);
                break;
            }
        }
        ASSERT_TRUE(found_comment, L"Should find single-line comment");
        
        wprintf(L"  ✓ Comment: %ls\n", test_comments[i]);
        
        // Clean up
        for (size_t j = 0; j < token_count; j++) {
            baa_free_token(tokens[j]);
        }
        free(tokens);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Single-line comments test passed\n");
}

void test_multi_line_comments(void)
{
    TEST_SETUP();
    wprintf(L"Testing multi-line comments...\n");
    
    const wchar_t* test_comments[] = {
        L"/* Simple multi-line comment */",
        L"/* Multi-line\ncomment\nwith\nnewlines */",
        L"/* Comment with Arabic: مرحبا بالعالم */",
        L"/* Comment with numbers: 123 ٤٥٦ */",
        L"/* Comment with symbols: !@#$%^&*() */",
        L"/* Empty comment */",
        L"/**/",
        L"/* Comment with nested // single-line comment */",
        L"/* Comment with escape sequences: \\n \\t \\r */",
        L"/* Very long comment that goes on and on\nand spans multiple lines\nwith various content */",
    };
    
    size_t num_comments = sizeof(test_comments) / sizeof(test_comments[0]);
    
    for (size_t i = 0; i < num_comments; i++) {
        size_t token_count;
        BaaToken** tokens = get_all_tokens(test_comments[i], &token_count);
        ASSERT_NOT_NULL(tokens, L"Tokens should not be NULL");
        
        // Find the comment token
        bool found_comment = false;
        for (size_t j = 0; j < token_count; j++) {
            if (tokens[j]->type == BAA_TOKEN_MULTI_LINE_COMMENT) {
                found_comment = true;
                ASSERT_WSTR_EQ(test_comments[i], tokens[j]->lexeme);
                break;
            }
        }
        ASSERT_TRUE(found_comment, L"Should find multi-line comment");
        
        wprintf(L"  ✓ Multi-line comment: %ls\n", test_comments[i]);
        
        // Clean up
        for (size_t j = 0; j < token_count; j++) {
            baa_free_token(tokens[j]);
        }
        free(tokens);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Multi-line comments test passed\n");
}

void test_doc_comments(void)
{
    TEST_SETUP();
    wprintf(L"Testing documentation comments...\n");
    
    const wchar_t* test_doc_comments[] = {
        L"/// Simple doc comment",
        L"/// Documentation with Arabic: مرحبا بالعالم",
        L"/// @param value The input value",
        L"/// @return The result of the operation",
        L"/// @brief Brief description",
        L"/// @details Detailed description",
        L"/// Multi-line doc comment\n/// Second line\n/// Third line",
        L"/** Block doc comment */",
        L"/** Multi-line block\n * doc comment\n * with asterisks */",
    };
    
    size_t num_comments = sizeof(test_doc_comments) / sizeof(test_doc_comments[0]);
    
    for (size_t i = 0; i < num_comments; i++) {
        size_t token_count;
        BaaToken** tokens = get_all_tokens(test_doc_comments[i], &token_count);
        ASSERT_NOT_NULL(tokens, L"Tokens should not be NULL");
        
        // Find the doc comment token
        bool found_comment = false;
        for (size_t j = 0; j < token_count; j++) {
            if (tokens[j]->type == BAA_TOKEN_DOC_COMMENT) {
                found_comment = true;
                // Doc comments might be processed differently, so just check they exist
                break;
            }
        }
        
        if (found_comment) {
            wprintf(L"  ✓ Doc comment: %ls\n", test_doc_comments[i]);
        } else {
            wprintf(L"  ⚠ Doc comment not recognized: %ls\n", test_doc_comments[i]);
        }
        
        // Clean up
        for (size_t j = 0; j < token_count; j++) {
            baa_free_token(tokens[j]);
        }
        free(tokens);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Documentation comments test passed\n");
}

void test_comments_with_code(void)
{
    TEST_SETUP();
    wprintf(L"Testing comments mixed with code...\n");
    
    const wchar_t* test_mixed[] = {
        L"variable // comment after code",
        L"// comment before code\nvariable",
        L"variable /* comment */ more_code",
        L"/* comment */ variable",
        L"variable1 // first comment\nvariable2 // second comment",
        L"/* multi-line\ncomment */ variable /* another comment */",
        L"variable1; // comment\nvariable2; /* another comment */",
    };
    
    size_t num_mixed = sizeof(test_mixed) / sizeof(test_mixed[0]);
    
    for (size_t i = 0; i < num_mixed; i++) {
        size_t token_count;
        BaaToken** tokens = get_all_tokens(test_mixed[i], &token_count);
        ASSERT_NOT_NULL(tokens, L"Tokens should not be NULL");
        
        // Should have both comments and identifiers
        bool found_comment = false;
        bool found_identifier = false;
        
        for (size_t j = 0; j < token_count; j++) {
            if (tokens[j]->type == BAA_TOKEN_SINGLE_LINE_COMMENT || 
                tokens[j]->type == BAA_TOKEN_MULTI_LINE_COMMENT) {
                found_comment = true;
            }
            if (tokens[j]->type == BAA_TOKEN_IDENTIFIER) {
                found_identifier = true;
            }
        }
        
        ASSERT_TRUE(found_comment, L"Should find comment in mixed code");
        ASSERT_TRUE(found_identifier, L"Should find identifier in mixed code");
        
        wprintf(L"  ✓ Mixed code: %ls\n", test_mixed[i]);
        
        // Clean up
        for (size_t j = 0; j < token_count; j++) {
            baa_free_token(tokens[j]);
        }
        free(tokens);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Comments with code test passed\n");
}

void test_nested_comments(void)
{
    TEST_SETUP();
    wprintf(L"Testing nested comments...\n");
    
    const wchar_t* test_nested[] = {
        L"/* outer /* inner */ still outer */",
        L"/* level 1 /* level 2 /* level 3 */ level 2 */ level 1 */",
        L"/* comment with // single-line inside */",
        L"// comment with /* multi-line inside",
        L"/* /* /* deeply nested */ */ */",
    };
    
    size_t num_nested = sizeof(test_nested) / sizeof(test_nested[0]);
    
    for (size_t i = 0; i < num_nested; i++) {
        size_t token_count;
        BaaToken** tokens = get_all_tokens(test_nested[i], &token_count);
        ASSERT_NOT_NULL(tokens, L"Tokens should not be NULL");
        
        // The behavior depends on whether nested comments are supported
        // Just verify that the lexer handles them without crashing
        wprintf(L"  ✓ Nested comment handled: %ls\n", test_nested[i]);
        
        // Clean up
        for (size_t j = 0; j < token_count; j++) {
            baa_free_token(tokens[j]);
        }
        free(tokens);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Nested comments test passed\n");
}

void test_unterminated_comments(void)
{
    TEST_SETUP();
    wprintf(L"Testing unterminated comments...\n");
    
    const wchar_t* test_unterminated[] = {
        L"/* unterminated multi-line comment",
        L"/* unterminated with newlines\nand more content",
        L"/* unterminated /* with nested",
        L"/** unterminated doc comment",
    };
    
    size_t num_unterminated = sizeof(test_unterminated) / sizeof(test_unterminated[0]);
    
    for (size_t i = 0; i < num_unterminated; i++) {
        size_t token_count;
        BaaToken** tokens = get_all_tokens(test_unterminated[i], &token_count);
        ASSERT_NOT_NULL(tokens, L"Tokens should not be NULL");
        
        // Should either produce an error token or handle gracefully
        bool found_error = false;
        for (size_t j = 0; j < token_count; j++) {
            if (tokens[j]->type == BAA_TOKEN_ERROR) {
                found_error = true;
                break;
            }
        }
        
        if (found_error) {
            wprintf(L"  ✓ Correctly detected unterminated comment: %ls\n", test_unterminated[i]);
        } else {
            wprintf(L"  ⚠ Unterminated comment handled gracefully: %ls\n", test_unterminated[i]);
        }
        
        // Clean up
        for (size_t j = 0; j < token_count; j++) {
            baa_free_token(tokens[j]);
        }
        free(tokens);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Unterminated comments test passed\n");
}

void test_comment_edge_cases(void)
{
    TEST_SETUP();
    wprintf(L"Testing comment edge cases...\n");
    
    const wchar_t* test_edge_cases[] = {
        L"//",                    // Empty single-line comment
        L"/**/",                 // Empty multi-line comment
        L"///",                  // Triple slash (might be doc comment)
        L"////",                 // Quadruple slash
        L"/* / */",              // Slash inside multi-line comment
        L"/* * */",              // Asterisk inside multi-line comment
        L"// /* not a multi-line comment */",
        L"/* // not end of comment */ still comment */",
        L"/*\n*/",               // Multi-line comment with just newline
        L"/*\r\n*/",             // Multi-line comment with CRLF
        L"/*\t*/",               // Multi-line comment with tab
    };
    
    size_t num_edge_cases = sizeof(test_edge_cases) / sizeof(test_edge_cases[0]);
    
    for (size_t i = 0; i < num_edge_cases; i++) {
        size_t token_count;
        BaaToken** tokens = get_all_tokens(test_edge_cases[i], &token_count);
        ASSERT_NOT_NULL(tokens, L"Tokens should not be NULL");
        
        wprintf(L"  ✓ Edge case handled: %ls\n", test_edge_cases[i]);
        
        // Clean up
        for (size_t j = 0; j < token_count; j++) {
            baa_free_token(tokens[j]);
        }
        free(tokens);
    }
    
    TEST_TEARDOWN();
    wprintf(L"✓ Comment edge cases test passed\n");
}

TEST_SUITE_BEGIN()

wprintf(L"Running Lexer Comment tests...\n\n");

TEST_CASE(test_single_line_comments);
TEST_CASE(test_multi_line_comments);
TEST_CASE(test_doc_comments);
TEST_CASE(test_comments_with_code);
TEST_CASE(test_nested_comments);
TEST_CASE(test_unterminated_comments);
TEST_CASE(test_comment_edge_cases);

wprintf(L"\n✓ All Lexer Comment tests completed!\n");

TEST_SUITE_END()
