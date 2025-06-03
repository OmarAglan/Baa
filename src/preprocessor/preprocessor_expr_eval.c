#include "preprocessor_internal.h"
#include <wctype.h>

// --- Preprocessor Expression Evaluation Implementation ---

// Helper to create an error token
static PpExprToken make_error_token(PpExprTokenizer *tz, const wchar_t *message)
{
    if (tz->error_message && !*tz->error_message)
    { // Set error only once
        if (tz->pp_state)
        {
            // Get the original location (file, line) from the stack
            PpSourceLocation original_loc = get_current_original_location(tz->pp_state);
            // Use the column number captured *before* the token was consumed
            PpSourceLocation error_loc = {
                .file_path = original_loc.file_path,
                .line = original_loc.line,
                // Calculate absolute column: offset of expression string + (1-based column within expression string) - 1
                .column = (tz->expr_string_column_offset + (tz->current_token_start_column > 0 ? tz->current_token_start_column : 1) - 1)};
            // If the message is a format string, we need to handle potential varargs.
            // For simplicity in this refactor, assuming 'message' is a simple string or we pass it as a single arg.
            // A more robust solution might involve changing make_error_token to accept varargs.
            wchar_t final_message[512];                                                        // Assuming a max length for simplicity
            swprintf(final_message, sizeof(final_message) / sizeof(wchar_t), L"%ls", message); // Basic formatting
            *tz->error_message = format_preprocessor_error_at_location(&error_loc, final_message);
        }
        else
        {
            // Fallback if pp_state isn't available
            PpSourceLocation generic_loc = {"(expr_eval_no_context)", 0, 0};
            *tz->error_message = format_preprocessor_error_at_location(&generic_loc, L"خطأ في مقيم التعبير (لا يوجد سياق): %ls", message);
        }
    }
    return (PpExprToken){.type = PP_EXPR_TOKEN_ERROR};
}

// Helper to create a simple token
static PpExprToken make_token(PpExprTokenType type)
{
    return (PpExprToken){.type = type};
}

// Helper to create an integer literal token
static PpExprToken make_int_token(long value)
{
    return (PpExprToken){.type = PP_EXPR_TOKEN_INT_LITERAL, .value = value};
}

// Helper to create an identifier or 'defined' token
static PpExprToken make_identifier_token(PpExprTokenizer *tz)
{
    size_t len = tz->current - tz->start;
    wchar_t *text = wcsndup_internal(tz->start, len); // Use internal wcsndup
    if (!text)
    {
        return make_error_token(tz, L"فشل في تخصيص ذاكرة للمعرف في التعبير الشرطي.");
    }

    // Check if it's the 'معرف' (defined) keyword
    if (wcscmp(text, L"معرف") == 0)
    {
        free(text);
        return make_token(PP_EXPR_TOKEN_DEFINED);
    }

    return (PpExprToken){.type = PP_EXPR_TOKEN_IDENTIFIER, .text = text};
}

// Skips whitespace and updates column number
// This now updates a local column counter within the expression string.
static void skip_whitespace(PpExprTokenizer *tz)
{
    while (iswspace(*tz->current))
    {
        tz->current++;
    }
}

