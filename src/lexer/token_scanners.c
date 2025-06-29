#include "baa/lexer/token_scanners.h" // For BaaToken and BaaLexer types, and scan_* declarations
#include "lexer_internal.h" // For make_token, peek, advance etc.
#include <stdlib.h>                   // For malloc, realloc, free
#include <string.h>                   // For wcslen, wcsncpy, wcscpy
#include <wctype.h>                   // For iswalnum

// This file contains implementations for:
// BaaToken *scan_identifier(BaaLexer *lexer);
// BaaToken *scan_number(BaaLexer *lexer);
// BaaToken *scan_string(BaaLexer *lexer);
// BaaToken *scan_char_literal(BaaLexer *lexer);
// BaaToken *scan_multiline_string_literal(BaaLexer *lexer);

// The 'keywords' array is now declared as extern in lexer_internal.h
// and defined in lexer.c. No local definition needed here.

// NUM_KEYWORDS is now declared as extern const size_t in lexer_internal.h

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
    // Use the extern 'keywords' array and NUM_KEYWORDS
    for (size_t i = 0; i < NUM_KEYWORDS; i++) // Using NUM_KEYWORDS
    {
        if (wcslen(keywords[i].keyword) == length &&
            wcsncmp(&lexer->source[lexer->start], keywords[i].keyword, length) == 0)
        {
            return make_token(lexer, keywords[i].token);
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

    // Handle numbers starting with a decimal point (e.g., .123)
    if (first_char_of_number == L'.' || first_char_of_number == 0x066B) // Arabic decimal separator
    {
        // baa_lexer_next_token ensures a digit follows, so no need to check peek_next here for validity.
        is_float = true;
        advance(lexer);                      // Consume '.' or '٫'
        number_start_index = lexer->current; // Digits start after the dot
    }
    else if (first_char_of_number == L'0')
    {
        wchar_t next = peek_next(lexer);
        if (next == L'x' || next == L'X')
        {
            base_prefix = 'x';
            is_valid_digit = is_baa_hex_digit;
            advance(lexer);                      // Consume '0'
            advance(lexer);                      // Consume 'x' or 'X'
            number_start_index = lexer->current; // Digits start after prefix
            wchar_t char_after_prefix = peek(lexer);
            // For hex literals, a hex digit OR a decimal point is valid after the prefix.
            if (!is_valid_digit(char_after_prefix) && char_after_prefix != L'.' && char_after_prefix != 0x066B)
            {
                BaaToken *error_token = make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR_INVALID_NUMBER,
                    1005, "number",
                    L"أضف رقم سداسي عشري صالح بعد 0x",
                    L"عدد سداسي عشر غير صالح: يجب أن يتبع البادئة 0x/0X رقم سداسي عشري أو فاصلة عشرية (السطر %zu، العمود %zu)",
                    lexer->line, lexer->column);
                synchronize(lexer);
                return error_token;
            }
        }
        else if (next == L'b' || next == L'B')
        {
            base_prefix = 'b';
            is_valid_digit = is_baa_bin_digit;
            advance(lexer);                      // Consume '0'
            advance(lexer);                      // Consume 'b' or 'B'
            number_start_index = lexer->current; // Digits start after prefix
            if (!is_valid_digit(peek(lexer)))
            {
                BaaToken *error_token = make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR_INVALID_NUMBER,
                    1005, "number",
                    L"أضف رقم ثنائي صالح (0 أو 1) بعد 0b",
                    L"عدد ثنائي غير صالح: يجب أن يتبع البادئة 0b/0B رقم ثنائي واحد على الأقل (السطر %zu، العمود %zu)",
                    lexer->line, lexer->column);
                synchronize(lexer);
                return error_token;
            }
        }
        else
        {
            // It's a number starting with '0' but not '0x' or '0b' (e.g., "0", "0.5", "007")
            advance(lexer);                      // Consume the '0'
            number_start_index = lexer->current; // Next char is start of post-'0' digits or dot
        }
    }
    else
    {
        // Starts with a non-'0' digit
        advance(lexer); // Consume the first digit
        number_start_index = lexer->current;
    }

    bool last_char_was_underscore = false;
    while (is_valid_digit(peek(lexer)) || peek(lexer) == L'_')
    {
        // wchar_t char_in_loop = peek(lexer);
        // fwprintf(stderr, L"DEBUG scan_number int_loop: char='%lc'(%u), is_valid_digit_fn=%d, is_underscore=%d\n",
        //     char_in_loop, (unsigned int)char_in_loop, is_valid_digit(char_in_loop), char_in_loop == L'_');

        if (peek(lexer) == L'_')
        {
            // Error if previous char was also underscore OR if this underscore is the first char after a prefix.
            if (last_char_was_underscore || (base_prefix != 0 && lexer->current == number_start_index))
            {
                BaaToken *error_token = make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR_INVALID_NUMBER,
                    1005, "number",
                    L"استخدم الشرطة السفلية لفصل الأرقام فقط، وليس في البداية أو متتالية",
                    L"شرطة سفلية غير صالحة في العدد: متتالية أو مباشرة بعد البادئة (السطر %zu، العمود %zu).",
                    lexer->line, lexer->column);
                synchronize(lexer);
                return error_token;
            }
            last_char_was_underscore = true;
        }
        else // It's a digit
        {
            if (!is_valid_digit(peek(lexer)))
                break;
            last_char_was_underscore = false;
        }
        advance(lexer);
    }

    if (last_char_was_underscore)
    {
        BaaToken *error_token = make_specific_error_token(lexer,
            BAA_TOKEN_ERROR_INVALID_NUMBER,
            1005, "number",
            L"احذف الشرطة السفلية من نهاية الرقم",
            L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تكون في نهاية الرقم.",
            lexer->line, lexer->column - 1);
        synchronize(lexer);
        return error_token;
    }

    wchar_t current_peek = peek(lexer);
    // Handle fractional part for both decimal (base 0) and hexadecimal (base 'x') numbers.
    if ((base_prefix == 0 || base_prefix == 'x') && (current_peek == L'.' || current_peek == 0x066B))
    {
        bool (*is_digit_for_frac)(wchar_t) = (base_prefix == 'x') ? is_baa_hex_digit : is_baa_digit;

        wchar_t next_peek = peek_next(lexer);
        if (next_peek == L'_')
        {
            BaaToken *error_token = make_specific_error_token(lexer,
                BAA_TOKEN_ERROR_INVALID_NUMBER,
                1005, "number",
                L"أضف رقم بعد الفاصلة العشرية قبل الشرطة السفلية",
                L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تتبع الفاصلة العشرية مباشرة.",
                lexer->line, lexer->column + 1);
            synchronize(lexer);
            return error_token;
        }

        // A digit must follow the decimal point.
        if (is_digit_for_frac(next_peek))
        {
            is_float = true;
            advance(lexer); // Consume '.' or '٫'
            last_char_was_underscore = false;
            while (is_digit_for_frac(peek(lexer)) || peek(lexer) == L'_')
            {
                if (peek(lexer) == L'_')
                {
                    if (last_char_was_underscore)
                    {
                        BaaToken *error_token = make_specific_error_token(lexer,
                            BAA_TOKEN_ERROR_INVALID_NUMBER,
                            1005, "number",
                            L"استخدم شرطة سفلية واحدة فقط لفصل الأرقام",
                            L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تكون متتالية.",
                            lexer->line, lexer->column);
                        synchronize(lexer);
                        return error_token;
                    }
                    last_char_was_underscore = true;
                }
                else
                {
                    if (!is_digit_for_frac(peek(lexer)))
                        break;
                    last_char_was_underscore = false;
                }
                advance(lexer);
            }
            if (last_char_was_underscore)
            {
                BaaToken *error_token = make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR_INVALID_NUMBER,
                    1005, "number",
                    L"احذف الشرطة السفلية من نهاية الجزء الكسري",
                    L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تكون في نهاية الجزء الكسري.",
                    lexer->line, lexer->column - 1);
                synchronize(lexer);
                return error_token;
            }
        }
    }

    current_peek = peek(lexer);
    /**
     * @brief Arabic exponent marker ALIF WITH HAMZA ABOVE (U+0623).
     * Replaces 'e'/'E' for scientific notation in floating-point numbers.
     */
    const wchar_t ARABIC_EXPONENT_MARKER_ALIF_HAMZA = L'أ'; // U+0623

    // Exponent part can follow decimal or hexadecimal floats.
    if (current_peek == ARABIC_EXPONENT_MARKER_ALIF_HAMZA)
    {
        wchar_t next_peek_exp = peek_next(lexer);
        if (next_peek_exp == L'_')
        {
            BaaToken *error_token = make_specific_error_token(lexer,
                BAA_TOKEN_ERROR_INVALID_NUMBER,
                1005, "number",
                L"أضف رقم بعد علامة الأس 'أ' قبل الشرطة السفلية",
                L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تتبع علامة الأس 'أ' مباشرة.",
                lexer->line, lexer->column + 1);
            synchronize(lexer);
            return error_token;
        }
        bool has_exponent_part = false;
        int sign_offset = 0;
        if (next_peek_exp == L'+' || next_peek_exp == L'-')
        {
            sign_offset = 1;
            // Ensure there's a character after the sign before peeking lexer->source[lexer->current + 2]
            if (lexer->current + 2 < lexer->source_length)
            {
                wchar_t after_sign_peek = lexer->source[lexer->current + 2];
                if (after_sign_peek == L'_')
                {
                    BaaToken *error_token = make_specific_error_token(lexer,
                        BAA_TOKEN_ERROR_INVALID_NUMBER,
                        1005, "number",
                        L"أضف رقم بعد علامة الأس (+/-) قبل الشرطة السفلية",
                        L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تتبع علامة الأس (+/-) في الأس 'أ' مباشرة.",
                        lexer->line, lexer->column + 2);
                    synchronize(lexer);
                    return error_token;
                }
                if (is_baa_digit(after_sign_peek))
                    has_exponent_part = true;
            }
            else
            { // EOF after sign
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
            advance(lexer); // Consume 'أ'
            if (sign_offset == 1)
                advance(lexer); // Consume '+' or '-'
            if (!is_baa_digit(peek(lexer)))
            {
                BaaToken *error_token = make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR_INVALID_NUMBER,
                    1005, "number",
                    L"أضف رقم صالح بعد علامة الأس 'أ'",
                    L"تنسيق أس غير صالح بعد 'أ' (السطر %zu، العمود %zu)",
                    lexer->line, lexer->column);
                synchronize(lexer);
                return error_token;
            }
            last_char_was_underscore = false;
            while (is_baa_digit(peek(lexer)) || peek(lexer) == L'_')
            {
                // wchar_t char_in_exp_loop = peek(lexer);
                // fwprintf(stderr, L"DEBUG scan_number exp_loop: char='%lc'(%u), is_baa_digit=%d, is_underscore=%d\n",
                //     char_in_exp_loop, (unsigned int)char_in_exp_loop, is_baa_digit(char_in_exp_loop), char_in_exp_loop == L'_');
                if (peek(lexer) == L'_')
                {
                    if (last_char_was_underscore)
                    {
                        BaaToken *error_token = make_specific_error_token(lexer,
                            BAA_TOKEN_ERROR_INVALID_NUMBER,
                            1005, "number",
                            L"استخدم شرطة سفلية واحدة فقط لفصل الأرقام في الأس",
                            L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تكون متتالية في الأس.",
                            lexer->line, lexer->column);
                        synchronize(lexer);
                        return error_token;
                    }
                    last_char_was_underscore = true;
                }
                else
                {
                    if (!is_baa_digit(peek(lexer)))
                        break;
                    last_char_was_underscore = false;
                }
                advance(lexer);
            }
            if (last_char_was_underscore)
            {
                BaaToken *error_token = make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR_INVALID_NUMBER,
                    1005, "number",
                    L"احذف الشرطة السفلية من نهاية الأس",
                    L"شرطة سفلية غير صالحة في العدد (السطر %zu، العمود %zu): لا يمكن أن تكون في نهاية الأس.",
                    lexer->line, lexer->column - 1);
                synchronize(lexer);
                return error_token;
            }
        }
    }


    if (!is_float)
    {                                      // Integer suffixes are only for integer literals
        const wchar_t GHAIN_SUFFIX = L'غ'; // U+063A
        const wchar_t TAH_SUFFIX = L'ط';   // U+0637

        bool suffix_seen_ghain = false;
        bool suffix_seen_tah = false;     // True if one 'ط' is seen
        bool suffix_seen_tah_tah = false; // True if 'طط' is seen

        // Try to parse up to 3 suffix characters (e.g., غطط or ططغ).
        for (int i = 0; i < 3; ++i)
        {
            wchar_t current_suffix_char = peek(lexer);

            if (current_suffix_char == GHAIN_SUFFIX)
            {
                if (suffix_seen_ghain)
                {
                    // Already processed 'غ', cannot have it twice - this is an error
                    BaaToken *error_token = make_specific_error_token(lexer,
                        BAA_TOKEN_ERROR_INVALID_SUFFIX,
                        1006, "number",
                        L"استخدم لاحقة غ واحدة فقط للأعداد غير المُوقعة",
                        L"لاحقة رقم غير صالحة: لا يمكن استخدام 'غ' أكثر من مرة (السطر %zu، العمود %zu)",
                        lexer->line, lexer->column);
                    synchronize(lexer);
                    return error_token;
                }
                suffix_seen_ghain = true;
                advance(lexer);
            }
            else if (current_suffix_char == TAH_SUFFIX)
            {
                if (suffix_seen_tah_tah)
                {
                    // Already processed 'طط', cannot have more 'ط' - this is an error
                    BaaToken *error_token = make_specific_error_token(lexer,
                        BAA_TOKEN_ERROR_INVALID_SUFFIX,
                        1006, "number",
                        L"استخدم لاحقات صالحة: ط (long) أو طط (long long)",
                        L"لاحقة رقم غير صالحة: لا يمكن استخدام أكثر من 'طط' (السطر %zu، العمود %zu)",
                        lexer->line, lexer->column);
                    synchronize(lexer);
                    return error_token;
                }
                if (suffix_seen_tah)
                {
                    // This is the second 'ط', forming 'طط'.
                    suffix_seen_tah = false; // No longer just 'ط'
                    suffix_seen_tah_tah = true;
                    advance(lexer);
                }
                else
                {
                    // This is the first 'ط'.
                    suffix_seen_tah = true;
                    advance(lexer);
                }
            }
            else
            {
                // Check if it's an invalid suffix character that looks like it might be intended as a suffix
                if (current_suffix_char == L'ح') // Float suffix on integer
                {
                    BaaToken *error_token = make_specific_error_token(lexer,
                        BAA_TOKEN_ERROR_INVALID_SUFFIX,
                        1006, "number",
                        L"لاحقة 'ح' مخصصة للأعداد العشرية فقط",
                        L"لاحقة رقم غير صالحة: لا يمكن استخدام 'ح' مع الأعداد الصحيحة (السطر %zu، العمود %zu)",
                        lexer->line, lexer->column);
                    synchronize(lexer);
                    return error_token;
                }
                // Not a recognized integer suffix character or sequence.
                break;
            }
        }

        // Check for any remaining invalid suffix characters after valid parsing
        wchar_t next_char = peek(lexer);
        if (next_char == GHAIN_SUFFIX || next_char == TAH_SUFFIX || next_char == L'ح')
        {
            // There are more suffix characters that couldn't be parsed - invalid combination
            BaaToken *error_token = make_specific_error_token(lexer,
                BAA_TOKEN_ERROR_INVALID_SUFFIX,
                1006, "number",
                L"استخدم لاحقات صالحة: غ (unsigned)، ط (long)، طط (long long)، ح (float)",
                L"لاحقة رقم غير صالحة: تركيبة لاحقات غير مدعومة (السطر %zu، العمود %zu)",
                lexer->line, lexer->column);
            synchronize(lexer);
            return error_token;
        }

        // The lexeme will now include these suffixes.
        // The actual interpretation of these (unsigned, long, long long)
        // will be handled by the parser or semantic analyzer.
    }
    else if (is_float) // It's a float, check for float suffix
    {                                                 /**
        * @brief Arabic float suffix HAH (U+062D).
        * Indicates that the literal is of a floating-point type.
        * Future: Could distinguish between float/double if more suffixes are added.
        */
        const wchar_t ARABIC_FLOAT_SUFFIX_HAH = L'ح'; // U+062D

        if (peek(lexer) == ARABIC_FLOAT_SUFFIX_HAH)
        {
            // Consume the 'ح' suffix. It becomes part of the BAA_TOKEN_FLOAT_LIT lexeme.
            advance(lexer);

            // Check for invalid additional suffixes after 'ح'
            wchar_t next_char = peek(lexer);
            if (next_char == L'غ' || next_char == L'ط' || next_char == ARABIC_FLOAT_SUFFIX_HAH)
            {
                BaaToken *error_token = make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR_INVALID_SUFFIX,
                    1006, "number",
                    L"لاحقة 'ح' يجب أن تكون الأخيرة في الأعداد العشرية",
                    L"لاحقة رقم غير صالحة: لا يمكن إضافة لاحقات بعد 'ح' (السطر %zu، العمود %zu)",
                    lexer->line, lexer->column);
                synchronize(lexer);
                return error_token;
            }
        }
        else
        {
            // Check for invalid integer suffixes on float numbers
            wchar_t next_char = peek(lexer);
            if (next_char == L'غ' || next_char == L'ط')
            {
                BaaToken *error_token = make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR_INVALID_SUFFIX,
                    1006, "number",
                    L"استخدم لاحقة 'ح' للأعداد العشرية",
                    L"لاحقة رقم غير صالحة: لاحقات الأعداد الصحيحة غير مدعومة للأعداد العشرية (السطر %zu، العمود %zu)",
                    lexer->line, lexer->column);
                synchronize(lexer);
                return error_token;
            }
        }
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
        return make_specific_error_token(lexer,
            BAA_TOKEN_ERROR,
            9001, "memory",
            L"تحقق من توفر ذاكرة كافية في النظام",
            L"فشل في تخصيص ذاكرة لسلسلة نصية (السطر %zu)",
            lexer->line);
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
            if (is_at_end(lexer))
                break;                                    // Unterminated escape at EOF
            wchar_t baa_escape_char_key = advance(lexer); // Consume the Arabic escape key char
            switch (baa_escape_char_key)
            {
            case L'س': // Baa Newline
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\n');
                break;
            case L'م': // Baa Tab
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\t');
                break;
            case L'ر': // Baa Carriage Return
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\r');
                break;
            case L'ص': // Baa Null Character
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\0');
                break;
            case L'\\': // Baa Backslash (remains the same)
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\\');
                break;
            case L'"': // Baa Double Quote (remains the same)
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'"');
                break;
            case L'\'': // Baa Single Quote (can appear in strings)
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\'');
                break;
            case L'ي': // Baa Unicode escape \يXXXX (replaces \uXXXX)
            {
                int value = scan_hex_escape(lexer, 4);
                if (value == -1)
                {
                    free(buffer);
                    BaaToken *error_token = make_specific_error_token(lexer,
                        BAA_TOKEN_ERROR_INVALID_ESCAPE,
                        1002, "escape",
                        L"استخدم تسلسل هروب صالح مثل \\س أو \\م أو \\يXXXX",
                        L"تسلسل هروب يونيكود غير صالح (\\يXXXX) في سلسلة نصية (بدأت في السطر %zu، العمود %zu)",
                        start_line, start_col);
                    synchronize(lexer);
                    return error_token;
                }
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, (wchar_t)value);
                break;
            }
            case L'ه': // Baa Hex byte escape \هـHH
            {
                if (peek(lexer) == L'ـ')
                {                                             // TATWEEL U+0640
                    advance(lexer);                           // Consume TATWEEL 'ـ'
                    int byte_val = scan_hex_escape(lexer, 2); // scan_hex_escape consumes HH
                    if (byte_val == -1 || byte_val > 0xFF)
                    { // byte_val must be 0-255
                        free(buffer);
                        return make_specific_error_token(lexer,
                            BAA_TOKEN_ERROR_INVALID_ESCAPE,
                            1002, "escape",
                            L"استخدم تسلسل هروب صالح مثل \\هـ01 إلى \\هـFF",
                            L"تسلسل هروب سداسي عشري '\\هـHH' غير صالح في سلسلة نصية (بدأت في السطر %zu، العمود %zu)",
                            start_line, start_col);
                    }
                    append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, (wchar_t)byte_val);
                }
                else
                {
                    free(buffer);
                    return make_specific_error_token(lexer,
                        BAA_TOKEN_ERROR_INVALID_ESCAPE,
                        1002, "escape",
                        L"استخدم \\هـHH للهروب السداسي عشري",
                        L"تسلسل هروب غير صالح: '\\ه' يجب أن يتبعها 'ـ' في سلسلة نصية (بدأت في السطر %zu، العمود %zu)",
                        start_line, start_col);
                }
                break;
            }
            default:
                free(buffer);
                BaaToken *error_token = make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR_INVALID_ESCAPE,
                    1002, "escape",
                    L"استخدم تسلسل هروب صالح مثل \\س أو \\م أو \\يXXXX",
                    L"تسلسل هروب غير صالح '\\%lc' في سلسلة نصية (بدأت في السطر %zu، العمود %zu)",
                    baa_escape_char_key, start_line, start_col);
                synchronize(lexer);
                return error_token;
            }
            if (buffer == NULL)
                return make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR,
                    9001, "memory",
                    L"تحقق من توفر ذاكرة كافية في النظام",
                    L"فشل في إعادة تخصيص ذاكرة لسلسلة نصية (السطر %zu)",
                    start_line);
        }
        else
        {
            if (c == L'\n')
            {
                lexer->line++;
                lexer->column = 0;
            } // Track newlines inside string
            append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, c);
            if (buffer == NULL)
                return make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR,
                    9001, "memory",
                    L"تحقق من توفر ذاكرة كافية في النظام",
                    L"فشل في إعادة تخصيص ذاكرة لسلسلة نصية (السطر %zu)",
                    start_line);
            advance(lexer);
        }
    }

    if (is_at_end(lexer) || peek(lexer) != L'"')
    {
        free(buffer);
        BaaToken *error_token = make_specific_error_token(lexer,
            BAA_TOKEN_ERROR_UNTERMINATED_STRING,
            1001, "string",
            L"أضف علامة اقتباس مزدوجة \" في نهاية السلسلة",
            L"سلسلة نصية غير منتهية (بدأت في السطر %zu، العمود %zu)",
            start_line, start_col);
        if (!is_at_end(lexer))
            synchronize(lexer); // Don't synchronize if already at EOF
        return error_token;
    }
    advance(lexer);                                                  // Consume closing quote "
    append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\0'); // Null-terminate the content
    if (buffer == NULL)
        return make_specific_error_token(lexer,
            BAA_TOKEN_ERROR,
            9001, "memory",
            L"تحقق من توفر ذاكرة كافية في النظام",
            L"فشل في إعادة تخصيص الذاكرة عند إنهاء السلسلة النصية (بدأت في السطر %zu)",
            start_line);

    // For make_token, lexer->start should be at the opening quote, lexer->current should be after closing quote.
    // The external dispatcher (baa_lexer_next_token) sets lexer->start.
    // We've advanced lexer->current to be after the closing quote.
    BaaToken *token = malloc(sizeof(BaaToken));
    if (!token)
    {
        free(buffer);
        return NULL;
    } // Should ideally use make_token_with_lexeme

    token->type = BAA_TOKEN_STRING_LIT;
    token->lexeme = buffer;         // Transfer ownership of buffer
    token->length = buffer_len - 1; // Don't count our internal null terminator
    token->line = start_line;
    token->column = start_col;

    // Initialize span (enhanced source location)
    token->span.start_line = start_line;
    token->span.start_column = start_col;
    token->span.end_line = lexer->line;
    token->span.end_column = lexer->column;
    token->span.start_offset = lexer->start;
    token->span.end_offset = lexer->current;

    // Initialize error context to NULL for non-error tokens
    token->error = NULL;

    return token;
}

