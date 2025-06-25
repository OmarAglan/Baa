#include "baa/lexer/lexer.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

// Forward declaration for baa_is_arabic_digit
bool baa_is_arabic_digit(wchar_t c);

// Arabic-Indic digits (٠١٢٣٤٥٦٧٨٩)
static const wchar_t ARABIC_DIGITS[] = {0x0660, 0x0661, 0x0662, 0x0663, 0x0664,
                                        0x0665, 0x0666, 0x0667, 0x0668, 0x0669};

// Convert Arabic-Indic digit to decimal value
static int arabic_to_decimal(wchar_t c)
{
    for (int i = 0; i < 10; i++)
    {
        if (c == ARABIC_DIGITS[i])
        {
            return i;
        }
    }
    return -1; // Not an Arabic digit
}

// Convert hexadecimal digit to decimal value
static int hex_to_decimal(wchar_t c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1; // Not a hex digit
}

// Check if a character is a hexadecimal digit
bool baa_is_hex_digit(wchar_t c)
{
    return (c >= '0' && c <= '9') || 
           (c >= 'a' && c <= 'f') || 
           (c >= 'A' && c <= 'F');
}

bool baa_is_digit(wchar_t c)
{
    return (c >= '0' && c <= '9') || baa_is_arabic_digit(c);
}

bool baa_is_decimal_digit(wchar_t c)
{
    return baa_is_digit(c) || c == '.' || c == L'٫'; // Support both . and Arabic decimal separator
}

bool baa_is_arabic_digit(wchar_t c)
{
    return (c >= 0x0660 && c <= 0x0669); // Arabic-Indic digits range
}

bool baa_is_number_start(wchar_t c)
{
    return baa_is_digit(c) || c == '.' || c == L'٫' || c == '0'; // 0 might be start of hex/bin
}

// Parse hexadecimal integer
static BaaNumberError parse_hex_integer(const wchar_t *text, size_t *pos, size_t length, long long *value)
{
    long long result = 0;
    bool has_digits = false;

    while (*pos < length && baa_is_hex_digit(text[*pos]))
    {
        has_digits = true;
        int digit = hex_to_decimal(text[*pos]);

        // Check for overflow (16 is the hexadecimal base)
        if (result > (LLONG_MAX - digit) / 16)
        {
            return BAA_NUM_OVERFLOW;
        }

        result = result * 16 + digit;
        (*pos)++;
    }

    if (!has_digits)
    {
        return BAA_NUM_INVALID_FORMAT;
    }

    *value = result;
    return BAA_NUM_SUCCESS;
}

// Parse binary integer
static BaaNumberError parse_binary_integer(const wchar_t *text, size_t *pos, size_t length, long long *value)
{
    long long result = 0;
    bool has_digits = false;

    while (*pos < length && (text[*pos] == '0' || text[*pos] == '1'))
    {
        has_digits = true;
        int digit = text[*pos] - '0';

        // Check for overflow (2 is the binary base)
        if (result > (LLONG_MAX - digit) / 2)
        {
            return BAA_NUM_OVERFLOW;
        }

        result = result * 2 + digit;
        (*pos)++;
    }

    if (!has_digits)
    {
        return BAA_NUM_INVALID_FORMAT;
    }

    *value = result;
    return BAA_NUM_SUCCESS;
}

// Parse exponent part of scientific notation
static BaaNumberError parse_exponent(const wchar_t *text, size_t *pos, size_t length, int *exponent)
{
    bool is_negative = false;
    if (*pos < length && (text[*pos] == '+' || text[*pos] == '-'))
    {
        is_negative = (text[*pos] == '-');
        (*pos)++;
    }

    int exp_value = 0;
    bool has_digits = false;

    while (*pos < length && baa_is_digit(text[*pos]))
    {
        has_digits = true;
        int digit;

        if (baa_is_arabic_digit(text[*pos]))
        {
            digit = arabic_to_decimal(text[*pos]);
        }
        else
        {
            digit = text[*pos] - '0';
        }

        // Limit exponent to reasonable size to prevent overflow
        if (exp_value > (INT_MAX - digit) / 10)
        {
            return BAA_NUM_OVERFLOW;
        }

        exp_value = exp_value * 10 + digit;
        (*pos)++;
    }

    if (!has_digits)
    {
        return BAA_NUM_INVALID_FORMAT;
    }

    *exponent = is_negative ? -exp_value : exp_value;
    return BAA_NUM_SUCCESS;
}

static BaaNumberError parse_integer_part(const wchar_t *text, size_t *pos, size_t length, long long *value)
{
    long long result = 0;
    bool has_digits = false;

    while (*pos < length && baa_is_digit(text[*pos]))
    {
        has_digits = true;
        int digit;

        if (baa_is_arabic_digit(text[*pos]))
        {
            digit = arabic_to_decimal(text[*pos]);
        }
        else
        {
            digit = text[*pos] - '0';
        }

        // Check for overflow
        if (result > (LLONG_MAX - digit) / 10)
        {
            return BAA_NUM_OVERFLOW;
        }

        result = result * 10 + digit;
        (*pos)++;
    }

    if (!has_digits)
    {
        return BAA_NUM_INVALID_FORMAT;
    }

    *value = result;
    return BAA_NUM_SUCCESS;
}

