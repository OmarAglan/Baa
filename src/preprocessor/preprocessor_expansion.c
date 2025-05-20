#include "preprocessor_internal.h"

// --- Macro Expansion Stack Implementation ---

bool push_macro_expansion(BaaPreprocessor *pp_state, const BaaMacro *macro)
{
    if (pp_state->expanding_macros_count >= pp_state->expanding_macros_capacity)
    {
        size_t new_capacity = (pp_state->expanding_macros_capacity == 0) ? 8 : pp_state->expanding_macros_capacity * 2;
        const BaaMacro **new_stack = realloc(pp_state->expanding_macros_stack, new_capacity * sizeof(BaaMacro *));
        if (!new_stack)
            return false; // Allocation failure
        pp_state->expanding_macros_stack = new_stack;
        pp_state->expanding_macros_capacity = new_capacity;
    }
    pp_state->expanding_macros_stack[pp_state->expanding_macros_count++] = macro;
    return true;
}

void pop_macro_expansion(BaaPreprocessor *pp_state)
{
    if (pp_state->expanding_macros_count > 0)
    {
        pp_state->expanding_macros_count--;
    }
}

bool is_macro_expanding(const BaaPreprocessor *pp_state, const BaaMacro *macro)
{
    for (size_t i = 0; i < pp_state->expanding_macros_count; ++i)
    {
        if (pp_state->expanding_macros_stack[i] == macro)
        {
            return true;
        }
    }
    return false;
}

void free_macro_expansion_stack(BaaPreprocessor *pp_state)
{
    free(pp_state->expanding_macros_stack);
    pp_state->expanding_macros_stack = NULL;
    pp_state->expanding_macros_count = 0;
    pp_state->expanding_macros_capacity = 0;
}

// --- Function-Like Macro Helpers ---

bool stringify_argument(BaaPreprocessor *pp_state, DynamicWcharBuffer *output_buffer, const wchar_t *argument, wchar_t **error_message)
{
    size_t initial_capacity = wcslen(argument) + 10;
    DynamicWcharBuffer temp_buffer;
    if (!init_dynamic_buffer(&temp_buffer, initial_capacity))
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في تخصيص ذاكرة مؤقتة لتسلسل الوسيطة.");
        return false;
    }

    bool success = true;
    if (!append_to_dynamic_buffer(&temp_buffer, L"\""))
    {
        success = false;
    }

    const wchar_t *ptr = argument;
    while (*ptr != L'\0' && success)
    {
        wchar_t char_to_append[3] = {0};
        if (*ptr == L'\\' || *ptr == L'"')
        {
            char_to_append[0] = L'\\';
            char_to_append[1] = *ptr;
        }
        else
        {
            char_to_append[0] = *ptr;
        }
        if (!append_to_dynamic_buffer(&temp_buffer, char_to_append))
        {
            success = false;
        }
        ptr++;
    }

    if (success && !append_to_dynamic_buffer(&temp_buffer, L"\""))
    {
        success = false;
    }

    if (success)
    {
        if (!append_to_dynamic_buffer(output_buffer, temp_buffer.buffer))
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            *error_message = format_preprocessor_error_at_location(&error_loc, L"فشل في إلحاق الوسيطة المتسلسلة للمخرج.");
            success = false;
        }
    }
    free_dynamic_buffer(&temp_buffer);
    return success;
}