// Scans a documentation comment /** ... */
// Assumes the initial /** has already been consumed.
BaaToken *scan_doc_comment(BaaLexer *lexer, size_t token_start_line, size_t token_start_col)
{
    size_t buffer_cap = 128;
    size_t buffer_len = 0;
    wchar_t *buffer = malloc(buffer_cap * sizeof(wchar_t));
    if (!buffer)
    {
        return make_specific_error_token(lexer,
            BAA_TOKEN_ERROR,
            9001, "memory",
            L"تحقق من توفر ذاكرة كافية في النظام",
            L"فشل في تخصيص ذاكرة لتعليق التوثيق (بدأ في السطر %zu، العمود %zu)",
            token_start_line, token_start_col);
    }

    // When called, lexer->current is positioned *after* the opening /**
    // token_start_line/col refer to the position of the initial '/'.

    while (true)
    {
        if (is_at_end(lexer))
        {
            free(buffer);
            BaaToken *err_token = make_specific_error_token(lexer,
                BAA_TOKEN_ERROR_UNTERMINATED_COMMENT,
                1007, "comment",
                L"أضف */ لإنهاء تعليق التوثيق",
                L"تعليق توثيق غير منتهٍ (بدأ في السطر %zu، العمود %zu)",
                token_start_line, token_start_col);
            return err_token;
        }

        // Check for closing */
        if (peek(lexer) == L'*' && peek_next(lexer) == L'/')
        {
            advance(lexer); // Consume '*'
            advance(lexer); // Consume '/'
            break;          // End of doc comment
        }

        // Append the character to the buffer
        // advance() handles line/column updates for newline
        wchar_t c = advance(lexer);
        append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, c);
        if (buffer == NULL)
        { // Check if realloc failed in append_char_to_buffer
            // Error token creation might fail too, but try
            return make_specific_error_token(lexer,
                BAA_TOKEN_ERROR,
                9001, "memory",
                L"تحقق من توفر ذاكرة كافية في النظام",
                L"فشل في إعادة تخصيص الذاكرة لتعليق التوثيق (السطر %zu)",
                token_start_line);
        }
    }

    append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\0'); // Null-terminate the content
    if (buffer == NULL)
    {
        return make_specific_error_token(lexer,
            BAA_TOKEN_ERROR,
            9001, "memory",
            L"تحقق من توفر ذاكرة كافية في النظام",
            L"فشل في إعادة تخصيص الذاكرة عند إنهاء تعليق التوثيق (بدأ في السطر %zu)",
            token_start_line);
    }

    BaaToken *token = malloc(sizeof(BaaToken));
    if (!token)
    {
        free(buffer);
        return NULL;
    }

    token->type = BAA_TOKEN_DOC_COMMENT; // Use the new token type
    token->lexeme = buffer;              // Transfer ownership
    token->length = buffer_len - 1;      // Don't count our internal null terminator
    token->line = token_start_line;
    token->column = token_start_col;

    // Initialize span (enhanced source location)
    token->span.start_line = token_start_line;
    token->span.start_column = token_start_col;
    token->span.end_line = lexer->line;
    token->span.end_column = lexer->column;
    token->span.start_offset = lexer->start;
    token->span.end_offset = lexer->current;

    // Initialize error context to NULL for non-error tokens
    token->error = NULL;

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
    size_t start_col = lexer->column - 1;

    wchar_t value_char;

    if (is_at_end(lexer))
    {
        return make_specific_error_token(lexer,
            BAA_TOKEN_ERROR_UNTERMINATED_CHAR,
            1003, "character",
            L"أضف علامة اقتباس مفردة ' في نهاية المحرف",
            L"قيمة حرفية غير منتهية (EOF بعد \' )");
    }

    if (peek(lexer) == L'\\')
    {
        advance(lexer); // Consume '\'
        if (is_at_end(lexer))
        {
            return make_specific_error_token(lexer,
                BAA_TOKEN_ERROR_UNTERMINATED_CHAR,
                1003, "character",
                L"أضف علامة اقتباس مفردة ' في نهاية المحرف",
                L"تسلسل هروب غير منته في قيمة حرفية (EOF بعد '\' في السطر %zu، العمود %zu)",
                start_line, start_col);
        }
        wchar_t baa_escape_char_key = advance(lexer); // Consume the Arabic escape key char
        switch (baa_escape_char_key)
        {
        case L'س': // Baa Newline
            value_char = L'\n';
            break;
        case L'م': // Baa Tab
            value_char = L'\t';
            break;
        case L'ر': // Baa Carriage Return
            value_char = L'\r';
            break;
        case L'ص': // Baa Null Character
            value_char = L'\0';
            break;
        case L'\\': // Baa Backslash
            value_char = L'\\';
            break;
        case L'\'': // Baa Single Quote
            value_char = L'\'';
            break;
        case L'"': // Baa Double Quote (can be escaped in char literal, though unusual)
            value_char = L'"';
            break;
        case L'ي': // Baa Unicode escape \يXXXX
        {
            int value = scan_hex_escape(lexer, 4);
            if (value == -1)
            {
                BaaToken *error_token = make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR_INVALID_ESCAPE,
                    1002, "escape",
                    L"استخدم تسلسل هروب صالح مثل \\س أو \\م أو \\يXXXX",
                    L"تسلسل هروب يونيكود غير صالح (\\يXXXX) في قيمة حرفية (بدأت في السطر %zu، العمود %zu)",
                    start_line, start_col);
                synchronize(lexer);
                return error_token;
            }
            value_char = (wchar_t)value;
            break;
        }
        case L'ه': // Baa Hex byte escape \هـHH
            if (peek(lexer) == L'ـ')
            {                   // TATWEEL U+0640
                advance(lexer); // Consume 'ـ'
                int byte_val = scan_hex_escape(lexer, 2);
                if (byte_val == -1 || byte_val > 0xFF)
                {
                    return make_specific_error_token(lexer,
                        BAA_TOKEN_ERROR_INVALID_ESCAPE,
                        1002, "escape",
                        L"استخدم تسلسل هروب صالح مثل \\هـ01 إلى \\هـFF",
                        L"تسلسل هروب سداسي عشري '\\هـHH' غير صالح في قيمة حرفية (بدأت في السطر %zu، العمود %zu)",
                        start_line, start_col);
                }
                value_char = (wchar_t)byte_val;
            }
            else
            {
                return make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR_INVALID_ESCAPE,
                    1002, "escape",
                    L"استخدم \\هـHH للهروب السداسي عشري",
                    L"تسلسل هروب غير صالح: '\\ه' يجب أن يتبعها 'ـ' في قيمة حرفية (بدأت في السطر %zu، العمود %zu)",
                    start_line, start_col);
            }
            break;
        default:
            BaaToken *error_token = make_specific_error_token(lexer,
                BAA_TOKEN_ERROR_INVALID_ESCAPE,
                1002, "escape",
                L"استخدم تسلسل هروب صالح مثل \\س أو \\م أو \\يXXXX",
                L"تسلسل هروب غير صالح '\\%lc' في قيمة حرفية (بدأت في السطر %zu، العمود %zu)",
                baa_escape_char_key, start_line, start_col);
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
            BaaToken *error_token = make_specific_error_token(lexer,
                BAA_TOKEN_ERROR_INVALID_CHARACTER,
                1004, "character",
                L"استخدم تسلسل هروب \\س للسطر الجديد",
                L"سطر جديد غير مسموح به في قيمة حرفية (بدأت في السطر %zu، العمود %zu)",
                start_line, start_col);
            synchronize(lexer); // Synchronize as this is a recoverable error.
            return error_token;
        }
        if (value_char == L'\'') // e.g. '' (empty char literal)
        {
            BaaToken *error_token = make_specific_error_token(lexer,
                BAA_TOKEN_ERROR_INVALID_CHARACTER,
                1004, "character",
                L"أضف محرف بين علامتي الاقتباس المفردتين",
                L"قيمة حرفية فارغة ('') (بدأت في السطر %zu، العمود %zu)",
                start_line, start_col);
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
            return make_specific_error_token(lexer,
                BAA_TOKEN_ERROR_UNTERMINATED_CHAR,
                1003, "character",
                L"أضف علامة اقتباس مفردة ' في نهاية المحرف",
                L"قيمة حرفية غير منتهية (علامة اقتباس أحادية ' مفقودة، بدأت في السطر %zu، العمود %zu)",
                start_line, start_col);
        }
        else
        {
            // If not EOF and not closing quote, it implies too many characters before closing quote or other syntax error.
            BaaToken *error_token = make_specific_error_token(lexer,
                BAA_TOKEN_ERROR_INVALID_CHARACTER,
                1004, "character",
                L"استخدم محرف واحد فقط بين علامتي الاقتباس المفردتين",
                L"قيمة حرفية غير صالحة (متعددة الأحرف أو علامة اقتباس مفقودة؟ بدأت في السطر %zu، العمود %zu)",
                start_line, start_col);
            synchronize(lexer); // Attempt to recover
            return error_token;
        }
    }
}

