#include "baa/lexer/token_scanners.h" // For BaaToken and BaaLexer types, and scan_* declarations
#include "baa/lexer/lexer_internal.h"      // For make_token, make_error_token, peek, advance etc.
#include <stdlib.h>              // For malloc, realloc, free
#include <string.h>              // For wcslen, wcsncpy, wcscpy
#include <wctype.h>              // For iswalnum

// This file contains implementations for:
// BaaToken *scan_identifier(BaaLexer *lexer);
// BaaToken *scan_number(BaaLexer *lexer);
// BaaToken *scan_string(BaaLexer *lexer);
// BaaToken *scan_char_literal(BaaLexer *lexer);
// BaaToken *scan_multiline_string_literal(BaaLexer *lexer);


// Array of keywords and their corresponding token types (copied from lexer.c)
// This needs to be accessible here if scan_identifier uses it.
// Alternatively, keyword checking could be a separate utility.
// For now, duplicate for simplicity of this step.
static struct
{
    const wchar_t *keyword;
    BaaTokenType token;
} keywords_for_scanners[] = {
    {L"دالة", BAA_TOKEN_FUNC},
    {L"إرجع", BAA_TOKEN_RETURN},
    {L"إذا", BAA_TOKEN_IF},
    {L"إلا", BAA_TOKEN_ELSE},
    {L"طالما", BAA_TOKEN_WHILE},
    {L"لأجل", BAA_TOKEN_FOR},
    {L"افعل", BAA_TOKEN_DO},
    {L"اختر", BAA_TOKEN_SWITCH},
    {L"حالة", BAA_TOKEN_CASE},
    {L"توقف", BAA_TOKEN_BREAK},
    {L"استمر", BAA_TOKEN_CONTINUE},
    {L"متغير", BAA_TOKEN_VAR},
    {L"ثابت", BAA_TOKEN_CONST},
    {L"صحيح", BAA_TOKEN_BOOL_LIT},
    {L"خطأ", BAA_TOKEN_BOOL_LIT},
    {L"عدد_صحيح", BAA_TOKEN_TYPE_INT},
    {L"عدد_حقيقي", BAA_TOKEN_TYPE_FLOAT},
    {L"حرف", BAA_TOKEN_TYPE_CHAR},
    {L"فراغ", BAA_TOKEN_TYPE_VOID},
    {L"منطقي", BAA_TOKEN_TYPE_BOOL}
};


BaaToken *scan_identifier(BaaLexer *lexer)
{
    // lexer->start is already set before calling this function
    while (iswalnum(peek(lexer)) || peek(lexer) == L'_' ||
           is_arabic_letter(peek(lexer)) || is_arabic_digit(peek(lexer)))
    {
        advance(lexer);
    }

    // Check if identifier is a keyword
    size_t length = lexer->current - lexer->start;
    for (size_t i = 0; i < sizeof(keywords_for_scanners) / sizeof(keywords_for_scanners[0]); i++)
    {
        if (wcslen(keywords_for_scanners[i].keyword) == length &&
            wcsncmp(&lexer->source[lexer->start], keywords_for_scanners[i].keyword, length) == 0)
        {
            return make_token(lexer, keywords_for_scanners[i].token);
        }
    }

    return make_token(lexer, BAA_TOKEN_IDENTIFIER);
}