// Parse the fractional part of a hexadecimal float
static BaaNumberError parse_hex_fractional_part(const wchar_t *text, size_t *pos, size_t length, double *value)
{
    double result = 0;
    double place = 1.0 / 16.0;
    bool has_digits = false;

    while (*pos < length && baa_is_hex_digit(text[*pos]))
    {
        has_digits = true;
        int digit = hex_to_decimal(text[*pos]);
        result += digit * place;
        place /= 16.0;
        (*pos)++;
    }

    // It's okay to have no digits after the point, e.g., "0x1."
    *value = result;
    return BAA_NUM_SUCCESS;
}

static BaaNumberError parse_decimal_part(const wchar_t *text, size_t *pos, size_t length, double *value)
{
    double result = 0;
    double place = 0.1;
    bool has_digits = false;

    while (*pos < length && baa_is_digit(text[*pos]))
    {
        has_digits = true;
        int digit;

        if (baa_is_arabic_digit(text[*pos]))
        {
            digit = arabic_to_decimal(text[*pos]);
        }
        else
        {
            digit = text[*pos] - '0';
        }

        result += digit * place;
        place /= 10;
        (*pos)++;
    }

    if (!has_digits)
    {
        return BAA_NUM_INVALID_FORMAT;
    }

    *value = result;
    return BAA_NUM_SUCCESS;
}