BaaToken *scan_multiline_string_literal(BaaLexer *lexer, size_t token_start_line, size_t token_start_col)
{
    size_t buffer_cap = 128; // Start with a slightly larger buffer for multiline strings
    size_t buffer_len = 0;
    wchar_t *buffer = malloc(buffer_cap * sizeof(wchar_t));
    if (!buffer)
    {
        // Use the passed start_line for error reporting if possible, though lexer->line is current
        return make_specific_error_token(lexer,
            BAA_TOKEN_ERROR,
            9001, "memory",
            L"تحقق من توفر ذاكرة كافية في النظام",
            L"فشل في تخصيص ذاكرة لسلسلة نصية متعددة الأسطر (بدأت في السطر %zu، العمود %zu)",
            token_start_line, token_start_col);
    }

    // When this function is called from baa_lexer_next_token:
    // lexer->start points to the *first* quote of the opening """.
    // The three opening quotes have already been consumed by advance() calls in baa_lexer_next_token.
    // So, lexer->current is positioned *after* the opening """.
    // token_start_line and token_start_col are the line/col of the *first* quote of """.

    while (true)
    {
        if (is_at_end(lexer))
        {
            free(buffer);
            BaaToken *err_token = make_specific_error_token(lexer,
                BAA_TOKEN_ERROR_UNTERMINATED_STRING,
                1001, "string",
                L"أضف علامة اقتباس مزدوجة ثلاثية \"\"\" في نهاية السلسلة",
                L"سلسلة نصية متعددة الأسطر غير منتهية (بدأت في السطر %zu، العمود %zu)",
                token_start_line, token_start_col);
            // No synchronize at EOF
            return err_token;
        }

        // Check for closing """
        if (peek(lexer) == L'"' &&
            lexer->current + 2 < lexer->source_length && // Ensure there are enough chars for """
            lexer->source[lexer->current + 1] == L'"' &&
            lexer->source[lexer->current + 2] == L'"')
        {
            advance(lexer); // Consume first " of closing """
            advance(lexer); // Consume second " of closing """
            advance(lexer); // Consume third " of closing """
            break;          // End of multiline string
        }

        if (peek(lexer) == L'\\')
        {
             advance(lexer); // Consume '\'
            if (is_at_end(lexer))
                break;                                    // Unterminated escape at EOF
            wchar_t baa_escape_char_key = advance(lexer); // Consume the Arabic escape key char
            switch (baa_escape_char_key)
            {
            case L'س': // Baa Newline
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\n');
                break;
            case L'م': // Baa Tab
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\t');
                break;
            case L'ر': // Baa Carriage Return
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\r');
                break;
            case L'ص': // Baa Null Character
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\0');
                break;
            case L'\\': // Baa Backslash (remains the same)
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\\');
                break;
            case L'"': // Baa Double Quote (remains the same)
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'"');
                break;
            case L'\'': // Baa Single Quote (can appear in strings)
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\'');
                break;
            case L'ي': // Baa Unicode escape \يXXXX (replaces \uXXXX)
            {
                int value = scan_hex_escape(lexer, 4);
                if (value == -1)
                {
                    free(buffer);
                    BaaToken *err_token = make_specific_error_token(lexer,
                        BAA_TOKEN_ERROR_INVALID_ESCAPE,
                        1002, "escape",
                        L"استخدم تسلسل هروب صالح مثل \\س أو \\م أو \\يXXXX",
                        L"تسلسل هروب يونيكود غير صالح (\\يXXXX) في سلسلة متعددة الأسطر (بدأت في السطر %zu، العمود %zu)",
                        token_start_line, token_start_col);
                    synchronize(lexer);
                    return err_token;
                }
                append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, (wchar_t)value);
                break;
            }
            case L'ه': // Baa Hex byte escape \هـHH
            {
                if (peek(lexer) == L'ـ')
                {
                    advance(lexer); // Consume 'ـ'
                    int byte_val = scan_hex_escape(lexer, 2);
                    if (byte_val == -1 || byte_val > 0xFF)
                    {
                        free(buffer);
                        BaaToken *err_token = make_specific_error_token(lexer,
                            BAA_TOKEN_ERROR_INVALID_ESCAPE,
                            1002, "escape",
                            L"استخدم تسلسل هروب صالح مثل \\هـ01 إلى \\هـFF",
                            L"تسلسل هروب سداسي عشري '\\هـHH' غير صالح في سلسلة متعددة الأسطر (بدأت في السطر %zu، العمود %zu)",
                            token_start_line, token_start_col);
                        synchronize(lexer);
                        return err_token;
                    }
                    append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, (wchar_t)byte_val);
                }
                else
                {
                    free(buffer);
                    BaaToken *err_token = make_specific_error_token(lexer,
                        BAA_TOKEN_ERROR_INVALID_ESCAPE,
                        1002, "escape",
                        L"استخدم \\هـHH للهروب السداسي عشري",
                        L"تسلسل هروب غير صالح: '\\ه' يجب أن يتبعها 'ـ' في سلسلة متعددة الأسطر (بدأت في السطر %zu، العمود %zu)",
                        token_start_line, token_start_col);
                    synchronize(lexer);
                    return err_token;
                }
                break;
            }
            default:
                free(buffer);
                BaaToken *err_token = make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR_INVALID_ESCAPE,
                    1002, "escape",
                    L"استخدم تسلسل هروب صالح مثل \\س أو \\م أو \\يXXXX",
                    L"تسلسل هروب غير صالح '\\%lc' في سلسلة نصية متعددة الأسطر (بدأت في السطر %zu، العمود %zu)",
                    baa_escape_char_key, token_start_line, token_start_col);
                synchronize(lexer);
                return err_token;
            }
            if (buffer == NULL)
                return make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR,
                    9001, "memory",
                    L"تحقق من توفر ذاكرة كافية في النظام",
                    L"فشل في إعادة تخصيص ذاكرة لسلسلة متعددة الأسطر (السطر %zu)",
                    token_start_line);
        }
        else
        {
            wchar_t c_to_append = advance(lexer); // Consume and get the character
            // advance() handles line/column updates for newline
            append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, c_to_append);
            if (buffer == NULL)
                return make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR,
                    9001, "memory",
                    L"تحقق من توفر ذاكرة كافية في النظام",
                    L"فشل في إعادة تخصيص ذاكرة لسلسلة متعددة الأسطر (السطر %zu)",
                    token_start_line);
        }
    }

    append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\0'); // Null-terminate the content
    if (buffer == NULL)
        return make_specific_error_token(lexer,
            BAA_TOKEN_ERROR,
            9001, "memory",
            L"تحقق من توفر ذاكرة كافية في النظام",
            L"فشل في إعادة تخصيص الذاكرة عند إنهاء السلسلة متعددة الأسطر (بدأت في السطر %zu)",
            token_start_line);

    BaaToken *token = malloc(sizeof(BaaToken));
    if (!token)
    {
        free(buffer);
        return NULL;
    }

    token->type = BAA_TOKEN_STRING_LIT;
    token->lexeme = buffer;         // Transfer ownership
    token->length = buffer_len - 1; // Don't count our internal null terminator
    token->line = token_start_line;
    token->column = token_start_col;

    // Initialize span (enhanced source location)
    token->span.start_line = token_start_line;
    token->span.start_column = token_start_col;
    token->span.end_line = lexer->line;
    token->span.end_column = lexer->column;
    token->span.start_offset = lexer->start;
    token->span.end_offset = lexer->current;

    // Initialize error context to NULL for non-error tokens
    token->error = NULL;

    return token;
}