// Gets the next token from the expression string
static PpExprToken get_next_pp_expr_token(PpExprTokenizer *tz)
{
    skip_whitespace(tz);
    tz->start = tz->current; // Mark start of lexeme

    // Calculate 1-based column within the expression string where this token starts
    tz->current_token_start_column = (tz->current - tz->expression_string_start) + 1;

    if (*tz->current == L'\0')
        return make_token(PP_EXPR_TOKEN_EOF);

    // The stored tz->current_token_start_column is now the relative start column for this token.
    switch (*tz->current)
    {
    case L'(':
        tz->current++;
        if (tz->pp_state)
            tz->pp_state->current_column_number++;
        return make_token(PP_EXPR_TOKEN_LPAREN);
    case L')':
        tz->current++;
        if (tz->pp_state)
            tz->pp_state->current_column_number++;
        return make_token(PP_EXPR_TOKEN_RPAREN);
    case L'+':
        tz->current++;
        if (tz->pp_state)
            tz->pp_state->current_column_number++;
        return make_token(PP_EXPR_TOKEN_PLUS);
    case L'-':
        tz->current++;
        if (tz->pp_state)
            tz->pp_state->current_column_number++;
        return make_token(PP_EXPR_TOKEN_MINUS);
    case L'*':
        tz->current++;
        if (tz->pp_state)
            tz->pp_state->current_column_number++;
        return make_token(PP_EXPR_TOKEN_STAR);
    case L'/':
        tz->current++;
        if (tz->pp_state)
            tz->pp_state->current_column_number++;
        return make_token(PP_EXPR_TOKEN_SLASH);
    case L'%':
        tz->current++;
        if (tz->pp_state)
            tz->pp_state->current_column_number++;
        return make_token(PP_EXPR_TOKEN_PERCENT);
    case L'!':
        if (*(tz->current + 1) == L'=')
        {
            tz->current += 2;
            if (tz->pp_state)
                tz->pp_state->current_column_number += 2;
            return make_token(PP_EXPR_TOKEN_BANGEQ);
        }
        tz->current++;
        if (tz->pp_state)
            tz->pp_state->current_column_number++;
        return make_token(PP_EXPR_TOKEN_BANG);
    case L'=':
        if (*(tz->current + 1) == L'=')
        {
            tz->current += 2;
            if (tz->pp_state)
                tz->pp_state->current_column_number += 2;
            return make_token(PP_EXPR_TOKEN_EQEQ);
        }
        // Single '=' is assignment, invalid in preprocessor expr
        return make_error_token(tz, L"المعامل '=' غير صالح في التعبير الشرطي.");
    case L'<':
        if (*(tz->current + 1) == L'=')
        {
            tz->current += 2;
            if (tz->pp_state)
                tz->pp_state->current_column_number += 2;
            return make_token(PP_EXPR_TOKEN_LTEQ);
        }
        if (*(tz->current + 1) == L'<')
        {
            tz->current += 2;
            if (tz->pp_state)
                tz->pp_state->current_column_number += 2;
            return make_token(PP_EXPR_TOKEN_LSHIFT);
        }
        tz->current++;
        if (tz->pp_state)
            tz->pp_state->current_column_number++;
        return make_token(PP_EXPR_TOKEN_LT);
    case L'>':
        if (*(tz->current + 1) == L'=')
        {
            tz->current += 2;
            if (tz->pp_state)
                tz->pp_state->current_column_number += 2;
            return make_token(PP_EXPR_TOKEN_GTEQ);
        }
        if (*(tz->current + 1) == L'>')
        {
            tz->current += 2;
            if (tz->pp_state)
                tz->pp_state->current_column_number += 2;
            return make_token(PP_EXPR_TOKEN_RSHIFT);
        }
        tz->current++;
        if (tz->pp_state)
            tz->pp_state->current_column_number++;
        return make_token(PP_EXPR_TOKEN_GT);
    case L'&':
        if (*(tz->current + 1) == L'&')
        {
            tz->current += 2;
            if (tz->pp_state)
                tz->pp_state->current_column_number += 2;
            return make_token(PP_EXPR_TOKEN_AMPAMP);
        }
        tz->current++;
        if (tz->pp_state)
            tz->pp_state->current_column_number++;
        return make_token(PP_EXPR_TOKEN_AMPERSAND);
    case L'|':
        if (*(tz->current + 1) == L'|')
        {
            tz->current += 2;
            if (tz->pp_state)
                tz->pp_state->current_column_number += 2;
            return make_token(PP_EXPR_TOKEN_PIPEPIPE);
        }
        tz->current++;
        if (tz->pp_state)
            tz->pp_state->current_column_number++;
        return make_token(PP_EXPR_TOKEN_PIPE);
    case L'^':
        tz->current++;
        if (tz->pp_state)
            tz->pp_state->current_column_number++;
        return make_token(PP_EXPR_TOKEN_CARET);
    case L'~':
        tz->current++;
        if (tz->pp_state)
            tz->pp_state->current_column_number++;
        return make_token(PP_EXPR_TOKEN_TILDE);
    }

    // Check for integer literals
    if (iswdigit(*tz->current))
    {
        wchar_t *endptr;
        long value;
        int base = 10;
        // Check for 0x or 0b prefixes
        if (*tz->start == L'0' && tz->current - tz->start == 0)
        { // Only if '0' is the first char we've seen for this token
            if (*(tz->start + 1) == L'x' || *(tz->start + 1) == L'X')
            {
                base = 16;
                // Attempt to parse starting after "0x"
                value = wcstol(tz->start + 2, &endptr, base);
                if (endptr == tz->start + 2)
                { // No valid hex digits after 0x
                    return make_error_token(tz, L"رقم سداسي عشري غير صالح بعد '0x' في التعبير الشرطي.");
                }
            }
            else if (*(tz->start + 1) == L'b' || *(tz->start + 1) == L'B')
            {
                base = 2;
                // Attempt to parse starting after "0b"
                value = wcstol(tz->start + 2, &endptr, base);
                if (endptr == tz->start + 2)
                { // No valid binary digits after 0b
                    return make_error_token(tz, L"رقم ثنائي غير صالح بعد '0b' في التعبير الشرطي.");
                }
            }
            else
            {
                // Regular decimal (could be octal in C, but preprocessor often treats as decimal or disallows)
                // For simplicity, let's assume decimal if starts with 0 and not 0x/0b.
                value = wcstol(tz->start, &endptr, base);
            }
        }
        else
        {
            // Regular decimal number not starting with 0, or already past the prefix
            value = wcstol(tz->start, &endptr, base);
        }
        if (endptr == tz->start)
        { // No valid digits parsed
            return make_error_token(tz, L"رقم غير صالح في التعبير الشرطي.");
        }
        size_t num_len = endptr - tz->start;
        tz->current = endptr; // Advance tokenizer past the number
        // tz->current is advanced. current_token_start_column refers to the start.
        // No global column update needed here; it's relative to the expression string.

        return make_int_token(value);
    }

    // Check for identifiers (including 'defined')
    if (iswalpha(*tz->current) || *tz->current == L'_')
    {
        while (iswalnum(*tz->current) || *tz->current == L'_')
        {
            tz->current++;
        }
        size_t id_len = tz->current - tz->start;
        // tz->current is advanced. No global column update needed here.
        return make_identifier_token(tz);
    }

    // Unknown character
    return make_error_token(tz, L"رمز غير متوقع في التعبير الشرطي.");
}