BaaToken *scan_number(BaaLexer *lexer)
{
    // lexer->start is already set
    // lexer->start is set by baa_lexer_next_token to its current position before calling scan_number.
    // The first character of the potential number is at peek(lexer).
    // This function is responsible for advancing past all characters of the number.
    bool is_float = false;
    wchar_t base_prefix = 0;
    bool (*is_valid_digit)(wchar_t) = is_baa_digit;
    size_t number_start_index; // Will be set after consuming potential prefix

    wchar_t first_char_of_number = peek(lexer);

    if (first_char_of_number == L'0')
    {
        wchar_t next = peek_next(lexer);
        if (next == L'x' || next == L'X')
        {
            base_prefix = 'x';
            is_valid_digit = is_baa_hex_digit;
            advance(lexer); // Consume '0'
            advance(lexer); // Consume 'x' or 'X'
            number_start_index = lexer->current; // Digits start after prefix
            if (!is_valid_digit(peek(lexer)))
            {
                BaaToken *error_token = make_error_token(lexer, L"عدد سداسي عشر غير صالح: يجب أن يتبع البادئة 0x/0X رقم سداسي عشري واحد على الأقل (السطر %zu، العمود %zu)", lexer->line, lexer->column);
                synchronize(lexer);
                return error_token;
            }
        }
        else if (next == L'b' || next == L'B')
        {
            base_prefix = 'b';
            is_valid_digit = is_baa_bin_digit;
            advance(lexer); // Consume '0'
            advance(lexer); // Consume 'b' or 'B'
            number_start_index = lexer->current; // Digits start after prefix
            if (!is_valid_digit(peek(lexer)))
            {
                BaaToken *error_token = make_error_token(lexer, L"عدد ثنائي غير صالح: يجب أن يتبع البادئة 0b/0B رقم ثنائي واحد على الأقل (السطر %zu، العمود %zu)", lexer->line, lexer->column);
                synchronize(lexer);
                return error_token;
            }
        } else {
            // It's a number starting with '0' but not '0x' or '0b' (e.g., "0", "0.5", "007")
            advance(lexer); // Consume the '0'
            number_start_index = lexer->current; // Next char is start of post-'0' digits or dot
        }
    } else {
        // Starts with a non-'0' digit
        advance(lexer); // Consume the first digit
        number_start_index = lexer->current;
    }

    bool last_char_was_underscore = false;
    while (is_valid_digit(peek(lexer)) || peek(lexer) == L'_')
    {
        //wchar_t char_in_loop = peek(lexer);
        //fwprintf(stderr, L"DEBUG scan_number int_loop: char='%lc'(%u), is_valid_digit_fn=%d, is_underscore=%d\n",
        //    char_in_loop, (unsigned int)char_in_loop, is_valid_digit(char_in_loop), char_in_loop == L'_');

        if (peek(lexer) == L'_')
        {
            // Error if previous char was also underscore OR if this underscore is the first char after a prefix.
            if (last_char_was_underscore || (base_prefix != 0 && lexer->current == number_start_index) ) {
                BaaToken *error_token = make_error_token(lexer, L"شرطة سفلية غير صالحة في العدد: متتالية أو مباشرة بعد البادئة (السطر %zu، العمود %zu).", lexer->line, lexer->column);
                synchronize(lexer);
                return error_token;
            }
            last_char_was_underscore = true;
        }
        else // It's a digit
        {
            if (!is_valid_digit(peek(lexer))) break;
            last_char_was_underscore = false;
        }
        advance(lexer);
    }

    if (last_char_was_underscore)
    {
        BaaToken *error_token = make_error_token(lexer, L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تكون في نهاية الرقم.", lexer->line, lexer->column - 1);
        synchronize(lexer);
        return error_token;
    }

    wchar_t current_peek = peek(lexer);
    if (base_prefix == 0 && (current_peek == L'.' || current_peek == 0x066B ))
    {
        wchar_t next_peek = peek_next(lexer);
        if (next_peek == L'_')
        {
            BaaToken *error_token = make_error_token(lexer, L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تتبع الفاصلة العشرية مباشرة.", lexer->line, lexer->column + 1);
            synchronize(lexer);
            return error_token;
        }
        if (is_baa_digit(next_peek))
        {
            is_float = true;
            advance(lexer); // Consume '.' or '٫'
            last_char_was_underscore = false;
            while (is_baa_digit(peek(lexer)) || peek(lexer) == L'_')
            {
                //wchar_t char_in_frac_loop = peek(lexer);
                //fwprintf(stderr, L"DEBUG scan_number frac_loop: char='%lc'(%u), is_baa_digit=%d, is_underscore=%d\n",
                //    char_in_frac_loop, (unsigned int)char_in_frac_loop, is_baa_digit(char_in_frac_loop), char_in_frac_loop == L'_');
                if (peek(lexer) == L'_')
                {
                    if (last_char_was_underscore)
                    {
                        BaaToken *error_token = make_error_token(lexer, L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تكون متتالية.", lexer->line, lexer->column);
                        synchronize(lexer);
                        return error_token;
                    }
                    last_char_was_underscore = true;
                }
                else
                {
                    if (!is_baa_digit(peek(lexer))) break;
                    last_char_was_underscore = false;
                }
                advance(lexer);
            }
            if (last_char_was_underscore)
            {
                BaaToken *error_token = make_error_token(lexer, L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تكون في نهاية الجزء الكسري.", lexer->line, lexer->column - 1);
                synchronize(lexer);
                return error_token;
            }
        }
    }

    current_peek = peek(lexer);
    if (base_prefix == 0 && (current_peek == L'e' || current_peek == L'E'))
    {
        wchar_t next_peek_exp = peek_next(lexer);
        if (next_peek_exp == L'_')
        {
            BaaToken *error_token = make_error_token(lexer, L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تتبع علامة الأس 'e'/'E' مباشرة.", lexer->line, lexer->column + 1);
            synchronize(lexer);
            return error_token;
        }
        bool has_exponent_part = false;
        int sign_offset = 0;
        if (next_peek_exp == L'+' || next_peek_exp == L'-')
        {
            sign_offset = 1;
            // Ensure there's a character after the sign before peeking lexer->source[lexer->current + 2]
            if (lexer->current + 2 < lexer->source_length) {
                wchar_t after_sign_peek = lexer->source[lexer->current + 2];
                if (after_sign_peek == L'_')
                {
                    BaaToken *error_token = make_error_token(lexer, L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تتبع علامة الأس (+/-) مباشرة.", lexer->line, lexer->column + 2);
                    synchronize(lexer);
                    return error_token;
                }
                if (is_baa_digit(after_sign_peek)) has_exponent_part = true;
            } else { // EOF after sign
                // Error: exponent needs digits after sign
            }
        }
        else if (is_baa_digit(next_peek_exp))
        {
            has_exponent_part = true;
        }

        if (has_exponent_part)
        {
            is_float = true;
            advance(lexer); // Consume 'e' or 'E'
            if (sign_offset == 1) advance(lexer); // Consume '+' or '-'
            if (!is_baa_digit(peek(lexer)))
            {
                BaaToken *error_token = make_error_token(lexer, L"تنسيق أس غير صالح بعد 'e' أو 'E' (السطر %zu، العمود %zu)", lexer->line, lexer->column);
                synchronize(lexer);
                return error_token;
            }
            last_char_was_underscore = false;
            while (is_baa_digit(peek(lexer)) || peek(lexer) == L'_')
            {
                //wchar_t char_in_exp_loop = peek(lexer);
                //fwprintf(stderr, L"DEBUG scan_number exp_loop: char='%lc'(%u), is_baa_digit=%d, is_underscore=%d\n",
                //    char_in_exp_loop, (unsigned int)char_in_exp_loop, is_baa_digit(char_in_exp_loop), char_in_exp_loop == L'_');
                if (peek(lexer) == L'_')
                {
                    if (last_char_was_underscore) {
                        BaaToken *error_token = make_error_token(lexer, L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تكون متتالية في الأس.", lexer->line, lexer->column);
                        synchronize(lexer);
                        return error_token;
                    }
                    last_char_was_underscore = true;
                }
                else
                {
                    if (!is_baa_digit(peek(lexer))) break;
                    last_char_was_underscore = false;
                }
                advance(lexer);
            }
            if (last_char_was_underscore)
            {
                BaaToken *error_token = make_error_token(lexer, L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تكون في نهاية الأس.", lexer->line, lexer->column - 1);
                synchronize(lexer);
                return error_token;
            }
        }
    }

    if (base_prefix != 0 && is_float)
    {
        BaaToken *error_token = make_error_token(lexer, L"الأعداد السداسية عشرية والثنائية لا يمكن أن تكون أعدادًا حقيقية (السطر %zu، العمود %zu)", lexer->line, number_start_index);
        synchronize(lexer);
        return error_token;
    }
    return make_token(lexer, is_float ? BAA_TOKEN_FLOAT_LIT : BAA_TOKEN_INT_LIT);
}

BaaToken *scan_string(BaaLexer *lexer)
{
    size_t buffer_cap = 64;
    size_t buffer_len = 0;
    wchar_t *buffer = malloc(buffer_cap * sizeof(wchar_t));
    if (!buffer)
    {
        return make_error_token(lexer, L"فشل في تخصيص ذاكرة لسلسلة نصية (السطر %zu)", lexer->line);
    }
    size_t start_line = lexer->line;
    // lexer->start is at the opening quote. advance() consumes it.
    // Column should be where the quote was.
    size_t start_col = lexer->column;
    advance(lexer); // Consume opening quote "

    while (peek(lexer) != L'"' && !is_at_end(lexer))
    {
        wchar_t c = peek(lexer);
        if (c == L'\\')
        {
            advance(lexer); // Consume '\'
            if (is_at_end(lexer)) break; // Unterminated escape at EOF
            wchar_t escaped_char = advance(lexer); // Consume char after '\'
            switch (escaped_char)
            {
            case L'n': append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\n'); break;
            case L't': append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\t'); break;
            case L'\\': append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\\'); break;
            case L'"': append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'"'); break;
            case L'r': append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\r'); break;
            case L'0': append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\0'); break;
            case L'u':
            {
                int value = scan_hex_escape(lexer, 4);
                if (value == -1)
                {
                    free(buffer);
                    BaaToken *error_token = make_error_token(lexer, L"تسلسل هروب يونيكود غير صالح (\\uXXXX) في السطر %zu، العمود %zu", start_line, start_col);
                    synchronize(lexer);
                    return error_token;
                }
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, (wchar_t)value);
                break;
            }
            default:
                free(buffer);
                // Show the backslash explicitly in the error message for clarity
                BaaToken *error_token = make_error_token(lexer, L"تسلسل هروب غير صالح '\\%lc' في سلسلة نصية (السطر %zu)", escaped_char, start_line);
                synchronize(lexer);
                return error_token;
            }
            if (buffer == NULL) return make_error_token(lexer, L"فشل في إعادة تخصيص ذاكرة لسلسلة نصية (السطر %zu)", start_line);
        }
        else
        {
            if (c == L'\n') { lexer->line++; lexer->column = 0; } // Track newlines inside string
            append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, c);
            if (buffer == NULL) return make_error_token(lexer, L"فشل في إعادة تخصيص ذاكرة لسلسلة نصية (السطر %zu)", start_line);
            advance(lexer);
        }
    }

    if (is_at_end(lexer) || peek(lexer) != L'"')
    {
        free(buffer);
        BaaToken *error_token = make_error_token(lexer, L"سلسلة نصية غير منتهية (بدأت في السطر %zu، العمود %zu)", start_line, start_col);
        if (!is_at_end(lexer)) synchronize(lexer); // Don't synchronize if already at EOF
        return error_token;
    }
    advance(lexer); // Consume closing quote "
    append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\0'); // Null-terminate the content
    if (buffer == NULL) return make_error_token(lexer, L"فشل في إعادة تخصيص الذاكرة عند إنهاء السلسلة النصية (بدأت في السطر %zu)", start_line);

    // For make_token, lexer->start should be at the opening quote, lexer->current should be after closing quote.
    // The external dispatcher (baa_lexer_next_token) sets lexer->start.
    // We've advanced lexer->current to be after the closing quote.
    BaaToken *token = malloc(sizeof(BaaToken));
    if (!token) { free(buffer); return NULL; } // Should ideally use make_token_with_lexeme

    token->type = BAA_TOKEN_STRING_LIT;
    token->lexeme = buffer; // Transfer ownership of buffer
    token->length = buffer_len - 1; // Don't count our internal null terminator
    token->line = start_line;
    token->column = start_col;
    // This token creation is a bit different from make_token as it uses an already prepared buffer.
    // Consider a make_token_from_buffer or similar if this pattern repeats.
    // For now, this is okay.
    return token;
}

BaaToken *scan_char_literal(BaaLexer *lexer)
{
    // lexer->start is at the opening quote, as set by baa_lexer_next_token
    // advance() in baa_lexer_next_token consumed the opening quote 'c'
    // So, lexer->current is currently at the character *inside* the literal, or the escape sequence.
    size_t start_line = lexer->line;
    // The main dispatcher (baa_lexer_next_token) calls advance() to get the opening quote,
    // then calls this function. So lexer->column is already past the opening quote.
    // We need the column of the opening quote itself.
    size_t start_col = lexer->column -1;

    wchar_t value_char;

    if (is_at_end(lexer))
    {
        return make_error_token(lexer, L"قيمة حرفية غير منتهية (EOF بعد \' )");
    }

    if (peek(lexer) == L'\\')
    {
        advance(lexer); // Consume '\'
        if (is_at_end(lexer))
        {
            return make_error_token(lexer, L"تسلسل هروب غير منته في قيمة حرفية (EOF بعد '\' في السطر %zu، العمود %zu)", start_line, start_col);
        }
        wchar_t escape = advance(lexer); // Consume char after '\'
        switch (escape)
        {
        case L'n': value_char = L'\n'; break;
        case L't': value_char = L'\t'; break;
        case L'\\': value_char = L'\\'; break;
        case L'\'': value_char = L'\''; break;
        case L'"': value_char = L'"'; break;
        case L'r': value_char = L'\r'; break;
        case L'0': value_char = L'\0'; break;
        case L'u':
        {
            int value = scan_hex_escape(lexer, 4);
            if (value == -1)
            {
                BaaToken *error_token = make_error_token(lexer, L"تسلسل هروب يونيكود غير صالح (\\uXXXX) في قيمة حرفية (بدأت في السطر %zu، العمود %zu)", start_line, start_col);
                synchronize(lexer);
                return error_token;
            }
            value_char = (wchar_t)value;
            break;
        }
        default:
            BaaToken *error_token = make_error_token(lexer, L"تسلسل هروب غير صالح '\%lc' في قيمة حرفية (بدأت في السطر %zu، العمود %zu)", escape, start_line, start_col);
            synchronize(lexer);
            return error_token;
        }
    }
    else
    {
        value_char = advance(lexer); // Consume the character content
        if (value_char == L'\n')
        {
            // Newlines are not allowed directly in char literals, must be escaped.
            BaaToken *error_token = make_error_token(lexer, L"سطر جديد غير مسموح به في قيمة حرفية (بدأت في السطر %zu، العمود %zu)", start_line, start_col);
            synchronize(lexer); // Synchronize as this is a recoverable error.
            return error_token;
        }
        if (value_char == L'\'') // e.g. '' (empty char literal)
        {
             BaaToken *error_token = make_error_token(lexer, L"قيمة حرفية فارغة ('') (بدأت في السطر %zu، العمود %zu)", start_line, start_col);
            synchronize(lexer);
            return error_token;
        }
    }

    if (peek(lexer) == L'\'')
    {
        advance(lexer); // Consume closing quote
        // The make_token function will use lexer->start and lexer->current.
        // lexer->start was at the opening quote.
        // lexer->current is now after the closing quote.
        return make_token(lexer, BAA_TOKEN_CHAR_LIT);
    }
    else
    {
        // Error: unterminated or too many characters
        if (is_at_end(lexer))
        {
            return make_error_token(lexer, L"قيمة حرفية غير منتهية (علامة اقتباس أحادية ' مفقودة، بدأت في السطر %zu، العمود %zu)", start_line, start_col);
        }
        else
        {
            // If not EOF and not closing quote, it implies too many characters before closing quote or other syntax error.
            BaaToken *error_token = make_error_token(lexer, L"قيمة حرفية غير صالحة (متعددة الأحرف أو علامة اقتباس مفقودة؟ بدأت في السطر %zu، العمود %zu)", start_line, start_col);
            synchronize(lexer); // Attempt to recover
            return error_token;
        }
    }
}

BaaToken *scan_multiline_string_literal(BaaLexer *lexer, size_t token_start_line, size_t token_start_col) {
    size_t buffer_cap = 128; // Start with a slightly larger buffer for multiline strings
    size_t buffer_len = 0;
    wchar_t *buffer = malloc(buffer_cap * sizeof(wchar_t));
    if (!buffer) {
        // Use the passed start_line for error reporting if possible, though lexer->line is current
        return make_error_token(lexer, L"فشل في تخصيص ذاكرة لسلسلة نصية متعددة الأسطر (بدأت في السطر %zu، العمود %zu)", token_start_line, token_start_col);
    }

    // When this function is called from baa_lexer_next_token:
    // lexer->start points to the *first* quote of the opening """.
    // The three opening quotes have already been consumed by advance() calls in baa_lexer_next_token.
    // So, lexer->current is positioned *after* the opening """.
    // token_start_line and token_start_col are the line/col of the *first* quote of """.

    while (true) {
        if (is_at_end(lexer)) {
            free(buffer);
            BaaToken *err_token = make_error_token(lexer, L"سلسلة نصية متعددة الأسطر غير منتهية (بدأت في السطر %zu، العمود %zu)", token_start_line, token_start_col);
            // No synchronize at EOF
            return err_token;
        }

        // Check for closing """
        if (peek(lexer) == L'"' &&
            lexer->current + 2 < lexer->source_length && // Ensure there are enough chars for """
            lexer->source[lexer->current + 1] == L'"' &&
            lexer->source[lexer->current + 2] == L'"') {
            advance(lexer); // Consume first " of closing """
            advance(lexer); // Consume second " of closing """
            advance(lexer); // Consume third " of closing """
            break; // End of multiline string
        }

        wchar_t c = peek(lexer);
        if (c == L'\\') {
            advance(lexer); // Consume '\'
            if (is_at_end(lexer)) { // Unterminated escape at EOF
                free(buffer);
                BaaToken *err_token = make_error_token(lexer, L"سلسلة نصية متعددة الأسطر غير منتهية (تسلسل هروب عند EOF، بدأت في السطر %zu، العمود %zu)", token_start_line, token_start_col);
                return err_token;
            }
            wchar_t escaped_char = advance(lexer); // Consume char after '\'
            switch (escaped_char) {
                case L'n': append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\n'); break;
                case L't': append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\t'); break;
                case L'\\': append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\\'); break;
                case L'"': append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'"'); break; // Escaped quote
                case L'r': append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\r'); break;
                case L'0': append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\0'); break;
                case L'u': {
                    int value = scan_hex_escape(lexer, 4);
                    if (value == -1) {
                        free(buffer);
                        BaaToken *err_token = make_error_token(lexer, L"تسلسل هروب يونيكود غير صالح (\\uXXXX) في سلسلة متعددة الأسطر (بدأت في السطر %zu، العمود %zu)", token_start_line, token_start_col);
                        synchronize(lexer);
                        return err_token;
                    }
                    append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, (wchar_t)value);
                    break;
                }
                default:
                    free(buffer);
                    BaaToken *err_token = make_error_token(lexer, L"تسلسل هروب غير صالح '\\%lc' في سلسلة متعددة الأسطر (بدأت في السطر %zu، العمود %zu)", escaped_char, token_start_line, token_start_col);
                    synchronize(lexer);
                    return err_token;
            }
            if (buffer == NULL) return make_error_token(lexer, L"فشل في إعادة تخصيص ذاكرة لسلسلة متعددة الأسطر (السطر %zu)", token_start_line);
        } else {
            // advance() handles line/column updates for newline
            append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, c);
            if (buffer == NULL) return make_error_token(lexer, L"فشل في إعادة تخصيص ذاكرة لسلسلة متعددة الأسطر (السطر %zu)", token_start_line);
            advance(lexer);
        }
    }

    append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\0'); // Null-terminate the content
    if (buffer == NULL) return make_error_token(lexer, L"فشل في إعادة تخصيص الذاكرة عند إنهاء السلسلة متعددة الأسطر (بدأت في السطر %zu)", token_start_line);

    BaaToken *token = malloc(sizeof(BaaToken));
    if (!token) { free(buffer); return NULL; }

    token->type = BAA_TOKEN_STRING_LIT;
    token->lexeme = buffer; // Transfer ownership
    token->length = buffer_len - 1; // Don't count our internal null terminator
    token->line = token_start_line;
    token->column = token_start_col;

    return token;
}

BaaToken *scan_raw_string_literal(BaaLexer *lexer, bool is_multiline, size_t token_start_line, size_t token_start_col) {
    size_t buffer_cap = 128;
    size_t buffer_len = 0;
    wchar_t *buffer = malloc(buffer_cap * sizeof(wchar_t));
    if (!buffer) {
        return make_error_token(lexer, L"فشل في تخصيص ذاكرة لسلسلة نصية خام (بدأت في السطر %zu، العمود %zu)", token_start_line, token_start_col);
    }

    // When this function is called:
    // lexer->start points to 'خ'.
    // The prefix ('خ' and opening quote(s)) have been consumed by the dispatcher.
    // lexer->current is positioned *after* the opening 'خ"' or 'خ"""'.
    // token_start_line and token_start_col refer to the position of 'خ'.

    if (is_multiline) {
        // Looking for closing """
        while (true) {
            if (is_at_end(lexer)) {
                free(buffer);
                BaaToken *err_token = make_error_token(lexer, L"سلسلة نصية خام متعددة الأسطر غير منتهية (بدأت في السطر %zu، العمود %zu)", token_start_line, token_start_col);
                return err_token;
            }

            if (peek(lexer) == L'"' &&
                lexer->current + 2 < lexer->source_length &&
                lexer->source[lexer->current + 1] == L'"' &&
                lexer->source[lexer->current + 2] == L'"') {
                advance(lexer); // Consume first " of closing """
                advance(lexer); // Consume second " of closing """
                advance(lexer); // Consume third " of closing """
                break;
            }
            // No escape sequence processing for raw strings
            append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, advance(lexer));
            if (buffer == NULL) return make_error_token(lexer, L"فشل في إعادة تخصيص ذاكرة لسلسلة خام متعددة الأسطر (السطر %zu)", token_start_line);
        }
    } else {
        // Looking for closing "
        while (peek(lexer) != L'"' && !is_at_end(lexer)) {
            if (peek(lexer) == L'\n') {
                // Newline not allowed in single-line raw string before closing quote
                free(buffer);
                // Error message should indicate that raw strings don't span lines unless multiline syntax is used.
                BaaToken *err_token = make_error_token(lexer, L"سلسلة نصية خام أحادية السطر غير منتهية قبل السطر الجديد (بدأت في السطر %zu، العمود %zu)", token_start_line, token_start_col);
                // Do not consume the newline, let synchronize handle it or next token be on new line.
                synchronize(lexer); // Try to recover
                return err_token;
            }
            // No escape sequence processing for raw strings
            append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, advance(lexer));
            if (buffer == NULL) return make_error_token(lexer, L"فشل في إعادة تخصيص ذاكرة لسلسلة خام (السطر %zu)", token_start_line);
        }

        if (is_at_end(lexer) || peek(lexer) != L'"') { // Could be EOF or some other char if newline was hit and error generated
            free(buffer);
            BaaToken *err_token = make_error_token(lexer, L"سلسلة نصية خام أحادية السطر غير منتهية (بدأت في السطر %zu، العمود %zu)", token_start_line, token_start_col);
            // No synchronize if at EOF, otherwise synchronize might have been called if newline was hit
            if (!is_at_end(lexer) && peek(lexer) != L'\n') synchronize(lexer);
            return err_token;
        }
        advance(lexer); // Consume closing quote "
    }

    append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\0'); // Null-terminate
    if (buffer == NULL) return make_error_token(lexer, L"فشل في إعادة تخصيص الذاكرة عند إنهاء السلسلة الخام (بدأت في السطر %zu)", token_start_line);

    BaaToken *token = malloc(sizeof(BaaToken));
    if (!token) { free(buffer); return NULL; }

    token->type = BAA_TOKEN_STRING_LIT; // Reusing BAA_TOKEN_STRING_LIT
    token->lexeme = buffer;
    token->length = buffer_len - 1;
    token->line = token_start_line;
    token->column = token_start_col;

    return token;
}
