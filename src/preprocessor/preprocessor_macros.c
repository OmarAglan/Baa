// preprocessor_macros.c
#include "preprocessor_internal.h"

// Helper function to normalize whitespace in macro body for comparison
// Returns a new allocated string with normalized whitespace
static wchar_t *normalize_macro_body(const wchar_t *body)
{
    if (!body) return NULL;
    
    size_t len = wcslen(body);
    wchar_t *normalized = malloc((len + 1) * sizeof(wchar_t));
    if (!normalized) {
        // Memory allocation failure - this is a system-level error
        // No location context available in this helper function
        return NULL;
    }
    
    const wchar_t *src = body;
    wchar_t *dst = normalized;
    bool in_whitespace = false;
    
    // Skip leading whitespace
    while (iswspace(*src)) src++;
    
    while (*src) {
        if (iswspace(*src)) {
            if (!in_whitespace) {
                *dst++ = L' '; // Replace any whitespace with single space
                in_whitespace = true;
            }
            src++;
        } else {
            *dst++ = *src++;
            in_whitespace = false;
        }
    }
    
    // Remove trailing whitespace
    while (dst > normalized && dst[-1] == L' ') dst--;
    
    *dst = L'\0';
    return normalized;
}

// Helper function to compare parameter lists of two macros
static bool are_parameter_lists_equivalent(const BaaMacro *macro1, const BaaMacro *macro2)
{
    // Check if both are function-like
    if (macro1->is_function_like != macro2->is_function_like)
        return false;
    
    // If neither is function-like, they're equivalent
    if (!macro1->is_function_like && !macro2->is_function_like)
        return true;
    
    // Check parameter count and variadic status
    if (macro1->param_count != macro2->param_count)
        return false;
    
    if (macro1->is_variadic != macro2->is_variadic)
        return false;
    
    // According to C99 standard, parameter names don't need to match for compatibility
    // Only the count and variadic status matter for the signature
    return true;
}

// Helper function to check if two macro bodies are equivalent
static bool are_macro_bodies_equivalent(const wchar_t *body1, const wchar_t *body2)
{
    if (!body1 && !body2) return true;
    if (!body1 || !body2) return false;
    
    // Normalize whitespace for comparison
    wchar_t *norm1 = normalize_macro_body(body1);
    wchar_t *norm2 = normalize_macro_body(body2);
    
    if (!norm1 || !norm2) {
        // Memory allocation failure during normalization
        // This is a system-level issue, treat as non-equivalent
        free(norm1);
        free(norm2);
        return false;
    }
    
    bool equivalent = (wcscmp(norm1, norm2) == 0);
    
    free(norm1);
    free(norm2);
    return equivalent;
}

// Helper function to check if two macros are equivalent according to C99 standard
static bool are_macros_equivalent(const BaaMacro *existing, const BaaMacro *new_macro)
{
    if (!existing || !new_macro) return false;
    
    // Check if parameter lists are equivalent
    if (!are_parameter_lists_equivalent(existing, new_macro))
        return false;
    
    // Check if bodies are equivalent
    return are_macro_bodies_equivalent(existing->body, new_macro->body);
}

// Helper function to check if a macro is a predefined macro
static bool is_predefined_macro(const wchar_t *name)
{
    if (!name) return false;
    
    const wchar_t *predefined_macros[] = {
        L"__الملف__",
        L"__السطر__", 
        L"__التاريخ__",
        L"__الوقت__",
        L"__الدالة__",
        L"__إصدار_المعيار_باء__",
        NULL
    };
    
    for (size_t i = 0; predefined_macros[i]; i++) {
        if (wcscmp(name, predefined_macros[i]) == 0) {
            return true;
        }
    }
    
    return false;
}

// Helper function to free macro storage
void free_macros(BaaPreprocessor *pp)
{
    if (pp && pp->macros)
    {
        for (size_t i = 0; i < pp->macro_count; ++i)
        {
            free(pp->macros[i].name);
            free(pp->macros[i].body);
            // Free parameter names if it's a function-like macro
            if (pp->macros[i].is_function_like && pp->macros[i].param_names)
            {
                for (size_t j = 0; j < pp->macros[i].param_count; ++j)
                {
                    free(pp->macros[i].param_names[j]);
                }
                free(pp->macros[i].param_names);
            }
        }
        free(pp->macros);
        pp->macros = NULL;
        pp->macro_count = 0;
        pp->macro_capacity = 0;
    }
}