// --- Actual Expression Evaluation Function ---

// Forward declarations for the expression parser/evaluator
static bool parse_and_evaluate_pp_expr(PpExprTokenizer *tz, long *result);
static bool parse_primary_pp_expr(PpExprTokenizer *tz, long *result);
static bool parse_unary_pp_expr(PpExprTokenizer *tz, long *result);
static bool parse_binary_expression_rhs(PpExprTokenizer *tz, int min_prec, long *lhs, long *result);
static int get_token_precedence(PpExprTokenType type);

// New static helper function to fully expand macros in an expression string
static wchar_t *fully_expand_expression_string(BaaPreprocessor *pp_state,
                                               const wchar_t *expression_str,
                                               size_t original_line_number_for_errors, // Line of the #if directive
                                               wchar_t **error_message)
{
    DynamicWcharBuffer current_input_buffer;
    DynamicWcharBuffer current_output_buffer;
    wchar_t *final_expanded_string = NULL;
    if (!init_dynamic_buffer(&current_input_buffer, wcslen(expression_str) + 128))
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        error_loc.line = original_line_number_for_errors;
        error_loc.column = 1;
        *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل تهيئة مخزن الإدخال لتوسيع تعبير #إذا.");
        return NULL;
    }
    if (!append_to_dynamic_buffer(&current_input_buffer, expression_str))
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        error_loc.line = original_line_number_for_errors;
        error_loc.column = 1;
        *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل نسخ التعبير إلى مخزن الإدخال لتوسيع تعبير #إذا.");
        free_dynamic_buffer(&current_input_buffer);
        return NULL;
    }
    bool overall_success = true;
    int pass_count = 0;
    const int MAX_RESCAN_PASSES = 256;
    bool expansion_made_this_pass;
    do
    {
        if (!init_dynamic_buffer(&current_output_buffer, current_input_buffer.length + 128))
        {
            overall_success = false;
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            error_loc.line = original_line_number_for_errors;
            error_loc.column = 1;
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل تهيئة مخزن الإخراج لتوسيع تعبير #إذا.");
            break;
        }
        expansion_made_this_pass = scan_and_substitute_macros_one_pass(
            pp_state,
            current_input_buffer.buffer,
            original_line_number_for_errors,
            &current_output_buffer,
            &overall_success,
            error_message);
        free_dynamic_buffer(&current_input_buffer);
        if (!init_dynamic_buffer(&current_input_buffer, current_output_buffer.length + 128))
        {
            overall_success = false;
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            error_loc.line = original_line_number_for_errors;
            error_loc.column = 1;
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل إعادة تهيئة مخزن الإدخال لتوسيع تعبير #إذا.");
            free_dynamic_buffer(&current_output_buffer);
            break;
        }
        if (current_output_buffer.length > 0)
        {
            if (!append_to_dynamic_buffer(&current_input_buffer, current_output_buffer.buffer))
            {
                overall_success = false;
                PpSourceLocation error_loc = get_current_original_location(pp_state);
                error_loc.line = original_line_number_for_errors;
                error_loc.column = 1;
                *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل نسخ إلى مخزن الإدخال لتوسيع تعبير #إذا.");
                free_dynamic_buffer(&current_output_buffer);
                break;
            }
        }
        free_dynamic_buffer(&current_output_buffer);
        if (!overall_success)
            break;
        pass_count++;
        if (pass_count > MAX_RESCAN_PASSES)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            error_loc.line = original_line_number_for_errors;
            error_loc.column = 1;
            *error_message = format_preprocessor_error_at_location(&error_loc, L"تم تجاوز الحد الأقصى لمرات إعادة فحص الماكرو لتعبير #إذا (%d).", MAX_RESCAN_PASSES);
            overall_success = false;
            break;
        }
    } while (expansion_made_this_pass && overall_success);
    if (overall_success)
    {
        final_expanded_string = baa_strdup(current_input_buffer.buffer);
        if (!final_expanded_string)
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            error_loc.line = original_line_number_for_errors;
            error_loc.column = 1;
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصيص الذاكرة للسلسلة النهائية لتعبير #إذا.");
        }
    }
    free_dynamic_buffer(&current_input_buffer);
    return final_expanded_string;
}