wchar_t **parse_macro_arguments(BaaPreprocessor *pp_state, const wchar_t **invocation_ptr_ref, const BaaMacro *macro, size_t *actual_arg_count, wchar_t **error_message)
{
    *actual_arg_count = 0;
    *error_message = NULL;
    const wchar_t *ptr = *invocation_ptr_ref;

    wchar_t **args = NULL;
    size_t args_capacity = 0;
    size_t named_param_count = macro->param_count;

    while (*ptr != L'\0')
    {
        while (iswspace(*ptr))
        {
            ptr++;
            if (pp_state)
                pp_state->current_column_number++;
        }

        if (*ptr == L')')
        {
            ptr++;
            if (pp_state)
                pp_state->current_column_number++;
            break;
        }

        // Comma handling:
        // A comma is expected if we've already parsed at least one argument AND
        // we are not about to parse the variadic part of a variadic macro that had named arguments.
        if (*actual_arg_count > 0)
        {
            bool expect_comma = true;
            if (macro->is_variadic && *actual_arg_count == named_param_count)
            {
                // If we've parsed all named arguments for a variadic macro,
                // the next part is __VA_ARGS__. A comma might or might not precede it.
                // If __VA_ARGS__ is empty and followed by ')', no comma.
                // If __VA_ARGS__ has content, a comma must have separated it from the last named arg (if any).
                // This logic is tricky. The current loop structure parses one "argument slot" at a time.
                // If it's the slot for __VA_ARGS__, it consumes everything.
                // The comma before __VA_ARGS__ (if named_param_count > 0) should be consumed
                // *before* parsing the __VA_ARGS__ content itself.
                // If named_param_count == 0 and it's variadic, no leading comma for __VA_ARGS__.
                if (named_param_count == 0)
                {                         // e.g. FOO(...)
                    expect_comma = false; // No comma before the first (variadic) argument
                }
                // If named_param_count > 0, a comma was needed to separate the last named from variadic.
                // This comma is consumed *before* this iteration if this iteration is for VA_ARGS.
                // So, if we are AT the VA_ARGS slot, we don't look for a comma here.
            }
            // Calculate column of the expected comma
            PpSourceLocation comma_loc = get_current_original_location(pp_state);
            comma_loc.column += (ptr - (*invocation_ptr_ref)) + 1; // +1 because ptr is at the char, column is 1-based

            if (expect_comma)
            {
                if (*ptr == L',')
                {
                    ptr++;
                    if (pp_state)
                        pp_state->current_column_number++;
                    while (iswspace(*ptr))
                    {
                        ptr++;
                        if (pp_state)
                            pp_state->current_column_number++;
                    }
                }
                else
                {
                    // If it's variadic, we've parsed all named args, and current char is not ')', it means variadic part starts.
                    // This case should be fine if no comma.
                    if (!(macro->is_variadic && *actual_arg_count == named_param_count))
                    {
                        *error_message = format_preprocessor_error_at_location(&comma_loc, L"تنسيق استدعاء الماكرو غير صالح: متوقع ',' أو ')' بين الوسيطات.");
                        goto parse_error;
                    }
                }
            }
        }

        const wchar_t *arg_start = ptr;
        const wchar_t *arg_end = ptr;
        PpSourceLocation arg_start_loc = get_current_original_location(pp_state);
        arg_start_loc.column += (arg_start - (*invocation_ptr_ref)) + 1;

        int paren_level = 0;
        bool in_string = false;
        bool in_char = false;
        wchar_t prev_char = L'\0';

        if (macro->is_variadic && *actual_arg_count >= named_param_count)
        {
            // This is the variadic part. Consume everything until the final closing parenthesis at level 0.
            while (*arg_end != L'\0')
            {
                // Physical column update for pp_state happens implicitly if needed
                if (in_string)
                {
                    if (*arg_end == L'"' && prev_char != L'\\')
                        in_string = false;
                }
                else if (in_char)
                {
                    if (*arg_end == L'\'' && prev_char != L'\\')
                        in_char = false;
                }
                else
                {
                    if (*arg_end == L'(')
                        paren_level++;
                    else if (*arg_end == L')')
                    {
                        if (paren_level == 0)
                            break;
                        paren_level--;
                    }
                    else if (*arg_end == L'"')
                        in_string = true;
                    else if (*arg_end == L'\'')
                        in_char = true;
                }
                if (*arg_end == L'\\' && prev_char == L'\\')
                    prev_char = L'\0';
                else
                    prev_char = *arg_end;
                arg_end++;
            }
        }
        else
        { // Parsing a named argument
            while (*arg_end != L'\0')
            {
                // Physical column update for pp_state happens implicitly if needed
                if (in_string)
                {
                    if (*arg_end == L'"' && prev_char != L'\\')
                        in_string = false;
                }
                else if (in_char)
                {
                    if (*arg_end == L'\'' && prev_char != L'\\')
                        in_char = false;
                }
                else
                {
                    if (*arg_end == L'(')
                        paren_level++;
                    else if (*arg_end == L')')
                    {
                        if (paren_level == 0)
                            break;
                        paren_level--;
                    }
                    else if (*arg_end == L',' && paren_level == 0)
                        break;
                    else if (*arg_end == L'"')
                        in_string = true;
                    else if (*arg_end == L'\'')
                        in_char = true;
                }
                if (*arg_end == L'\\' && prev_char == L'\\')
                    prev_char = L'\0';
                else
                    prev_char = *arg_end;
                arg_end++;
            }
        }

        if (paren_level != 0)
        {
            // Error occurred scanning this argument, use arg_start_loc
            *error_message = format_preprocessor_error_at_location(&arg_start_loc, L"أقواس غير متطابقة في وسيطات الماكرو.");
            goto parse_error;
        }
        if (in_string || in_char)
        {
            // Error occurred scanning this argument, use arg_start_loc
            *error_message = format_preprocessor_error_at_location(&arg_start_loc, L"علامة اقتباس غير منتهية في وسيطات الماكرو.");
            goto parse_error;
        }

        // After successfully scanning an argument (or __VA_ARGS__ content),
        // ptr points to the character that terminated the scan (e.g., ',', ')', or '\0').
        // arg_start and arg_end define the span of the current argument's text.

        ptr = arg_end;

        const wchar_t *effective_arg_end = arg_end;
        const wchar_t *effective_arg_start = arg_start;

        if (!(macro->is_variadic && *actual_arg_count >= named_param_count))
        {
            while (effective_arg_end > effective_arg_start && iswspace(*(effective_arg_end - 1)))
            {
                effective_arg_end--;
            }
            while (iswspace(*effective_arg_start) && effective_arg_start < effective_arg_end)
            {
                effective_arg_start++;
            }
        }
        size_t arg_len = effective_arg_end - effective_arg_start;

        if (macro->is_variadic && *actual_arg_count == named_param_count &&
            effective_arg_start == effective_arg_end && *ptr == L')')
        { // Check effective_arg_start == effective_arg_end for empty VA
            arg_len = 0;
        }

        wchar_t *arg_str = wcsndup_internal(effective_arg_start, arg_len);
        if (!arg_str)
        {
            // Error allocating for this argument, use arg_start_loc
            *error_message = format_preprocessor_error_at_location(&arg_start_loc, L"فشل في تخصيص ذاكرة لوسيطة الماكرو.");
            goto parse_error;
        }

        if (*actual_arg_count >= args_capacity)
        {
            size_t new_capacity = (args_capacity == 0) ? (macro->is_variadic ? named_param_count + 1 : 4) : args_capacity * 2;
            if (macro->is_variadic && new_capacity <= named_param_count)
                new_capacity = named_param_count + 1;
            wchar_t **new_args = realloc(args, new_capacity * sizeof(wchar_t *));
            if (!new_args)
            {
                free(arg_str);
                *error_message = format_preprocessor_error_at_location(&arg_start_loc, L"فشل إعادة تخصيص وسيطات الماكرو.");
                goto parse_error;
            }
            args = new_args;
            args_capacity = new_capacity;
        }
        args[*actual_arg_count] = arg_str;
        (*actual_arg_count)++;

        if (*ptr == L'\0')
        { // Reached end of string before closing parenthesis for the call
          // Error is about the overall call structure, point to where ')' was expected (macro invocation loc)
            PpSourceLocation call_loc = get_current_original_location(pp_state);
            *error_message = format_preprocessor_error_at_location(&call_loc, L"قوس إغلاق ')' مفقود في استدعاء الماكرو.");
            goto parse_error;
        }

        if (macro->is_variadic && *actual_arg_count == named_param_count + 1)
        {
            if (*ptr == L')')
            {
                ptr++; /* physical column update handled by caller */
                break;
            }
        }
    }

    if (ptr == *invocation_ptr_ref || (*(ptr - 1) != L')' && *ptr != L'\0'))
    {
        if (!(*ptr == L'\0' && *(ptr - 1) == L')'))
        {
            PpSourceLocation error_loc = get_current_original_location(pp_state);
            error_loc.column += (ptr - (*invocation_ptr_ref)); // Adjust column to current scan point
            *error_message = format_preprocessor_error_at_location(&error_loc, L"تنسيق استدعاء الماكرو غير صالح: قوس الإغلاق ')' مفقود بعد الوسيطات.");
            goto parse_error;
        }
    }

    if (macro->is_variadic && *actual_arg_count == named_param_count)
    {
        if (*actual_arg_count >= args_capacity)
        {
            size_t new_capacity = args_capacity + 1;
            wchar_t **new_args = realloc(args, new_capacity * sizeof(wchar_t *));
            if (!new_args)
            {
                PpSourceLocation va_alloc_loc = get_current_original_location(pp_state);
                va_alloc_loc.column += (ptr - (*invocation_ptr_ref)); // Point to where VA_ARGS should start
                *error_message = format_preprocessor_error_at_location(&va_alloc_loc, L"فشل تخصيص __VA_ARGS__ فارغ.");
                goto parse_error;
            }
            args = new_args;
            args_capacity = new_capacity;
        }
        args[*actual_arg_count] = _wcsdup(L"");
        if (!args[*actual_arg_count])
        {
            PpSourceLocation va_dup_loc = get_current_original_location(pp_state);
            va_dup_loc.column += (ptr - (*invocation_ptr_ref));
            *error_message = format_preprocessor_error_at_location(&va_dup_loc, L"فشل نسخ __VA_ARGS__ فارغ.");
            goto parse_error;
        }
        (*actual_arg_count)++;
    }

    // Final check for argument count for non-variadic macros
    if (!macro->is_variadic && *actual_arg_count != named_param_count)
    {
        PpSourceLocation error_loc = get_current_original_location(pp_state);
        // Point error to the location of the macro call itself
        *error_message = format_preprocessor_error_at_location(&error_loc, L"عدد وسيطات غير صحيح للماكرو '%ls' (متوقع %zu، تم الحصول على %zu).", macro->name, named_param_count, *actual_arg_count);
        goto parse_error;
    }
    // For variadic, minimum is named_param_count (VA_ARGS is empty), actual_arg_count becomes named_param_count + 1
    if (macro->is_variadic && *actual_arg_count < named_param_count + 1)
    { // Should be param_count + 1 after empty VA_ARGS added
      // This case should be covered by the empty VA_ARGS addition, unless something went wrong.
    }

    *invocation_ptr_ref = ptr;
    return args;

parse_error:
    if (args)
    {
        for (size_t i = 0; i < *actual_arg_count; ++i)
            free(args[i]);
        free(args);
    }
    *actual_arg_count = 0;
    return NULL;
}