BaaNumber *baa_parse_number(const wchar_t *text, size_t length, BaaNumberError *error)
{
    if (!text || length == 0)
    {
        if (error)
            *error = BAA_NUM_INVALID_FORMAT;
        return NULL;
    }

    BaaNumber *number = (BaaNumber *)malloc(sizeof(BaaNumber));
    if (!number)
    {
        if (error)
            *error = BAA_NUM_MEMORY_ERROR;
        return NULL;
    }

    // Copy the raw text
    wchar_t *raw_text = (wchar_t *)malloc((length + 1) * sizeof(wchar_t));
    if (!raw_text)
    {
        free(number);
        if (error)
            *error = BAA_NUM_MEMORY_ERROR;
        return NULL;
    }
    wcsncpy(raw_text, text, length);
    raw_text[length] = L'\0';

    number->raw_text = raw_text;
    number->text_length = length;

    size_t pos = 0;
    BaaNumberError parse_error = BAA_NUM_SUCCESS;

    // Handle hexadecimal format (0x...)
    if (length >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        pos = 2; // Skip "0x" prefix

        // Check for hex float (must have 'أ' for exponent)
        bool is_hex_float = false;
        for (size_t i = pos; i < length; i++) {
            if (text[i] == L'أ') {
                is_hex_float = true;
                break;
            }
        }

        if (is_hex_float) {
            long long int_part = 0;
            double frac_part = 0.0;

            // Parse integer part (can be empty)
            size_t start_pos = pos;
            if (text[pos] != '.') {
                parse_error = parse_hex_integer(text, &pos, length, &int_part);
            } else {
                parse_error = BAA_NUM_SUCCESS; // No integer part, which is valid
            }

            // Parse fractional part if a dot is present
            if (pos < length && text[pos] == '.') {
                pos++; // Skip '.'
                parse_error = parse_hex_fractional_part(text, &pos, length, &frac_part);
            }

            if (parse_error == BAA_NUM_SUCCESS) {
                // Parse exponent
                if (pos < length && text[pos] == L'أ') {
                    pos++; // Skip 'أ'
                    int exponent;
                    parse_error = parse_exponent(text, &pos, length, &exponent);
                    if (parse_error == BAA_NUM_SUCCESS) {
                        number->type = BAA_NUM_SCIENTIFIC;
                        double mantissa = (double)int_part + frac_part;
                        number->decimal_value = mantissa * pow(2.0, exponent);
                    }
                } else {
                    parse_error = BAA_NUM_INVALID_FORMAT; // Missing exponent
                }
            }
        } else {
            // Regular hex integer
            long long int_value;
            parse_error = parse_hex_integer(text, &pos, length, &int_value);

            if (parse_error == BAA_NUM_SUCCESS)
            {
                number->type = BAA_NUM_INTEGER;
                number->int_value = int_value;
            }
        }
    }
    // Handle binary format (0b...)
    else if (length >= 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B'))
    {
        pos = 2; // Skip "0b" prefix
        long long int_value;
        parse_error = parse_binary_integer(text, &pos, length, &int_value);

        if (parse_error == BAA_NUM_SUCCESS)
        {
            number->type = BAA_NUM_INTEGER;
            number->int_value = int_value;
        }
    }
    else
    {
        // Check for scientific notation or decimal point
        bool is_decimal = false;
        bool is_scientific = false;
        
        for (size_t i = 0; i < length; i++)
        {
            if (text[i] == '.' || text[i] == L'٫')
            {
                if (is_decimal)
                {
                    free(raw_text);
                    free(number);
                    if (error)
                        *error = BAA_NUM_MULTIPLE_DOTS;
                    return NULL;
                }
                is_decimal = true;
            }
            // UPDATED: Check for Arabic exponent marker 'أ' instead of 'e'/'E'
            else if (text[i] == L'أ')
            {
                is_scientific = true;
                break;
            }
        }

        if (!is_decimal && !is_scientific)
        {
            // Parse regular integer
            long long int_value;
            parse_error = parse_integer_part(text, &pos, length, &int_value);

            if (parse_error == BAA_NUM_SUCCESS)
            {
                number->type = BAA_NUM_INTEGER;
                number->int_value = int_value;
            }
        }
        else if (is_scientific)
        {
            // Parse scientific notation (e.g., 1.23e4 or 5e-3)
            long long int_part = 0;
            double dec_part = 0.0;
            bool has_dec_point = false;
            
            // Find position of 'e' or 'E'
            size_t e_pos = 0;
            for (e_pos = 0; e_pos < length; e_pos++)
            {
                // UPDATED: Check for Arabic exponent marker 'أ' instead of 'e'/'E'
                if (text[e_pos] == L'أ')
                    break;
                if (text[e_pos] == '.' || text[e_pos] == L'٫')
                    has_dec_point = true;
            }
            
            // Parse mantissa (part before 'e')
            if (has_dec_point)
            {
                // Parse integer part
                parse_error = parse_integer_part(text, &pos, e_pos, &int_part);
                
                if (parse_error == BAA_NUM_SUCCESS)
                {
                    // Skip decimal point
                    pos++;
                    
                    // Parse decimal part
                    parse_error = parse_decimal_part(text, &pos, e_pos, &dec_part);
                }
            }
            else
            {
                // No decimal point, just parse integer
                parse_error = parse_integer_part(text, &pos, e_pos, &int_part);
            }
            
            // Parse exponent
            if (parse_error == BAA_NUM_SUCCESS)
            {
                pos = e_pos + 1; // Skip 'e' or 'E'
                int exponent;
                parse_error = parse_exponent(text, &pos, length, &exponent);
                
                if (parse_error == BAA_NUM_SUCCESS)
                {
                    number->type = BAA_NUM_SCIENTIFIC;
                    double mantissa = (double)int_part + dec_part;
                    number->decimal_value = mantissa * pow(10.0, exponent);
                }
            }
        }
        else
        {
            // Parse regular decimal number
            long long int_part = 0;
            double dec_part = 0.0;

            // Parse integer part
            parse_error = parse_integer_part(text, &pos, length, &int_part);
            if (parse_error == BAA_NUM_SUCCESS)
            {
                // Skip decimal point
                pos++;

                // Parse decimal part
                parse_error = parse_decimal_part(text, &pos, length, &dec_part);
                if (parse_error == BAA_NUM_SUCCESS)
                {
                    number->type = BAA_NUM_DECIMAL;
                    number->decimal_value = (double)int_part + dec_part;
                }
            }
        }
    }

    if (parse_error != BAA_NUM_SUCCESS)
    {
        free(raw_text);
        free(number);
        if (error)
            *error = BAA_NUM_SUCCESS;
        return NULL;
    }

    // Check for float suffix 'ح' at the end of the number
    if (pos < length && text[pos] == L'ح') {
        pos++; // Consume the suffix
        // The number is forced to be a float type.
        if (number->type == BAA_NUM_INTEGER) {
            number->type = BAA_NUM_DECIMAL;
            number->decimal_value = (double)number->int_value;
        }
    }

    if (pos < length) {
        // There are unprocessed characters left in the number lexeme
        free(raw_text);
        free(number);
        if (error) *error = BAA_NUM_INVALID_FORMAT;
        return NULL;
    }

    if (error)
        *error = BAA_NUM_SUCCESS;
    return number;
}

void baa_free_number(BaaNumber *number)
{
    if (number)
    {
        if (number->raw_text)
        {
            free((void *)number->raw_text);
        }
        free(number);
    }
}

const wchar_t *baa_number_error_message(BaaNumberError error)
{
    switch (error)
    {
    case BAA_NUM_SUCCESS:
        return L"نجاح"; // Success
    case BAA_NUM_OVERFLOW:
        return L"الرقم كبير جداً"; // Number too large
    case BAA_NUM_INVALID_CHAR:
        return L"حرف غير صالح في الرقم"; // Invalid character in number
    case BAA_NUM_MULTIPLE_DOTS:
        return L"نقاط عشرية متعددة"; // Multiple decimal points
    case BAA_NUM_INVALID_FORMAT:
        return L"تنسيق رقم غير صالح"; // Invalid number format
    case BAA_NUM_MEMORY_ERROR:
        return L"خطأ في الذاكرة"; // Memory error
    default:
        return L"خطأ غير معروف"; // Unknown error
    }
}