bool evaluate_preprocessor_expression(BaaPreprocessor *pp_state, const wchar_t *raw_expression, bool *value, wchar_t **error_message, const char *abs_path_unused)
{
    // The caller (handle_preprocessor_directive) must ensure that
    // pp_state->current_column_number is set to the column where the `expression` string
    // *starts on the original source line* before this function is called.
    // This value will be used to initialize tz.expr_string_column_offset.

    *error_message = NULL;
    *value = false; // Default to false

    // Store the original line number of the #if directive for error reporting and __LINE__ expansion
    size_t directive_line_number = pp_state->current_line_number;
    wchar_t *expanded_expression_str = fully_expand_expression_string(pp_state, raw_expression, directive_line_number, error_message);
    if (!expanded_expression_str)
    {
        // error_message already set by fully_expand_expression_string
        return false;
    }
    fwprintf(stderr, L"DEBUG #if Fully Expanded Expr: [%ls]\n", expanded_expression_str); // Uncomment for debugging
    PpExprTokenizer tz = {
        .current = expanded_expression_str,                           // Use the fully expanded string
        .expression_string_start = expanded_expression_str,           // Store the beginning of the expanded expression string
        .expr_string_column_offset = pp_state->current_column_number, // Column where this expression starts on the original line
        .start = expanded_expression_str,
        .pp_state = pp_state,                    // Pass the state for context
        .abs_path = pp_state->current_file_path, // Use current file from pp_state for context
        .error_message = error_message,
        .current_token_start_column = 1 // Reset column for the start of tokenizing the expanded string
    };
    // tz.current_token_start_column will be calculated by get_next_pp_expr_token relative to expression_string_start
    long result_value = 0;
    if (!parse_and_evaluate_pp_expr(&tz, &result_value))
    {
        free(expanded_expression_str);
        // Error message should be set by the parser/evaluator
        return false;
    }

    // Check if the entire expression was consumed
    PpExprToken eof_token = get_next_pp_expr_token(&tz);
    if (eof_token.type != PP_EXPR_TOKEN_EOF)
    { // Check for EOF on the expanded string
        if (eof_token.type != PP_EXPR_TOKEN_ERROR)
        {
            // When making this error, use directive_line_number and tz.current_token_start_column
            // (or an approximation of column within the expanded_expression_str)
            make_error_token(&tz, L"رموز زائدة في نهاية التعبير الشرطي.");
        }
        free(expanded_expression_str);
        return false;
    }

    *value = (result_value != 0); // Final result: 0 is false, non-zero is true
    free(expanded_expression_str);
    return true;
}