// Helper function to add or update a macro definition
// Returns true on success, false on allocation failure.
// Handles reallocation of the macro array.
bool add_macro(BaaPreprocessor *pp_state, const wchar_t *name, const wchar_t *body, bool is_function_like, bool is_variadic, size_t param_count, wchar_t **param_names)
{
    if (!pp_state || !name || !body)
    {
        // Invalid parameters for macro definition
        if (pp_state) {
            PpSourceLocation current_loc = get_current_original_location(pp_state);
            if (!name) {
                PP_REPORT_ERROR(pp_state, &current_loc, PP_ERROR_INVALID_MACRO_NAME, "macro",
                    L"اسم الماكرو مفقود أو غير صحيح.");
            } else if (!body) {
                PP_REPORT_ERROR(pp_state, &current_loc, PP_ERROR_MACRO_EXPANSION_FAILED, "macro",
                    L"جسم الماكرو '%ls' مفقود أو غير صحيح.", name);
            }
        }
        
        // Free potentially allocated params if other args are invalid
        // is_variadic implies is_function_like for this cleanup
        if ((is_function_like || is_variadic) && param_names)
        {
            for (size_t j = 0; j < param_count; ++j)
            {
                free(param_names[j]);
            }
            free(param_names);
        }
        return false;
    }

    // Check if macro already exists (simple linear search for now)
    for (size_t i = 0; i < pp_state->macro_count; ++i)
    {
        if (wcscmp(pp_state->macros[i].name, name) == 0)
        {
            // Found existing macro - check for redefinition compatibility
            BaaMacro new_macro = {
                .name = (wchar_t*)name,
                .body = (wchar_t*)body,
                .is_function_like = is_function_like,
                .is_variadic = is_function_like ? is_variadic : false,
                .param_count = param_count,
                .param_names = param_names
            };
            
            // Check if the redefinition is equivalent to the existing macro
            if (are_macros_equivalent(&pp_state->macros[i], &new_macro))
            {
                // Identical redefinition - allowed silently per C99 standard
                // Free the new parameters since we're keeping the old definition
                if ((is_function_like || is_variadic) && param_names)
                {
                    for (size_t j = 0; j < param_count; ++j)
                    {
                        free(param_names[j]);
                    }
                    free(param_names);
                }
                return true; // Successful, no change needed
            }
            else
            {
                // Incompatible redefinition - issue warning through diagnostic system
                PpSourceLocation current_loc = get_current_original_location(pp_state);
                
                // Check if it's a predefined macro (more serious)
                if (is_predefined_macro(name))
                {
                    // Report error for predefined macro redefinition using enhanced error system
                    PP_REPORT_ERROR(pp_state, &current_loc, PP_ERROR_MACRO_REDEFINITION, "macro",
                        L"إعادة تعريف الماكرو المدمج '%ls' غير مسموحة.", name);
                    
                    // Free the new parameters and reject the redefinition
                    if ((is_function_like || is_variadic) && param_names)
                    {
                        for (size_t j = 0; j < param_count; ++j)
                        {
                            free(param_names[j]);
                        }
                        free(param_names);
                    }
                    return false; // Reject predefined macro redefinition
                }
                else
                {
                    // Issue warning for regular macro incompatible redefinition using enhanced error system
                    PP_REPORT_WARNING(pp_state, &current_loc, PP_ERROR_MACRO_REDEFINITION, "macro",
                        L"إعادة تعريف الماكرو '%ls' بتعريف مختلف، سيتم استبدال التعريف السابق.", name);
                    
                    // Proceed with replacement after warning
                    free(pp_state->macros[i].body);
                    if (pp_state->macros[i].is_function_like && pp_state->macros[i].param_names)
                    {
                        for (size_t j = 0; j < pp_state->macros[i].param_count; ++j)
                        {
                            free(pp_state->macros[i].param_names[j]);
                        }
                        free(pp_state->macros[i].param_names);
                    }

                    pp_state->macros[i].body = baa_strdup(body);
                    pp_state->macros[i].is_function_like = is_function_like;
                    pp_state->macros[i].is_variadic = is_function_like ? is_variadic : false;
                    pp_state->macros[i].param_count = param_count;
                    pp_state->macros[i].param_names = param_names; // Takes ownership

                    // Check if allocations succeeded
                    if (!pp_state->macros[i].body)
                    {
                        // Memory allocation failure during macro body replacement
                        PP_REPORT_FATAL(pp_state, &current_loc, PP_ERROR_OUT_OF_MEMORY, "memory",
                            L"فشل في تخصيص الذاكرة لجسم الماكرو '%ls'.", name);
                        
                        // If body fails, try to clean up params if they were just assigned
                        if ((is_function_like || is_variadic) && param_names)
                        {
                            for (size_t j = 0; j < param_count; ++j)
                            {
                                free(param_names[j]);
                            }
                            free(param_names);
                        }
                        pp_state->macros[i].param_names = NULL; // Ensure it's NULL on failure
                        return false;
                    }
                    return true; // Redefinition successful with warning
                }
            }
        }
    }

    // New macro: Check capacity
    if (pp_state->macro_count >= pp_state->macro_capacity)
    {
        size_t new_capacity = (pp_state->macro_capacity == 0) ? 8 : pp_state->macro_capacity * 2;
        BaaMacro *new_macros = realloc(pp_state->macros, new_capacity * sizeof(BaaMacro));
        if (!new_macros)
        {
            // Memory reallocation failure for macro array expansion
            PpSourceLocation current_loc = get_current_original_location(pp_state);
            PP_REPORT_FATAL(pp_state, &current_loc, PP_ERROR_OUT_OF_MEMORY, "memory",
                L"فشل في إعادة تخصيص الذاكرة لتوسيع مصفوفة الماكرو لإضافة '%ls'.", name);
            
            // Free params if reallocation fails, as ownership wasn't transferred
            if (is_function_like && param_names)
            {
                for (size_t j = 0; j < param_count; ++j)
                    free(param_names[j]);
                free(param_names);
            }
            return false; // Reallocation failed
        }
        pp_state->macros = new_macros;
        pp_state->macro_capacity = new_capacity;
    }

    // Add the new macro
    BaaMacro *new_entry = &pp_state->macros[pp_state->macro_count];
    new_entry->name = baa_strdup(name);
    new_entry->body = baa_strdup(body);
    new_entry->is_function_like = is_function_like;
    new_entry->is_variadic = is_function_like ? is_variadic : false; // Variadic only if function-like
    new_entry->param_count = param_count;
    new_entry->param_names = param_names; // Takes ownership of the passed array and its contents

    // Check allocations
    if (!new_entry->name || !new_entry->body)
    {
        // Memory allocation failure for new macro entry
        PpSourceLocation current_loc = get_current_original_location(pp_state);
        PP_REPORT_FATAL(pp_state, &current_loc, PP_ERROR_OUT_OF_MEMORY, "memory",
            L"فشل في تخصيص الذاكرة لإدخال الماكرو الجديد '%ls'.", name);
        
        // Clean up everything allocated for this new entry on failure
        free(new_entry->name);                                           // Safe even if NULL
        free(new_entry->body);                                           // Safe even if NULL
        if ((is_function_like || new_entry->is_variadic) && param_names) // Use new_entry->is_variadic here
        {
            for (size_t j = 0; j < param_count; ++j)
            {
                free(param_names[j]); // Assumes param_names contains allocated strings
            }
            free(param_names);
        }
        // Don't increment count on failure
        return false; // Allocation failed
    }

    pp_state->macro_count++;
    return true;
}