BaaToken *scan_raw_string_literal(BaaLexer *lexer, bool is_multiline, size_t token_start_line, size_t token_start_col)
{
    size_t buffer_cap = 128;
    size_t buffer_len = 0;
    wchar_t *buffer = malloc(buffer_cap * sizeof(wchar_t));
    if (!buffer)
    {
        return make_specific_error_token(lexer,
            BAA_TOKEN_ERROR,
            9001, "memory",
            L"تحقق من توفر ذاكرة كافية في النظام",
            L"فشل في تخصيص ذاكرة لسلسلة نصية خام (بدأت في السطر %zu، العمود %zu)",
            token_start_line, token_start_col);
    }

    // When this function is called:
    // lexer->start points to 'خ'.
    // The prefix ('خ' and opening quote(s)) have been consumed by the dispatcher.
    // lexer->current is positioned *after* the opening 'خ"' or 'خ"""'.
    // token_start_line and token_start_col refer to the position of 'خ'.

    if (is_multiline)
    {
        // Looking for closing """
        while (true)
        {
            if (is_at_end(lexer))
            {
                free(buffer);
                BaaToken *err_token = make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR_UNTERMINATED_STRING,
                    1001, "string",
                    L"أضف علامة اقتباس مزدوجة ثلاثية \"\"\" في نهاية السلسلة الخام",
                    L"سلسلة نصية خام متعددة الأسطر غير منتهية (بدأت في السطر %zu، العمود %zu)",
                    token_start_line, token_start_col);
                return err_token;
            }

            if (peek(lexer) == L'"' &&
                lexer->current + 2 < lexer->source_length &&
                lexer->source[lexer->current + 1] == L'"' &&
                lexer->source[lexer->current + 2] == L'"')
            {
                advance(lexer); // Consume first " of closing """
                advance(lexer); // Consume second " of closing """
                advance(lexer); // Consume third " of closing """
                break;
            }
            // No escape sequence processing for raw strings
            append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, advance(lexer));
            if (buffer == NULL)
                return make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR,
                    9001, "memory",
                    L"تحقق من توفر ذاكرة كافية في النظام",
                    L"فشل في إعادة تخصيص ذاكرة لسلسلة خام متعددة الأسطر (السطر %zu)",
                    token_start_line);
        }
    }
    else
    {
        // Looking for closing "
        while (peek(lexer) != L'"' && !is_at_end(lexer))
        {
            if (peek(lexer) == L'\n')
            {
                // Newline not allowed in single-line raw string before closing quote
                free(buffer);
                // Error message should indicate that raw strings don't span lines unless multiline syntax is used.
                BaaToken *err_token = make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR_UNTERMINATED_STRING,
                    1001, "string",
                    L"استخدم خ\"\"\" للسلاسل الخام متعددة الأسطر أو أضف \" لإنهاء السلسلة",
                    L"سلسلة نصية خام أحادية السطر غير منتهية قبل السطر الجديد (بدأت في السطر %zu، العمود %zu)",
                    token_start_line, token_start_col);
                // Do not consume the newline, let synchronize handle it or next token be on new line.
                synchronize(lexer); // Try to recover
                return err_token;
            }
            // No escape sequence processing for raw strings
            append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, advance(lexer));
            if (buffer == NULL)
                return make_specific_error_token(lexer,
                    BAA_TOKEN_ERROR,
                    9001, "memory",
                    L"تحقق من توفر ذاكرة كافية في النظام",
                    L"فشل في إعادة تخصيص ذاكرة لسلسلة خام (السطر %zu)",
                    token_start_line);
        }

        if (is_at_end(lexer) || peek(lexer) != L'"')
        { // Could be EOF or some other char if newline was hit and error generated
            free(buffer);
            BaaToken *err_token = make_specific_error_token(lexer,
                BAA_TOKEN_ERROR_UNTERMINATED_STRING,
                1001, "string",
                L"أضف علامة اقتباس مزدوجة \" في نهاية السلسلة الخام",
                L"سلسلة نصية خام أحادية السطر غير منتهية (بدأت في السطر %zu، العمود %zu)",
                token_start_line, token_start_col);
            // No synchronize if at EOF, otherwise synchronize might have been called if newline was hit
            if (!is_at_end(lexer) && peek(lexer) != L'\n')
                synchronize(lexer);
            return err_token;
        }
        advance(lexer); // Consume closing quote "
    }

    append_char_to_buffer(&buffer, &buffer_len, &buffer_cap, L'\0'); // Null-terminate
    if (buffer == NULL)
        return make_specific_error_token(lexer,
            BAA_TOKEN_ERROR,
            9001, "memory",
            L"تحقق من توفر ذاكرة كافية في النظام",
            L"فشل في إعادة تخصيص الذاكرة عند إنهاء السلسلة الخام (بدأت في السطر %zu)",
            token_start_line);

    BaaToken *token = malloc(sizeof(BaaToken));
    if (!token)
    {
        free(buffer);
        return NULL;
    }

    token->type = BAA_TOKEN_STRING_LIT; // Reusing BAA_TOKEN_STRING_LIT
    token->lexeme = buffer;
    token->length = buffer_len - 1;
    token->line = token_start_line;
    token->column = token_start_col;

    // Initialize span (enhanced source location)
    token->span.start_line = token_start_line;
    token->span.start_column = token_start_col;
    token->span.end_line = lexer->line;
    token->span.end_column = lexer->column;
    token->span.start_offset = lexer->start;
    token->span.end_offset = lexer->current;

    // Initialize error context to NULL for non-error tokens
    token->error = NULL;

    return token;
}