// Main parsing function - starts with unary operators
static bool parse_and_evaluate_pp_expr(PpExprTokenizer *tz, long *result)
{
    long lhs;
    if (!parse_unary_pp_expr(tz, &lhs))
    {
        return false;
    }

    // Now parse binary operators with precedence climbing
    if (!parse_binary_expression_rhs(tz, 0, &lhs, result))
    {
        return false;
    }

    return true;
}

// Parses unary expressions: + - ! ~ primary
static bool parse_unary_pp_expr(PpExprTokenizer *tz, long *result)
{
    const wchar_t *unary_start_pos = tz->current; // Remember position before token read
    PpExprToken op_token = get_next_pp_expr_token(tz);

    if (op_token.type == PP_EXPR_TOKEN_PLUS)
    {
        // Unary plus - parse operand and return its value
        return parse_unary_pp_expr(tz, result);
    }
    else if (op_token.type == PP_EXPR_TOKEN_MINUS)
    {
        // Unary minus - parse operand, then negate
        if (!parse_unary_pp_expr(tz, result))
            return false;
        *result = -(*result);
        return true;
    }
    else if (op_token.type == PP_EXPR_TOKEN_BANG)
    {
        // Logical NOT - parse operand, then apply !
        if (!parse_unary_pp_expr(tz, result))
            return false;
        *result = (*result == 0); // 0 -> 1, non-zero -> 0
        return true;
    }
    else if (op_token.type == PP_EXPR_TOKEN_TILDE)
    { // Bitwise NOT
        if (!parse_unary_pp_expr(tz, result))
            return false;
        *result = ~(*result);
        return true;
    }
    else
    {
        // Not a unary operator we handle, put token back and parse primary
        tz->current = unary_start_pos; // Reset position
        return parse_primary_pp_expr(tz, result);
    }
}

