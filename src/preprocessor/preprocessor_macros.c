#include "preprocessor_internal.h"

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
            // Redefinition: Free old body and params, update with new ones
            // Note: C standard usually warns/errors on incompatible redefinition.
            // Here, we just replace everything.
            free(pp_state->macros[i].body);
            if (pp_state->macros[i].is_function_like && pp_state->macros[i].param_names)
            {
                for (size_t j = 0; j < pp_state->macros[i].param_count; ++j)
                {
                    free(pp_state->macros[i].param_names[j]);
                }
                free(pp_state->macros[i].param_names);
            }

            pp_state->macros[i].body = _wcsdup(body);
            pp_state->macros[i].is_function_like = is_function_like;
            pp_state->macros[i].is_variadic = is_function_like ? is_variadic : false; // Variadic only if function-like
            pp_state->macros[i].param_count = param_count;
            pp_state->macros[i].param_names = param_names; // Takes ownership

            // Check if allocations succeeded
            if (!pp_state->macros[i].body)
            {
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
                // Macro entry might be in a bad state, but we signal failure
                return false;
            }
            return true; // Redefinition successful
        }
    }

    // New macro: Check capacity
    if (pp_state->macro_count >= pp_state->macro_capacity)
    {
        size_t new_capacity = (pp_state->macro_capacity == 0) ? 8 : pp_state->macro_capacity * 2;
        BaaMacro *new_macros = realloc(pp_state->macros, new_capacity * sizeof(BaaMacro));
        if (!new_macros)
        {
            // Free params if reallocation fails, as ownership wasn't transferred
             if (is_function_like && param_names) {
                for (size_t j = 0; j < param_count; ++j) free(param_names[j]);
                free(param_names);
            }
            return false; // Reallocation failed
        }
        pp_state->macros = new_macros;
        pp_state->macro_capacity = new_capacity;
    }

    // Add the new macro
    BaaMacro *new_entry = &pp_state->macros[pp_state->macro_count];
    new_entry->name = _wcsdup(name);
    new_entry->body = _wcsdup(body);
    new_entry->is_function_like = is_function_like;
    new_entry->is_variadic = is_function_like ? is_variadic : false; // Variadic only if function-like
    new_entry->param_count = param_count;
    new_entry->param_names = param_names; // Takes ownership of the passed array and its contents

    // Check allocations
    if (!new_entry->name || !new_entry->body)
    {
        // Clean up everything allocated for this new entry on failure
        free(new_entry->name); // Safe even if NULL
        free(new_entry->body); // Safe even if NULL
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