/**
 * @brief Scans a sequence of non-newline whitespace characters (spaces and tabs).
 * Assumes lexer->start is positioned at the first whitespace character.
 * Consumes all subsequent adjacent spaces and tabs.
 *
 * @param lexer Pointer to the BaaLexer instance.
 * @return A BaaToken of type BAA_TOKEN_WHITESPACE.
 */
BaaToken *scan_whitespace_sequence(BaaLexer *lexer)
{
    // lexer->start is already at the first space/tab when this is called.
    while (peek(lexer) == L' ' || peek(lexer) == L'\t')
    {
        advance(lexer);
    }
    // lexer->current is now after the sequence. make_token uses lexer->start and lexer->current.
    return make_token(lexer, BAA_TOKEN_WHITESPACE);
}
/**
 * @brief Scans the content of a single-line comment (after //).
 * Assumes lexer->start is positioned at the first '/' of the "//".
 * The opening "//" is consumed by the caller (baa_lexer_next_token).
 * This function consumes the comment content until newline or EOF.
 *
 * @param lexer Pointer to the BaaLexer instance.
 * @param comment_delimiter_start_line Line where the opening "//" started.
 * @param comment_delimiter_start_col Column where the opening "//" started.
 * @return A BaaToken of type BAA_TOKEN_SINGLE_LINE_COMMENT. Lexeme is the content.
 */