// Parses primary expressions: integer literals, defined(MACRO), ( expression )
static bool parse_primary_pp_expr(PpExprTokenizer *tz, long *result)
{
    PpExprToken token = get_next_pp_expr_token(tz);

    if (token.type == PP_EXPR_TOKEN_INT_LITERAL)
    {
        *result = token.value;
        return true;
    }
    else if (token.type == PP_EXPR_TOKEN_DEFINED)
    {
        // Expect '(' or identifier
        bool parens = false;
        PpExprToken next_token = get_next_pp_expr_token(tz);
        if (next_token.type == PP_EXPR_TOKEN_LPAREN)
        {
            parens = true;
            next_token = get_next_pp_expr_token(tz); // Get token inside parens
        }

        if (next_token.type == PP_EXPR_TOKEN_IDENTIFIER)
        {
            // Check if the identifier macro is defined
            *result = (find_macro(tz->pp_state, next_token.text) != NULL) ? 1L : 0L; // Result is 1 or 0
            free(next_token.text);
        }
        else if (next_token.type == PP_EXPR_TOKEN_DEFINED)
        {
            // Special case: معرف معرف - treat the second معرف as identifier "معرف"
            // Check if there's a macro named "معرف" (there shouldn't be, so result is 0)
            *result = (find_macro(tz->pp_state, L"معرف") != NULL) ? 1L : 0L;
        }
        else
        {
            if (next_token.type != PP_EXPR_TOKEN_ERROR)
            {
                make_error_token(tz, L"تنسيق defined() غير صالح: متوقع معرف.");
            }
            if (next_token.type == PP_EXPR_TOKEN_IDENTIFIER)
                free(next_token.text); // Free if allocated
            return false;
        }

        // Check for closing parenthesis if needed
        if (parens)
        {
            PpExprToken closing_paren = get_next_pp_expr_token(tz);
            if (closing_paren.type != PP_EXPR_TOKEN_RPAREN)
            {
                if (closing_paren.type != PP_EXPR_TOKEN_ERROR)
                {
                    make_error_token(tz, L"تنسيق defined() غير صالح: قوس الإغلاق ')' مفقود.");
                }
                return false;
            }
        }
        return true; // Successfully evaluated defined()
    }
    else if (token.type == PP_EXPR_TOKEN_LPAREN)
    {
        // Parse expression inside parentheses
        if (!parse_and_evaluate_pp_expr(tz, result))
        {
            return false; // Error inside parentheses
        }
        // Expect closing parenthesis
        PpExprToken closing_paren = get_next_pp_expr_token(tz);
        if (closing_paren.type != PP_EXPR_TOKEN_RPAREN)
        {
            if (closing_paren.type != PP_EXPR_TOKEN_ERROR)
            {
                make_error_token(tz, L"قوس الإغلاق ')' مفقود بعد التعبير.");
            }
            return false;
        }
        return true;
    }
    else if (token.type == PP_EXPR_TOKEN_IDENTIFIER)
    {
        // Identifier: Check if it's a defined macro.
        const BaaMacro *macro = find_macro(tz->pp_state, token.text);
        if (macro && !macro->is_function_like)
        {
            // Found an object-like macro. Attempt to evaluate its body as an integer.
            // This is a simplified approach; a full implementation would need to
            // substitute and re-tokenize/re-parse the macro body.
            wchar_t *endptr;
            long macro_value = wcstol(macro->body, &endptr, 10); // Try base 10

            // Check if the entire body was a valid integer
            // Skip leading/trailing whitespace in the check
            const wchar_t *body_ptr = macro->body;
            while (iswspace(*body_ptr))
                body_ptr++;
            if (endptr == body_ptr || *endptr != L'\0')
            {
                // Body wasn't a simple integer literal, treat as 0 for now
                // TODO: Implement full macro expansion and re-evaluation here.
                *result = 0L;
                // Report a warning? Or just evaluate to 0 silently? C standard is 0.
                // make_error_token(tz, L"Warning: Macro body is not a simple integer, evaluating as 0.");
            }
            else
            {
                // Successfully parsed the body as an integer
                *result = macro_value;
            }
        }
        else
        {
            // Not a defined object-like macro (or is function-like), evaluates to 0.
            *result = 0L;
        }
        free(token.text);
        return true;
    }
    else if (token.type == PP_EXPR_TOKEN_ERROR)
    {
        // Error already set by tokenizer
        return false;
    }
    else
    {
        // Unexpected token
        make_error_token(tz, L"رمز غير متوقع في بداية التعبير الأولي.");
        return false;
    }
}

// --- Precedence Climbing Parser Implementation ---

// Gets the precedence level of a binary operator token. Returns -1 if not a binary operator.
static int get_token_precedence(PpExprTokenType type)
{
    switch (type)
    {
    // Lower precedence binds looser
    case PP_EXPR_TOKEN_PIPEPIPE:
        return 10; // ||
    case PP_EXPR_TOKEN_AMPAMP:
        return 20; // &&
    case PP_EXPR_TOKEN_PIPE:
        return 30; // | (Bitwise OR)
    case PP_EXPR_TOKEN_CARET:
        return 40; // ^ (Bitwise XOR)
    case PP_EXPR_TOKEN_AMPERSAND:
        return 50; // & (Bitwise AND)
    case PP_EXPR_TOKEN_EQEQ:
    case PP_EXPR_TOKEN_BANGEQ:
        return 60; // == !=
    case PP_EXPR_TOKEN_LT:
    case PP_EXPR_TOKEN_GT:
    case PP_EXPR_TOKEN_LTEQ:
    case PP_EXPR_TOKEN_GTEQ:
        return 70; // < > <= >=
    case PP_EXPR_TOKEN_LSHIFT:
    case PP_EXPR_TOKEN_RSHIFT:
        return 80; // << >>
    case PP_EXPR_TOKEN_PLUS:
    case PP_EXPR_TOKEN_MINUS:
        return 90; // + -
    case PP_EXPR_TOKEN_STAR:
    case PP_EXPR_TOKEN_SLASH:
    case PP_EXPR_TOKEN_PERCENT:
        return 100; // * / %
    // Higher precedence binds tighter (Unary operators handled separately)
    default:
        return -1; // Not a binary operator we handle yet
    }
}