bool substitute_macro_body(BaaPreprocessor *pp_state, DynamicWcharBuffer *output_buffer, const BaaMacro *macro, wchar_t **arguments, size_t arg_count, wchar_t **error_message)
{
    const wchar_t *body_ptr = macro->body;
    bool success = true;
    DynamicWcharBuffer pending_token_buffer;
    bool pending_token_active = false;

    const wchar_t *va_args_keyword = L"__وسائط_متغيرة__";
    size_t va_args_keyword_len = wcslen(va_args_keyword);

    if (!init_dynamic_buffer(&pending_token_buffer, 64))
    {
        PpSourceLocation el = get_current_original_location(pp_state);
        *error_message = format_preprocessor_error_at_location(&el, L"فشل تهيئة مخزن الرمز المعلق.");
        return false;
    }

    while (*body_ptr != L'\0' && success)
    {
        if (iswspace(*body_ptr))
        {
            if (pending_token_active)
            {
                if (!append_to_dynamic_buffer(output_buffer, pending_token_buffer.buffer))
                {
                    success = false;
                    break;
                }
                free_dynamic_buffer(&pending_token_buffer);
                if (!init_dynamic_buffer(&pending_token_buffer, 64))
                {
                    success = false;
                    break;
                }
                pending_token_active = false;
            }
            if (!append_dynamic_buffer_n(output_buffer, body_ptr, 1))
            {
                success = false;
                break;
            }
            body_ptr++;
            continue;
        }

        if (*body_ptr == L'#' && *(body_ptr + 1) == L'#')
        { // Token Pasting
            if (!pending_token_active)
            {
                PpSourceLocation el = get_current_original_location(pp_state);
                *error_message = format_preprocessor_error_at_location(&el, L"## في موقع غير صالح بـ '%ls'.", macro->name);
                success = false;
                break;
            }
            body_ptr += 2;
            while (iswspace(*body_ptr))
                body_ptr++;

            const wchar_t *rhs_start = body_ptr;
            bool rhs_is_va_args = false;
            if (macro->is_variadic && wcsncmp(body_ptr, va_args_keyword, va_args_keyword_len) == 0 && !(iswalnum(*(body_ptr + va_args_keyword_len)) || *(body_ptr + va_args_keyword_len) == L'_'))
            {
                body_ptr += va_args_keyword_len;
                rhs_is_va_args = true;
            }
            else if (iswalpha(*body_ptr) || *body_ptr == L'_')
            {
                while (iswalnum(*body_ptr) || *body_ptr == L'_')
                    body_ptr++;
            }
            else if (iswdigit(*body_ptr))
            {
                while (iswdigit(*body_ptr))
                    body_ptr++;
            }
            else
            {
                PpSourceLocation el = get_current_original_location(pp_state);
                *error_message = format_preprocessor_error_at_location(&el, L"## يجب أن يتبعه معرف أو رقم أو __وسائط_متغيرة__ في '%ls'.", macro->name);
                success = false;
                break;
            }
            size_t rhs_len = body_ptr - rhs_start;
            wchar_t *rhs_token_str = wcsndup_internal(rhs_start, rhs_len);
            if (!rhs_token_str)
            {
                PpSourceLocation el = get_current_original_location(pp_state);
                *error_message = format_preprocessor_error_at_location(&el, L"فشل تخصيص RHS لـ ##.");
                success = false;
                break;
            }

            wchar_t *rhs_value_to_paste = NULL;
            if (rhs_is_va_args)
            {
                if (arg_count > macro->param_count)
                {
                    rhs_value_to_paste = _wcsdup(arguments[macro->param_count]);
                }
                else
                {
                    rhs_value_to_paste = _wcsdup(L"");
                }
            }
            else
            {
                bool rhs_is_param = false;
                for (size_t i = 0; i < macro->param_count; ++i)
                {
                    if (wcscmp(rhs_token_str, macro->param_names[i]) == 0)
                    {
                        rhs_value_to_paste = (arguments[i][0] == L'\0') ? _wcsdup(L"") : _wcsdup(arguments[i]);
                        rhs_is_param = true;
                        break;
                    }
                }
                if (!rhs_is_param)
                    rhs_value_to_paste = _wcsdup(rhs_token_str);
            }
            free(rhs_token_str);
            if (!rhs_value_to_paste)
            {
                PpSourceLocation el = get_current_original_location(pp_state);
                *error_message = format_preprocessor_error_at_location(&el, L"فشل تخصيص قيمة RHS لـ ##.");
                success = false;
                break;
            }

            if (pending_token_buffer.length == 0)
            {
                if (!append_to_dynamic_buffer(&pending_token_buffer, rhs_value_to_paste))
                    success = false;
            }
            else if (rhs_value_to_paste[0] == L'\0')
            { /* RHS empty, LHS stays */
            }
            else
            {
                if (!append_to_dynamic_buffer(&pending_token_buffer, rhs_value_to_paste))
                    success = false;
            }

            free(rhs_value_to_paste);
            if (!success)
            {
                PpSourceLocation el = get_current_original_location(pp_state);
                *error_message = format_preprocessor_error_at_location(&el, L"فشل إلحاق قيمة RHS لـ ##.");
                break;
            }
            pending_token_active = true;
            continue;
        }

        if (pending_token_active)
        {
            if (!append_to_dynamic_buffer(output_buffer, pending_token_buffer.buffer))
            {
                success = false;
                break;
            }
            free_dynamic_buffer(&pending_token_buffer);
            if (!init_dynamic_buffer(&pending_token_buffer, 64))
            {
                success = false;
                break;
            }
            pending_token_active = false;
        }
        if (!success)
            break;

        if (*body_ptr == L'#')
        { // Stringify
            const wchar_t *operator_ptr = body_ptr;
            body_ptr++;
            if (macro->is_variadic && wcsncmp(body_ptr, va_args_keyword, va_args_keyword_len) == 0 && !(iswalnum(*(body_ptr + va_args_keyword_len)) || *(body_ptr + va_args_keyword_len) == L'_'))
            {
                if (arg_count > macro->param_count)
                {
                    if (!stringify_argument(pp_state, output_buffer, arguments[macro->param_count], error_message))
                        success = false;
                }
                else
                {
                    if (!append_to_dynamic_buffer(output_buffer, L"\"\""))
                        success = false;
                }
                body_ptr += va_args_keyword_len;
            }
            else if (iswalpha(*body_ptr) || *body_ptr == L'_')
            {
                const wchar_t *id_start = body_ptr;
                while (iswalnum(*body_ptr) || *body_ptr == L'_')
                    body_ptr++;
                size_t id_len = body_ptr - id_start;
                wchar_t *id_text = wcsndup_internal(id_start, id_len);
                if (!id_text)
                {
                    PpSourceLocation el = get_current_original_location(pp_state);
                    *error_message = format_preprocessor_error_at_location(&el, L"فشل تخصيص معرف لـ #.");
                    success = false;
                    break;
                }
                bool param_found = false;
                for (size_t i = 0; i < macro->param_count; ++i)
                {
                    if (wcscmp(id_text, macro->param_names[i]) == 0)
                    {
                        if (!stringify_argument(pp_state, output_buffer, arguments[i], error_message))
                            success = false;
                        param_found = true;
                        break;
                    }
                }
                free(id_text);
                if (!success)
                    break;
                if (!param_found)
                {
                    if (!append_dynamic_buffer_n(output_buffer, operator_ptr, 1))
                    {
                        success = false;
                        break;
                    }
                    body_ptr = id_start;
                }
            }
            else
            {
                if (!append_dynamic_buffer_n(output_buffer, operator_ptr, 1))
                {
                    success = false;
                    break;
                }
            }
            continue;
        }

        if (iswalpha(*body_ptr) || *body_ptr == L'_')
        { // Identifier
            const wchar_t *id_start = body_ptr;
            while (iswalnum(*body_ptr) || *body_ptr == L'_')
                body_ptr++;
            size_t id_len = body_ptr - id_start;
            wchar_t *identifier = wcsndup_internal(id_start, id_len);
            if (!identifier)
            {
                PpSourceLocation el = get_current_original_location(pp_state);
                *error_message = format_preprocessor_error_at_location(&el, L"فشل تخصيص معرف بنص ماكرو '%ls'.", macro->name);
                success = false;
                break;
            }

            bool substituted = false;
            if (macro->is_variadic && wcscmp(identifier, va_args_keyword) == 0)
            {
                if (arg_count > macro->param_count && arguments[macro->param_count][0] != L'\0')
                {
                    if (!append_to_dynamic_buffer(&pending_token_buffer, arguments[macro->param_count]))
                        success = false;
                }
                substituted = true;
            }
            else
            {
                for (size_t i = 0; i < macro->param_count; ++i)
                {
                    if (wcscmp(identifier, macro->param_names[i]) == 0)
                    {
                        if (arguments[i][0] != L'\0')
                        {
                            if (!append_to_dynamic_buffer(&pending_token_buffer, arguments[i]))
                                success = false;
                        }
                        substituted = true;
                        break;
                    }
                }
            }
            if (!substituted)
            {
                if (!append_to_dynamic_buffer(&pending_token_buffer, identifier))
                    success = false;
            }
            free(identifier);
            if (!success)
                break;
            pending_token_active = true;
        }
        else
        { // Single character token
            if (!append_dynamic_buffer_n(&pending_token_buffer, body_ptr, 1))
            {
                success = false;
                break;
            }
            body_ptr++;
            pending_token_active = true;
        }
    }

    if (success && pending_token_active)
    {
        if (!append_to_dynamic_buffer(output_buffer, pending_token_buffer.buffer))
        {
            success = false;
            if (!*error_message)
            {
                PpSourceLocation el = get_current_original_location(pp_state);
                *error_message = format_preprocessor_error_at_location(&el, L"فشل إلحاق الرمز المعلق الأخير في '%ls'.", macro->name);
            }
        }
    }
    free_dynamic_buffer(&pending_token_buffer);
    return success;
}