BaaToken *scan_single_line_comment(BaaLexer *lexer, size_t comment_delimiter_start_line, size_t comment_delimiter_start_col)
{
    // Called after '//' is consumed by baa_lexer_next_token.
    // lexer->current is at the first character of the comment content.
    lexer->start = lexer->current; // Lexeme STARTS AFTER //

    while (peek(lexer) != L'\n' && !is_at_end(lexer))
    {
        advance(lexer);
    }
    // lexer->current is at '\n' or EOF.
    // make_token uses lexer->start (start of content) and lexer->current (end of content).
    BaaToken *token = make_token(lexer, BAA_TOKEN_SINGLE_LINE_COMMENT);
    if (token)
    {                                                // Ensure make_token succeeded
        token->line = comment_delimiter_start_line;  // Report token at the line of "//"
        token->column = comment_delimiter_start_col; // Report token at the column of "//"
    }
    // Do NOT consume the newline here; let baa_lexer_next_token handle it as a separate token.
    return token;
}

/**
 * @brief Scans the content of a multi-line comment (between /* and * /).
 * Assumes lexer->start is positioned at the first '/' of the "/*".
 * The opening "/*" is consumed by the caller (baa_lexer_next_token).
 * This function consumes the comment content and the closing "* /".
 *
 * @param lexer Pointer to the BaaLexer instance.
 * @param comment_delimiter_start_line Line where the opening "/*" started.
 * @param comment_delimiter_start_col Column where the opening "/*" started.
 * @return A BaaToken of type BAA_TOKEN_MULTI_LINE_COMMENT or BAA_TOKEN_ERROR if unterminated.
 */