// Parses the right-hand side of binary expressions using precedence climbing.
static bool parse_binary_expression_rhs(PpExprTokenizer *tz, int min_prec, long *lhs, long *result)
{
    while (true)
    {
        // Peek at the next token to see if it's a binary operator
        const wchar_t *current_pos_backup = tz->current; // Backup position
        PpExprToken op_token = get_next_pp_expr_token(tz);
        int token_prec = get_token_precedence(op_token.type);

        // If it's not a binary operator OR its precedence is too low, we're done with this level.
        if (token_prec < min_prec)
        {
            tz->current = current_pos_backup; // Put the token back by resetting position
            *result = *lhs;                   // The LHS is the final result for this precedence level
            return true;
        }

        // We have a binary operator with sufficient precedence. Consume it (already done by get_next_pp_expr_token).

        // Parse the RHS operand (starting with unary)
        long rhs = 0;
        if (!parse_unary_pp_expr(tz, &rhs))
        {
            // Error parsing RHS operand
            return false;
        }

        // Peek at the *next* operator to handle right-associativity or higher precedence.
        const wchar_t *next_op_pos_backup = tz->current;
        PpExprToken next_op_token = get_next_pp_expr_token(tz);
        int next_prec = get_token_precedence(next_op_token.type);
        tz->current = next_op_pos_backup; // Reset position after peeking

        // If the next operator has higher precedence, recursively parse its RHS first.
        // Note: For right-associative operators (like assignment, not handled here),
        // we would use `token_prec` instead of `token_prec + 1`.
        if (next_prec > token_prec)
        {
            if (!parse_binary_expression_rhs(tz, token_prec + 1, &rhs, &rhs))
            {
                // Error parsing nested RHS
                return false;
            }
        }
        // --- Perform the operation for the current operator ---
        long current_lhs = *lhs; // Capture LHS before potential modification by recursive calls
        switch (op_token.type)
        {
        case PP_EXPR_TOKEN_PLUS:
            *lhs = current_lhs + rhs;
            break;
        case PP_EXPR_TOKEN_MINUS:
            *lhs = current_lhs - rhs;
            break;
        case PP_EXPR_TOKEN_STAR:
            *lhs = current_lhs * rhs;
            break;
        case PP_EXPR_TOKEN_SLASH:
            if (rhs == 0)
            {
                make_error_token(tz, L"قسمة على صفر في التعبير الشرطي.");
                return false;
            }
            *lhs = current_lhs / rhs;
            break;
        case PP_EXPR_TOKEN_PERCENT:
            if (rhs == 0)
            {
                make_error_token(tz, L"قسمة على صفر (معامل الباقي) في التعبير الشرطي.");
                return false;
            }
            *lhs = current_lhs % rhs;
            break;
        case PP_EXPR_TOKEN_EQEQ:
            *lhs = (current_lhs == rhs);
            break;
        case PP_EXPR_TOKEN_BANGEQ:
            *lhs = (current_lhs != rhs);
            break;
        case PP_EXPR_TOKEN_LT:
            *lhs = (current_lhs < rhs);
            break;
        case PP_EXPR_TOKEN_GT:
            *lhs = (current_lhs > rhs);
            break;
        case PP_EXPR_TOKEN_LTEQ:
            *lhs = (current_lhs <= rhs);
            break;
        case PP_EXPR_TOKEN_GTEQ:
            *lhs = (current_lhs >= rhs);
            break;
        case PP_EXPR_TOKEN_AMPAMP:
            *lhs = (current_lhs != 0 && rhs != 0);
            break; // Use integer logic
        case PP_EXPR_TOKEN_PIPEPIPE:
            *lhs = (current_lhs != 0 || rhs != 0);
            break; // Use integer logic
        // Bitwise operations
        case PP_EXPR_TOKEN_AMPERSAND:
            *lhs = current_lhs & rhs;
            break;
        case PP_EXPR_TOKEN_PIPE:
            *lhs = current_lhs | rhs;
            break;
        case PP_EXPR_TOKEN_CARET:
            *lhs = current_lhs ^ rhs;
            break;
        case PP_EXPR_TOKEN_LSHIFT:
            *lhs = current_lhs << rhs;
            break;
        case PP_EXPR_TOKEN_RSHIFT:
            *lhs = current_lhs >> rhs;
            break;
        default:
            // Should not happen if get_token_precedence is correct
            make_error_token(tz, L"معامل ثنائي غير متوقع أو غير مدعوم.");
            return false;
        }
        // The result of the operation becomes the new LHS for the next iteration of the loop.
    }
}