// Helper function to find a macro by name
// Returns the macro definition or NULL if not found.
const BaaMacro *find_macro(const BaaPreprocessor *pp_state, const wchar_t *name)
{
    if (!pp_state || !name || !pp_state->macros)
        return NULL;

    // Simple linear search for now. Could use hash map for performance later.
    for (size_t i = 0; i < pp_state->macro_count; ++i)
    {
        if (wcscmp(pp_state->macros[i].name, name) == 0)
        {
            return &pp_state->macros[i];
        }
    }
    return NULL; // Not found
}

// Helper function to remove a macro definition by name
// Returns true if found and removed, false otherwise.
bool undefine_macro(BaaPreprocessor *pp_state, const wchar_t *name)
{
    if (!pp_state || !name || !pp_state->macros)
        return false;

    for (size_t i = 0; i < pp_state->macro_count; ++i)
    {
        if (wcscmp(pp_state->macros[i].name, name) == 0)
        {
            // Found the macro, now remove it
            free(pp_state->macros[i].name);
            free(pp_state->macros[i].body);
            // Free parameters if function-like
            if (pp_state->macros[i].is_function_like && pp_state->macros[i].param_names)
            {
                for (size_t j = 0; j < pp_state->macros[i].param_count; ++j)
                {
                    free(pp_state->macros[i].param_names[j]);
                }
                free(pp_state->macros[i].param_names);
            }

            // Shift subsequent elements down
            if (i < pp_state->macro_count - 1)
            {
                memmove(&pp_state->macros[i], &pp_state->macros[i + 1],
                        (pp_state->macro_count - 1 - i) * sizeof(BaaMacro));
            }

            pp_state->macro_count--;

            // Optional: Shrink capacity if desired, but not strictly necessary here
            // if (pp_state->macro_count < pp_state->macro_capacity / 4) { ... }

            return true; // Successfully removed
        }
    }

    return false; // Macro not found
}