BaaToken *scan_multi_line_comment(BaaLexer *lexer, size_t comment_delimiter_start_line, size_t comment_delimiter_start_col)
{
    // Called AFTER "/*" has been consumed by baa_lexer_next_token.
    // lexer->current is at the first character of the comment content.
    lexer->start = lexer->current; // Lexeme STARTS AFTER /*

    bool terminated = false;
    while (!is_at_end(lexer))
    {
        if (peek(lexer) == L'*' && peek_next(lexer) == L'/')
        {
            terminated = true;
            break; // lexer->current is at the '*' of "*/"
        }
        advance(lexer); // Consumes content char, advance handles line/col updates
    }

    if (!terminated)
    {
        // Error token should point to the start of the unterminated comment "/*"
        return make_specific_error_token(lexer,
            BAA_TOKEN_ERROR_UNTERMINATED_COMMENT,
            1007, "comment",
            L"أضف */ لإنهاء التعليق",
            L"تعليق متعدد الأسطر غير منتهٍ (بدأ في السطر %zu، العمود %zu)",
            comment_delimiter_start_line, comment_delimiter_start_col);
    }

    // lexer->current is at the '*' of "*/". make_token will create lexeme up to this point.
    BaaToken *token = make_token(lexer, BAA_TOKEN_MULTI_LINE_COMMENT);
    if (token)
    {
        token->line = comment_delimiter_start_line;  // Report token at the line of "/*"
        token->column = comment_delimiter_start_col; // Report token at the column of "/*"
    }

    advance(lexer); // Consume '*' of "*/"
    advance(lexer); // Consume '/' of "*/"
    return token;
}
