/**
 * @file driver_semantic.c
 * @brief Compiler-owned hover, signature, definition, and reference query data.
 */

#include "driver_semantic.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SEMANTIC_MAX_COMPLETION_ITEMS 1024

typedef struct
{
    const Node* declaration;
    const char* scope;
    int priority;
} SemanticCompletionCandidate;

typedef struct
{
    const char* logical_file;
    const char* source;
    const Node* program;
    size_t source_size;
    size_t position_byte;
    const Node* hover_node;
    size_t hover_start;
    size_t hover_end;
    const Node* signature_call;
    const Node* signature_decl;
    size_t signature_open;
    int active_parameter;
    const Node* reference_target;
    FILE* references_out;
    bool references_first;
    FILE* index_out;
    bool index_first;
    SemanticCompletionCandidate
        completion_items[SEMANTIC_MAX_COMPLETION_ITEMS];
    size_t completion_count;
} SemanticQuery;

static void semantic_json_escape(FILE* out, const char* text)
{
    fputc('"', out);
    if (text)
    {
        for (const unsigned char* p = (const unsigned char*)text; *p; ++p)
        {
            switch (*p)
            {
                case '"': fputs("\\\"", out); break;
                case '\\': fputs("\\\\", out); break;
                case '\b': fputs("\\b", out); break;
                case '\f': fputs("\\f", out); break;
                case '\n': fputs("\\n", out); break;
                case '\r': fputs("\\r", out); break;
                case '\t': fputs("\\t", out); break;
                default:
                    if (*p < 0x20u) fprintf(out, "\\u%04x", (unsigned int)*p);
                    else fputc((int)*p, out);
                    break;
            }
        }
    }
    fputc('"', out);
}

static bool semantic_append(char* buffer, size_t capacity, size_t* used,
                            const char* format, ...)
{
    if (!buffer || !used || *used >= capacity) return false;
    va_list args;
    va_start(args, format);
    const int written = vsnprintf(buffer + *used, capacity - *used, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= capacity - *used)
    {
        buffer[capacity - 1] = '\0';
        *used = capacity - 1;
        return false;
    }
    *used += (size_t)written;
    return true;
}

static const char* semantic_type_name(DataType type, const char* named_type)
{
    if (named_type && named_type[0]) return named_type;
    switch (type)
    {
        case TYPE_INT: return "صحيح";
        case TYPE_I8: return "ص٨";
        case TYPE_I16: return "ص١٦";
        case TYPE_I32: return "ص٣٢";
        case TYPE_U8: return "ط٨";
        case TYPE_U16: return "ط١٦";
        case TYPE_U32: return "ط٣٢";
        case TYPE_U64: return "ط٦٤";
        case TYPE_STRING: return "نص";
        case TYPE_BOOL: return "منطقي";
        case TYPE_CHAR: return "حرف";
        case TYPE_FLOAT: return "عشري";
        case TYPE_VOID: return "عدم";
        case TYPE_ENUM: return "تعداد";
        case TYPE_STRUCT: return "هيكل";
        case TYPE_UNION: return "اتحاد";
        case TYPE_FUNC_PTR: return "دالة";
        case TYPE_POINTER: return "مؤشر";
        default: return "غير_معروف";
    }
}

static void semantic_render_type(char* buffer,
                                 size_t capacity,
                                 size_t* used,
                                 DataType type,
                                 const char* type_name,
                                 DataType pointer_base,
                                 const char* pointer_base_name,
                                 int pointer_depth,
                                 const FuncPtrSig* function_signature);

static void semantic_render_function_pointer(char* buffer,
                                             size_t capacity,
                                             size_t* used,
                                             const FuncPtrSig* signature)
{
    semantic_append(buffer, capacity, used, "دالة(");
    if (signature)
    {
        for (int index = 0; index < signature->param_count; ++index)
        {
            if (index > 0) semantic_append(buffer, capacity, used, "، ");
            const DataType type = signature->param_types
                ? signature->param_types[index] : TYPE_INT;
            const DataType pointer_base = signature->param_ptr_base_types
                ? signature->param_ptr_base_types[index] : TYPE_INT;
            const char* pointer_name =
                signature->param_ptr_base_type_names
                    ? signature->param_ptr_base_type_names[index] : NULL;
            const int pointer_depth = signature->param_ptr_depths
                ? signature->param_ptr_depths[index] : 0;
            semantic_render_type(buffer, capacity, used, type, NULL,
                                 pointer_base, pointer_name, pointer_depth, NULL);
        }
        if (signature->is_variadic)
        {
            if (signature->param_count > 0)
                semantic_append(buffer, capacity, used, "، ");
            semantic_append(buffer, capacity, used, "...");
        }
    }
    semantic_append(buffer, capacity, used, ") -> ");
    semantic_render_type(buffer, capacity, used,
                         signature ? signature->return_type : TYPE_INT,
                         NULL,
                         signature ? signature->return_ptr_base_type : TYPE_INT,
                         signature ? signature->return_ptr_base_type_name : NULL,
                         signature ? signature->return_ptr_depth : 0,
                         NULL);
}

static void semantic_render_type(char* buffer,
                                 size_t capacity,
                                 size_t* used,
                                 DataType type,
                                 const char* type_name,
                                 DataType pointer_base,
                                 const char* pointer_base_name,
                                 int pointer_depth,
                                 const FuncPtrSig* function_signature)
{
    if (type == TYPE_FUNC_PTR)
    {
        semantic_render_function_pointer(buffer, capacity, used, function_signature);
        return;
    }
    if (type == TYPE_POINTER)
    {
        semantic_append(buffer, capacity, used, "%s",
                        semantic_type_name(pointer_base, pointer_base_name));
        const int depth = pointer_depth > 0 ? pointer_depth : 1;
        for (int index = 0; index < depth; ++index)
            semantic_append(buffer, capacity, used, "*");
        return;
    }
    semantic_append(buffer, capacity, used, "%s",
                    semantic_type_name(type, type_name));
}

static size_t semantic_byte_offset(const char* source, int line, int column)
{
    if (!source) return 0u;
    int current_line = 1;
    size_t offset = 0u;
    while (source[offset] && current_line < line)
    {
        if (source[offset++] == '\n') ++current_line;
    }
    if (current_line != line) return strlen(source);

    size_t line_end = offset;
    while (source[line_end] && source[line_end] != '\n') ++line_end;
    size_t within_line = column > 1 ? (size_t)(column - 1) : 0u;
    if (within_line > line_end - offset) within_line = line_end - offset;
    return offset + within_line;
}

static bool semantic_same_file(const Node* node, const char* logical_file)
{
    return node && node->filename && logical_file &&
           strcmp(node->filename, logical_file) == 0;
}

static bool semantic_is_declaration(const Node* node)
{
    if (!node) return false;
    switch (node->type)
    {
        case NODE_FUNC_DEF:
        case NODE_VAR_DECL:
        case NODE_ARRAY_DECL:
        case NODE_TYPE_ALIAS:
        case NODE_ENUM_DECL:
        case NODE_ENUM_MEMBER:
        case NODE_STRUCT_DECL:
        case NODE_UNION_DECL:
            return true;
        default:
            return false;
    }
}

static bool semantic_is_symbol_node(const Node* node)
{
    if (semantic_is_declaration(node)) return true;
    if (!node) return false;
    switch (node->type)
    {
        case NODE_VAR_REF:
        case NODE_CALL_EXPR:
        case NODE_CALL_STMT:
        case NODE_ARRAY_ACCESS:
        case NODE_ARRAY_ASSIGN:
        case NODE_ASSIGN:
        case NODE_READ:
        case NODE_MEMBER_ACCESS:
            return true;
        default:
            return false;
    }
}

static const Node* semantic_declaration(const Node* node)
{
    if (!node) return NULL;
    if (node->resolved_decl) return node->resolved_decl;
    return semantic_is_declaration(node) ? node : NULL;
}

static const Node* semantic_find_callable_declaration(const SemanticQuery* query,
                                                      const char* name)
{
    if (!query || !query->program || query->program->type != NODE_PROGRAM ||
        !name || !name[0])
        return NULL;
    const Node* prototype = NULL;
    for (const Node* declaration = query->program->data.program.declarations;
         declaration; declaration = declaration->next)
    {
        if (declaration->type != NODE_FUNC_DEF ||
            !declaration->data.func_def.name ||
            strcmp(declaration->data.func_def.name, name) != 0)
            continue;
        if (!declaration->data.func_def.is_prototype) return declaration;
        if (!prototype) prototype = declaration;
    }
    return prototype;
}

static const Node* semantic_find_global_value_declaration(
    const SemanticQuery* query,
    const Node* declaration)
{
    if (!query || !query->program || query->program->type != NODE_PROGRAM ||
        !declaration)
        return declaration;

    const char* name = NULL;
    bool is_global = false;
    if (declaration->type == NODE_VAR_DECL)
    {
        name = declaration->data.var_decl.name;
        is_global = declaration->data.var_decl.is_global;
    }
    else if (declaration->type == NODE_ARRAY_DECL)
    {
        name = declaration->data.array_decl.name;
        is_global = declaration->data.array_decl.is_global;
    }
    if (!is_global || !name || !name[0]) return declaration;

    const Node* first = NULL;
    for (const Node* candidate = query->program->data.program.declarations;
         candidate; candidate = candidate->next)
    {
        const char* candidate_name = NULL;
        bool candidate_global = false;
        bool candidate_extern = false;
        if (candidate->type == NODE_VAR_DECL)
        {
            candidate_name = candidate->data.var_decl.name;
            candidate_global = candidate->data.var_decl.is_global;
            candidate_extern = candidate->data.var_decl.is_extern;
        }
        else if (candidate->type == NODE_ARRAY_DECL)
        {
            candidate_name = candidate->data.array_decl.name;
            candidate_global = candidate->data.array_decl.is_global;
            candidate_extern = candidate->data.array_decl.is_extern;
        }
        if (!candidate_global || !candidate_name ||
            strcmp(candidate_name, name) != 0)
            continue;
        if (!first) first = candidate;
        if (!candidate_extern) return candidate;
    }
    return first ? first : declaration;
}

static const Node* semantic_query_declaration(const SemanticQuery* query,
                                              const Node* node)
{
    const Node* declaration = semantic_declaration(node);
    if (declaration)
    {
        if (declaration->type == NODE_FUNC_DEF)
        {
            const Node* canonical = semantic_find_callable_declaration(
                query, declaration->data.func_def.name);
            return canonical ? canonical : declaration;
        }
        return semantic_find_global_value_declaration(query, declaration);
    }
    if (node && (node->type == NODE_CALL_EXPR || node->type == NODE_CALL_STMT))
        return semantic_find_callable_declaration(query, node->data.call.name);
    return NULL;
}

static const char* semantic_node_name(const Node* node)
{
    if (!node) return NULL;
    switch (node->type)
    {
        case NODE_FUNC_DEF: return node->data.func_def.name;
        case NODE_VAR_DECL: return node->data.var_decl.name;
        case NODE_ARRAY_DECL: return node->data.array_decl.name;
        case NODE_ARRAY_ACCESS:
        case NODE_ARRAY_ASSIGN: return node->data.array_op.name;
        case NODE_TYPE_ALIAS: return node->data.type_alias.name;
        case NODE_ENUM_DECL: return node->data.enum_decl.name;
        case NODE_ENUM_MEMBER: return node->data.enum_member.name;
        case NODE_STRUCT_DECL: return node->data.struct_decl.name;
        case NODE_UNION_DECL: return node->data.union_decl.name;
        case NODE_VAR_REF: return node->data.var_ref.name;
        case NODE_CALL_EXPR:
        case NODE_CALL_STMT: return node->data.call.name;
        case NODE_ASSIGN: return node->data.assign_stmt.name;
        case NODE_READ: return node->data.read_stmt.var_name;
        case NODE_MEMBER_ACCESS: return node->data.member_access.member;
        default: return NULL;
    }
}

static const char* semantic_kind(const Node* declaration)
{
    if (!declaration) return "unknown";
    switch (declaration->type)
    {
        case NODE_FUNC_DEF: return "function";
        case NODE_VAR_DECL:
            return declaration->data.var_decl.is_const ? "constant" : "variable";
        case NODE_ARRAY_DECL: return "array";
        case NODE_TYPE_ALIAS: return "type-alias";
        case NODE_ENUM_DECL: return "enum";
        case NODE_ENUM_MEMBER: return "enum-member";
        case NODE_STRUCT_DECL: return "struct";
        case NODE_UNION_DECL: return "union";
        default: return "unknown";
    }
}

static const char* semantic_description(const Node* declaration)
{
    if (!declaration) return "رمز باء";
    switch (declaration->type)
    {
        case NODE_FUNC_DEF:
            return declaration->data.func_def.is_prototype
                ? "تصريح دالة باء" : "دالة باء";
        case NODE_VAR_DECL:
            return declaration->data.var_decl.is_const
                ? "قيمة ثابتة في باء" : "متغير في باء";
        case NODE_ARRAY_DECL: return "مصفوفة في باء";
        case NODE_TYPE_ALIAS: return "اسم بديل لنوع في باء";
        case NODE_ENUM_DECL: return "نوع تعدادي في باء";
        case NODE_ENUM_MEMBER: return "عضو تعداد في باء";
        case NODE_STRUCT_DECL: return "نوع هيكلي في باء";
        case NODE_UNION_DECL: return "نوع اتحادي في باء";
        default: return "رمز باء";
    }
}

static bool semantic_render_declaration(const Node* declaration,
                                        char* buffer,
                                        size_t capacity)
{
    if (!declaration || !buffer || capacity == 0) return false;
    buffer[0] = '\0';
    size_t used = 0u;

    switch (declaration->type)
    {
        case NODE_FUNC_DEF:
        {
            semantic_render_type(buffer, capacity, &used,
                                 declaration->data.func_def.return_type,
                                 NULL,
                                 declaration->data.func_def.return_ptr_base_type,
                                 declaration->data.func_def.return_ptr_base_type_name,
                                 declaration->data.func_def.return_ptr_depth,
                                 declaration->data.func_def.return_func_sig);
            semantic_append(buffer, capacity, &used, " %s(",
                            declaration->data.func_def.name ?
                                declaration->data.func_def.name : "");
            int parameter_index = 0;
            for (const Node* parameter = declaration->data.func_def.params;
                 parameter; parameter = parameter->next)
            {
                if (parameter->type != NODE_VAR_DECL) continue;
                if (parameter_index++ > 0)
                    semantic_append(buffer, capacity, &used, "، ");
                semantic_render_type(buffer, capacity, &used,
                                     parameter->data.var_decl.type,
                                     parameter->data.var_decl.type_name,
                                     parameter->data.var_decl.ptr_base_type,
                                     parameter->data.var_decl.ptr_base_type_name,
                                     parameter->data.var_decl.ptr_depth,
                                     parameter->data.var_decl.func_sig);
                semantic_append(buffer, capacity, &used, " %s",
                                parameter->data.var_decl.name ?
                                    parameter->data.var_decl.name : "");
            }
            if (declaration->data.func_def.is_variadic)
            {
                if (parameter_index > 0)
                    semantic_append(buffer, capacity, &used, "، ");
                semantic_append(buffer, capacity, &used, "...");
            }
            semantic_append(buffer, capacity, &used, ")");
            break;
        }
        case NODE_VAR_DECL:
            if (declaration->data.var_decl.is_const)
                semantic_append(buffer, capacity, &used, "ثابت ");
            semantic_render_type(buffer, capacity, &used,
                                 declaration->data.var_decl.type,
                                 declaration->data.var_decl.type_name,
                                 declaration->data.var_decl.ptr_base_type,
                                 declaration->data.var_decl.ptr_base_type_name,
                                 declaration->data.var_decl.ptr_depth,
                                 declaration->data.var_decl.func_sig);
            semantic_append(buffer, capacity, &used, " %s",
                            declaration->data.var_decl.name ?
                                declaration->data.var_decl.name : "");
            break;
        case NODE_ARRAY_DECL:
            if (declaration->data.array_decl.is_const)
                semantic_append(buffer, capacity, &used, "ثابت ");
            semantic_render_type(buffer, capacity, &used,
                                 declaration->data.array_decl.element_type,
                                 declaration->data.array_decl.element_type_name,
                                 declaration->data.array_decl.element_ptr_base_type,
                                 declaration->data.array_decl.element_ptr_base_type_name,
                                 declaration->data.array_decl.element_ptr_depth,
                                 NULL);
            semantic_append(buffer, capacity, &used, " %s",
                            declaration->data.array_decl.name ?
                                declaration->data.array_decl.name : "");
            for (int index = 0; index < declaration->data.array_decl.dim_count; ++index)
            {
                const int dimension = declaration->data.array_decl.dims
                    ? declaration->data.array_decl.dims[index] : 0;
                semantic_append(buffer, capacity, &used, "[%d]", dimension);
            }
            break;
        case NODE_TYPE_ALIAS:
            semantic_append(buffer, capacity, &used, "نوع %s = ",
                            declaration->data.type_alias.name ?
                                declaration->data.type_alias.name : "");
            semantic_render_type(buffer, capacity, &used,
                                 declaration->data.type_alias.target_type,
                                 declaration->data.type_alias.target_type_name,
                                 declaration->data.type_alias.target_ptr_base_type,
                                 declaration->data.type_alias.target_ptr_base_type_name,
                                 declaration->data.type_alias.target_ptr_depth,
                                 declaration->data.type_alias.target_func_sig);
            break;
        case NODE_ENUM_DECL:
            semantic_append(buffer, capacity, &used, "تعداد %s",
                            declaration->data.enum_decl.name ?
                                declaration->data.enum_decl.name : "");
            break;
        case NODE_ENUM_MEMBER:
            semantic_append(buffer, capacity, &used, "%s = %lld",
                            declaration->data.enum_member.name ?
                                declaration->data.enum_member.name : "",
                            (long long)declaration->data.enum_member.value);
            break;
        case NODE_STRUCT_DECL:
            semantic_append(buffer, capacity, &used, "هيكل %s",
                            declaration->data.struct_decl.name ?
                                declaration->data.struct_decl.name : "");
            break;
        case NODE_UNION_DECL:
            semantic_append(buffer, capacity, &used, "اتحاد %s",
                            declaration->data.union_decl.name ?
                                declaration->data.union_decl.name : "");
            break;
        default:
            return false;
    }
    return used > 0u;
}

static bool semantic_completion_declaration(const Node* declaration)
{
    if (!declaration) return false;
    switch (declaration->type)
    {
        case NODE_FUNC_DEF:
        case NODE_VAR_DECL:
        case NODE_ARRAY_DECL:
        case NODE_TYPE_ALIAS:
        case NODE_ENUM_DECL:
        case NODE_STRUCT_DECL:
        case NODE_UNION_DECL:
            return semantic_node_name(declaration) != NULL;
        default:
            return false;
    }
}

static bool semantic_completion_prefer(const Node* candidate,
                                       const Node* current)
{
    if (!candidate || !current || candidate->type != current->type)
        return false;
    if (candidate->type == NODE_FUNC_DEF)
        return current->data.func_def.is_prototype &&
               !candidate->data.func_def.is_prototype;
    if (candidate->type == NODE_VAR_DECL)
        return current->data.var_decl.is_extern &&
               !candidate->data.var_decl.is_extern;
    if (candidate->type == NODE_ARRAY_DECL)
        return current->data.array_decl.is_extern &&
               !candidate->data.array_decl.is_extern;
    return false;
}

static void semantic_completion_add(SemanticQuery* query,
                                    const Node* declaration,
                                    const char* scope,
                                    int priority)
{
    if (!query || !semantic_completion_declaration(declaration)) return;
    const char* name = semantic_node_name(declaration);
    if (!name || !name[0]) return;

    for (size_t i = 0; i < query->completion_count; ++i)
    {
        SemanticCompletionCandidate* current = &query->completion_items[i];
        const char* current_name =
            semantic_node_name(current->declaration);
        if (!current_name || strcmp(current_name, name) != 0) continue;
        if (priority > current->priority ||
            (priority == current->priority &&
             semantic_completion_prefer(declaration,
                                        current->declaration)))
        {
            current->declaration = declaration;
            current->scope = scope;
            current->priority = priority;
        }
        return;
    }

    if (query->completion_count >= SEMANTIC_MAX_COMPLETION_ITEMS) return;
    SemanticCompletionCandidate* slot =
        &query->completion_items[query->completion_count++];
    slot->declaration = declaration;
    slot->scope = scope;
    slot->priority = priority;
}

static size_t semantic_matching_brace(const SemanticQuery* query,
                                      size_t open)
{
    if (!query || open >= query->source_size ||
        query->source[open] != '{')
        return query ? query->source_size : 0u;

    int depth = 0;
    bool in_string = false;
    bool in_character = false;
    bool escaped = false;
    bool in_line_comment = false;
    bool in_block_comment = false;
    for (size_t index = open; index < query->source_size; ++index)
    {
        const unsigned char current =
            (unsigned char)query->source[index];
        const unsigned char next = index + 1 < query->source_size
            ? (unsigned char)query->source[index + 1] : 0u;
        if (in_line_comment)
        {
            if (current == '\n') in_line_comment = false;
            continue;
        }
        if (in_block_comment)
        {
            if (current == '*' && next == '/')
            {
                in_block_comment = false;
                ++index;
            }
            continue;
        }
        if (in_string || in_character)
        {
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (current == '\\')
            {
                escaped = true;
                continue;
            }
            if (in_string && current == '"') in_string = false;
            else if (in_character && current == '\'') in_character = false;
            continue;
        }
        if (current == '/' && next == '/')
        {
            in_line_comment = true;
            ++index;
            continue;
        }
        if (current == '/' && next == '*')
        {
            in_block_comment = true;
            ++index;
            continue;
        }
        if (current == '"')
        {
            in_string = true;
            continue;
        }
        if (current == '\'')
        {
            in_character = true;
            continue;
        }
        if (current == '{') ++depth;
        else if (current == '}' && --depth == 0) return index;
    }
    return query->source_size;
}

static bool semantic_block_contains(const SemanticQuery* query,
                                    const Node* block)
{
    if (!query || !block || block->type != NODE_BLOCK ||
        !semantic_same_file(block, query->logical_file))
        return false;
    const size_t open =
        semantic_byte_offset(query->source, block->line, block->col);
    const size_t close = semantic_matching_brace(query, open);
    return query->position_byte >= open &&
           query->position_byte <= close;
}

static bool semantic_completion_before_cursor(const SemanticQuery* query,
                                              const Node* node)
{
    if (!query || !semantic_same_file(node, query->logical_file))
        return false;
    return semantic_byte_offset(query->source, node->line, node->col) <
           query->position_byte;
}

static bool semantic_completion_local(const Node* node)
{
    if (!node) return false;
    if (node->type == NODE_VAR_DECL)
        return !node->data.var_decl.is_global;
    if (node->type == NODE_ARRAY_DECL)
        return !node->data.array_decl.is_global;
    return false;
}

static bool semantic_switch_span(const SemanticQuery* query,
                                 const Node* node,
                                 size_t* out_open,
                                 size_t* out_close)
{
    if (!query || !node || node->type != NODE_SWITCH ||
        !semantic_same_file(node, query->logical_file))
        return false;
    size_t cursor =
        semantic_byte_offset(query->source, node->line, node->col);
    bool in_string = false;
    bool in_character = false;
    bool escaped = false;
    bool in_line_comment = false;
    bool in_block_comment = false;
    for (; cursor < query->source_size; ++cursor)
    {
        const unsigned char current =
            (unsigned char)query->source[cursor];
        const unsigned char next = cursor + 1 < query->source_size
            ? (unsigned char)query->source[cursor + 1] : 0u;
        if (in_line_comment)
        {
            if (current == '\n') in_line_comment = false;
            continue;
        }
        if (in_block_comment)
        {
            if (current == '*' && next == '/')
            {
                in_block_comment = false;
                ++cursor;
            }
            continue;
        }
        if (in_string || in_character)
        {
            if (escaped) escaped = false;
            else if (current == '\\') escaped = true;
            else if (in_string && current == '"') in_string = false;
            else if (in_character && current == '\'') in_character = false;
            continue;
        }
        if (current == '/' && next == '/')
        {
            in_line_comment = true;
            ++cursor;
        }
        else if (current == '/' && next == '*')
        {
            in_block_comment = true;
            ++cursor;
        }
        else if (current == '"') in_string = true;
        else if (current == '\'') in_character = true;
        else if (current == '{')
        {
            if (out_open) *out_open = cursor;
            if (out_close) *out_close =
                semantic_matching_brace(query, cursor);
            return true;
        }
    }
    return false;
}

static bool semantic_has_active_scope(const SemanticQuery* query,
                                      const Node* node);
static void semantic_collect_nested_scopes(SemanticQuery* query,
                                           const Node* node,
                                           int depth);

static void semantic_collect_statement_scope(SemanticQuery* query,
                                             const Node* statements,
                                             int depth)
{
    for (const Node* current = statements; current; current = current->next)
    {
        if (semantic_completion_local(current) &&
            semantic_completion_before_cursor(query, current))
            semantic_completion_add(query, current, "local", 30 + depth);
    }
    for (const Node* current = statements; current; current = current->next)
        semantic_collect_nested_scopes(query, current, depth);
}

static bool semantic_has_active_scope(const SemanticQuery* query,
                                      const Node* node)
{
    if (!node) return false;
    switch (node->type)
    {
        case NODE_BLOCK:
            return semantic_block_contains(query, node);
        case NODE_IF:
            return semantic_has_active_scope(
                       query, node->data.if_stmt.then_branch) ||
                   semantic_has_active_scope(
                       query, node->data.if_stmt.else_branch);
        case NODE_WHILE:
            return semantic_has_active_scope(
                query, node->data.while_stmt.body);
        case NODE_FOR:
            return semantic_has_active_scope(
                query, node->data.for_stmt.body);
        case NODE_SWITCH:
        {
            size_t open = 0u;
            size_t close = 0u;
            return semantic_switch_span(query, node, &open, &close) &&
                   query->position_byte >= open &&
                   query->position_byte <= close;
        }
        case NODE_CASE:
            for (const Node* current = node->data.case_stmt.body;
                 current; current = current->next)
                if (semantic_has_active_scope(query, current)) return true;
            return false;
        default:
            return false;
    }
}

static void semantic_collect_switch_scope(SemanticQuery* query,
                                          const Node* node,
                                          int depth)
{
    size_t open = 0u;
    size_t close = 0u;
    if (!semantic_switch_span(query, node, &open, &close) ||
        query->position_byte < open || query->position_byte > close)
        return;

    const Node* active_case = NULL;
    for (const Node* current = node->data.switch_stmt.cases;
         current; current = current->next)
    {
        if (!semantic_same_file(current, query->logical_file)) continue;
        const size_t start =
            semantic_byte_offset(query->source, current->line, current->col);
        if (start > query->position_byte) break;
        active_case = current;
    }
    if (active_case)
        semantic_collect_statement_scope(
            query, active_case->data.case_stmt.body, depth + 1);
}

static void semantic_collect_nested_scopes(SemanticQuery* query,
                                           const Node* node,
                                           int depth)
{
    if (!node) return;
    switch (node->type)
    {
        case NODE_BLOCK:
            if (semantic_block_contains(query, node))
                semantic_collect_statement_scope(
                    query, node->data.block.statements, depth + 1);
            break;
        case NODE_IF:
            semantic_collect_nested_scopes(
                query, node->data.if_stmt.then_branch, depth + 1);
            semantic_collect_nested_scopes(
                query, node->data.if_stmt.else_branch, depth + 1);
            break;
        case NODE_WHILE:
            semantic_collect_nested_scopes(
                query, node->data.while_stmt.body, depth + 1);
            break;
        case NODE_FOR:
        {
            const Node* body = node->data.for_stmt.body;
            bool active = semantic_has_active_scope(query, body);
            if (!active && semantic_same_file(node, query->logical_file))
            {
                const size_t start = semantic_byte_offset(
                    query->source, node->line, node->col);
                size_t body_start = query->source_size;
                if (body && semantic_same_file(body, query->logical_file))
                    body_start = semantic_byte_offset(
                        query->source, body->line, body->col);
                active = query->position_byte >= start &&
                         query->position_byte < body_start;
            }
            if (active)
            {
                const Node* init = node->data.for_stmt.init;
                if (semantic_completion_local(init) &&
                    semantic_completion_before_cursor(query, init))
                    semantic_completion_add(
                        query, init, "local", 31 + depth);
                semantic_collect_nested_scopes(query, body, depth + 1);
            }
            break;
        }
        case NODE_SWITCH:
            semantic_collect_switch_scope(query, node, depth);
            break;
        case NODE_CASE:
            if (semantic_has_active_scope(query, node))
                semantic_collect_statement_scope(
                    query, node->data.case_stmt.body, depth + 1);
            break;
        default:
            break;
    }
}

static void semantic_collect_completion(SemanticQuery* query)
{
    if (!query || !query->program ||
        query->program->type != NODE_PROGRAM)
        return;

    const Node* active_function = NULL;
    for (const Node* declaration =
             query->program->data.program.declarations;
         declaration; declaration = declaration->next)
    {
        bool top_level = declaration->type == NODE_FUNC_DEF ||
                         declaration->type == NODE_TYPE_ALIAS ||
                         declaration->type == NODE_ENUM_DECL ||
                         declaration->type == NODE_STRUCT_DECL ||
                         declaration->type == NODE_UNION_DECL ||
                         (declaration->type == NODE_VAR_DECL &&
                          declaration->data.var_decl.is_global) ||
                         (declaration->type == NODE_ARRAY_DECL &&
                          declaration->data.array_decl.is_global);
        if (top_level)
        {
            const bool included =
                !semantic_same_file(declaration, query->logical_file);
            semantic_completion_add(
                query,
                declaration,
                included ? "included" : "global",
                included ? 10 : 11);
        }
        if (declaration->type == NODE_FUNC_DEF &&
            semantic_same_file(declaration, query->logical_file) &&
            declaration->data.func_def.body &&
            semantic_has_active_scope(
                query, declaration->data.func_def.body))
            active_function = declaration;
    }

    if (!active_function) return;
    for (const Node* parameter = active_function->data.func_def.params;
         parameter; parameter = parameter->next)
    {
        if (parameter->type == NODE_VAR_DECL)
            semantic_completion_add(
                query, parameter, "parameter", 20);
    }
    semantic_collect_nested_scopes(
        query, active_function->data.func_def.body, 0);
}

static void semantic_print_completion(FILE* out,
                                      const SemanticQuery* query)
{
    fputs("{\"items\":[", out);
    bool first = true;
    for (size_t i = 0; i < query->completion_count; ++i)
    {
        const SemanticCompletionCandidate* candidate =
            &query->completion_items[i];
        const Node* declaration = candidate->declaration;
        const char* name = semantic_node_name(declaration);
        char detail[2048] = {0};
        if (!name || !name[0] ||
            !semantic_render_declaration(
                declaration, detail, sizeof(detail)))
            continue;
        if (!first) fputc(',', out);
        fputs("{\"label\":", out);
        semantic_json_escape(out, name);
        fputs(",\"kind\":", out);
        semantic_json_escape(out, semantic_kind(declaration));
        fputs(",\"detail\":", out);
        semantic_json_escape(out, detail);
        fputs(",\"documentation\":", out);
        semantic_json_escape(out, semantic_description(declaration));
        fputs(",\"filter_text\":", out);
        semantic_json_escape(out, name);
        fputs(",\"insert_text\":", out);
        semantic_json_escape(out, name);
        fputs(",\"insert_text_format\":\"plain\",\"scope\":", out);
        semantic_json_escape(
            out, candidate->scope ? candidate->scope : "global");
        fputc('}', out);
        first = false;
    }
    fputs("]}", out);
}

static bool semantic_signature_declaration(const Node* declaration)
{
    return declaration &&
           (declaration->type == NODE_FUNC_DEF ||
            (declaration->type == NODE_VAR_DECL &&
             declaration->data.var_decl.type == TYPE_FUNC_PTR));
}

static bool semantic_call_span(const SemanticQuery* query,
                               const Node* call,
                               size_t* out_open,
                               size_t* out_close,
                               int* out_active_parameter)
{
    if (!query || !call || !out_open || !out_close || !out_active_parameter)
        return false;
    const size_t name_start =
        semantic_byte_offset(query->source, call->line, call->col);
    size_t cursor = name_start + (call->length > 0 ? (size_t)call->length : 1u);
    while (cursor < query->source_size &&
           (query->source[cursor] == ' ' || query->source[cursor] == '\t' ||
            query->source[cursor] == '\r' || query->source[cursor] == '\n'))
        ++cursor;
    if (cursor >= query->source_size || query->source[cursor] != '(') return false;

    const size_t open = cursor;
    int parenthesis_depth = 1;
    int bracket_depth = 0;
    int brace_depth = 0;
    int active_parameter = 0;
    bool in_string = false;
    bool in_character = false;
    bool escaped = false;
    bool in_line_comment = false;
    bool in_block_comment = false;
    size_t close = query->source_size;

    for (size_t index = open + 1; index < query->source_size; ++index)
    {
        const unsigned char current = (unsigned char)query->source[index];
        const unsigned char next = index + 1 < query->source_size
            ? (unsigned char)query->source[index + 1] : 0u;

        if (in_line_comment)
        {
            if (current == '\n') in_line_comment = false;
            continue;
        }
        if (in_block_comment)
        {
            if (current == '*' && next == '/')
            {
                in_block_comment = false;
                ++index;
            }
            continue;
        }
        if (in_string || in_character)
        {
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (current == '\\')
            {
                escaped = true;
                continue;
            }
            if (in_string && current == '"') in_string = false;
            else if (in_character && current == '\'') in_character = false;
            continue;
        }
        if (current == '/' && next == '/')
        {
            in_line_comment = true;
            ++index;
            continue;
        }
        if (current == '/' && next == '*')
        {
            in_block_comment = true;
            ++index;
            continue;
        }
        if (current == '"')
        {
            in_string = true;
            continue;
        }
        if (current == '\'')
        {
            in_character = true;
            continue;
        }
        if (current == '(') ++parenthesis_depth;
        else if (current == ')')
        {
            --parenthesis_depth;
            if (parenthesis_depth == 0)
            {
                close = index;
                break;
            }
        }
        else if (current == '[') ++bracket_depth;
        else if (current == ']' && bracket_depth > 0) --bracket_depth;
        else if (current == '{') ++brace_depth;
        else if (current == '}' && brace_depth > 0) --brace_depth;
        else if (parenthesis_depth == 1 && bracket_depth == 0 && brace_depth == 0 &&
                 index < query->position_byte &&
                 (current == ',' || (current == 0xD8u && next == 0x8Cu)))
        {
            ++active_parameter;
            if (current == 0xD8u) ++index;
        }
    }

    if (query->position_byte <= open || query->position_byte > close) return false;
    *out_open = open;
    *out_close = close;
    *out_active_parameter = active_parameter;
    return true;
}

static const char* semantic_symbol_domain(const Node* declaration)
{
    if (!declaration) return "unknown";
    if (declaration->type == NODE_FUNC_DEF) return "external";
    if (declaration->type == NODE_VAR_DECL)
    {
        if (!declaration->data.var_decl.is_global) return "local";
        return declaration->data.var_decl.is_static ? "file" : "external";
    }
    if (declaration->type == NODE_ARRAY_DECL)
    {
        if (!declaration->data.array_decl.is_global) return "local";
        return declaration->data.array_decl.is_static ? "file" : "external";
    }
    return "declaration";
}

static void semantic_print_symbol_identity(FILE* out,
                                           const Node* declaration)
{
    if (!declaration)
    {
        fputs("null", out);
        return;
    }

    const char* domain = semantic_symbol_domain(declaration);
    fputs("{\"domain\":", out);
    semantic_json_escape(out, domain);
    fputs(",\"kind\":", out);
    semantic_json_escape(out, semantic_kind(declaration));
    fputs(",\"name\":", out);
    semantic_json_escape(out, semantic_node_name(declaration));
    if (strcmp(domain, "external") != 0)
    {
        fputs(",\"declaration\":{\"file\":", out);
        semantic_json_escape(out, declaration->filename
            ? declaration->filename : "");
        fprintf(out, ",\"line\":%d,\"column\":%d}",
                declaration->line, declaration->col);
    }
    fputc('}', out);
}

static const char* semantic_occurrence_role(const Node* node)
{
    if (!semantic_is_declaration(node)) return "reference";
    if (node->type == NODE_FUNC_DEF)
        return node->data.func_def.is_prototype ? "declaration" : "definition";
    if (node->type == NODE_VAR_DECL)
        return node->data.var_decl.is_extern ? "declaration" : "definition";
    if (node->type == NODE_ARRAY_DECL)
        return node->data.array_decl.is_extern ? "declaration" : "definition";
    return "definition";
}

static void semantic_print_symbol_location(FILE* out,
                                           const SemanticQuery* query,
                                           const Node* node,
                                           const char* role)
{
    const bool in_root = semantic_same_file(node, query->logical_file);
    const size_t start = in_root
        ? semantic_byte_offset(query->source, node->line, node->col) : 0u;
    size_t end = start + (node->length > 0 ? (size_t)node->length : 1u);
    if (in_root && end > query->source_size) end = query->source_size;

    fputs("{\"file\":", out);
    semantic_json_escape(out, node->filename ? node->filename : "");
    fputs(",\"name\":", out);
    semantic_json_escape(out, semantic_node_name(node));
    fputs(",\"kind\":", out);
    semantic_json_escape(out, semantic_kind(
        semantic_query_declaration(query, node)));
    if (role)
    {
        fputs(",\"role\":", out);
        semantic_json_escape(out, role);
    }
    fputs(",\"range\":{\"start\":{", out);
    fprintf(out, "\"line\":%d,\"column\":%d", node->line, node->col);
    if (in_root) fprintf(out, ",\"byte\":%zu", start);
    fputs("},\"end\":{", out);
    fprintf(out, "\"line\":%d,\"column\":%d",
            node->line, node->col + (node->length > 0 ? node->length : 1));
    if (in_root) fprintf(out, ",\"byte\":%zu", end);
    fputs("}}}", out);
}

static void semantic_print_index_occurrence(SemanticQuery* query,
                                            const Node* node,
                                            const Node* declaration)
{
    if (!query || !query->index_out || !node || !declaration) return;
    if (!query->index_first) fputc(',', query->index_out);
    fputs("{\"symbol\":", query->index_out);
    semantic_print_symbol_identity(query->index_out, declaration);
    fputs(",\"role\":", query->index_out);
    semantic_json_escape(query->index_out, semantic_occurrence_role(node));
    fputs(",\"location\":", query->index_out);
    semantic_print_symbol_location(
        query->index_out, query, node, NULL);
    fputc('}', query->index_out);
    query->index_first = false;
}

static bool semantic_same_declaration(const Node* left, const Node* right)
{
    if (left == right) return true;
    if (!left || !right || left->type != right->type ||
        left->line != right->line || left->col != right->col)
        return false;
    if (!left->filename || !right->filename ||
        strcmp(left->filename, right->filename) != 0)
        return false;
    const char* left_name = semantic_node_name(left);
    const char* right_name = semantic_node_name(right);
    return left_name && right_name && strcmp(left_name, right_name) == 0;
}

static void semantic_consider_node(SemanticQuery* query, const Node* node)
{
    if (!query || !node) return;
    if (query->index_out)
    {
        if (semantic_is_symbol_node(node))
        {
            const Node* declaration =
                semantic_query_declaration(query, node);
            if (declaration)
                semantic_print_index_occurrence(query, node, declaration);
        }
        return;
    }
    if (query->reference_target)
    {
        if (semantic_is_symbol_node(node) &&
            semantic_same_declaration(
                semantic_query_declaration(query, node),
                query->reference_target))
        {
            if (!query->references_first) fputc(',', query->references_out);
            semantic_print_symbol_location(
                query->references_out, query, node,
                semantic_is_declaration(node) ? "declaration" : "reference");
            query->references_first = false;
        }
        return;
    }
    if (!semantic_same_file(node, query->logical_file)) return;
    if (semantic_is_symbol_node(node))
    {
        const size_t start = semantic_byte_offset(query->source, node->line, node->col);
        size_t end = start + (node->length > 0 ? (size_t)node->length : 1u);
        if (end > query->source_size) end = query->source_size;
        if (query->position_byte >= start && query->position_byte < end &&
            semantic_query_declaration(query, node))
        {
            const size_t current_width = query->hover_node
                ? query->hover_end - query->hover_start : SIZE_MAX;
            if (!query->hover_node || end - start <= current_width)
            {
                query->hover_node = node;
                query->hover_start = start;
                query->hover_end = end;
            }
        }
    }

    if (node->type == NODE_CALL_EXPR || node->type == NODE_CALL_STMT)
    {
        const Node* declaration = semantic_query_declaration(query, node);
        size_t open = 0u;
        size_t close = 0u;
        int active_parameter = 0;
        if (semantic_signature_declaration(declaration) &&
            semantic_call_span(query, node, &open, &close, &active_parameter) &&
            (!query->signature_call || open >= query->signature_open))
        {
            query->signature_call = node;
            query->signature_decl = declaration;
            query->signature_open = open;
            query->active_parameter = active_parameter;
        }
    }
}

static void semantic_visit_list(SemanticQuery* query, const Node* node);

static void semantic_visit_one(SemanticQuery* query, const Node* node)
{
    if (!node) return;
    semantic_consider_node(query, node);
    switch (node->type)
    {
        case NODE_PROGRAM:
            semantic_visit_list(query, node->data.program.declarations);
            break;
        case NODE_FUNC_DEF:
            semantic_visit_list(query, node->data.func_def.params);
            semantic_visit_list(query, node->data.func_def.body);
            break;
        case NODE_VAR_DECL:
            semantic_visit_list(query, node->data.var_decl.expression);
            semantic_visit_list(query, node->data.var_decl.struct_init_values);
            break;
        case NODE_ENUM_DECL:
            semantic_visit_list(query, node->data.enum_decl.members);
            break;
        case NODE_STRUCT_DECL:
            semantic_visit_list(query, node->data.struct_decl.fields);
            break;
        case NODE_UNION_DECL:
            semantic_visit_list(query, node->data.union_decl.fields);
            break;
        case NODE_BLOCK:
            semantic_visit_list(query, node->data.block.statements);
            break;
        case NODE_MEMBER_ACCESS:
            semantic_visit_list(query, node->data.member_access.base);
            break;
        case NODE_MEMBER_ASSIGN:
            semantic_visit_list(query, node->data.member_assign.target);
            semantic_visit_list(query, node->data.member_assign.value);
            break;
        case NODE_STRUCT_FIELD_INIT:
            semantic_visit_list(query, node->data.struct_field_init.value);
            break;
        case NODE_DEREF_ASSIGN:
            semantic_visit_list(query, node->data.deref_assign.target);
            semantic_visit_list(query, node->data.deref_assign.value);
            break;
        case NODE_ARRAY_DECL:
            semantic_visit_list(query, node->data.array_decl.init_values);
            break;
        case NODE_ARRAY_ACCESS:
        case NODE_ARRAY_ASSIGN:
            semantic_visit_list(query, node->data.array_op.indices);
            semantic_visit_list(query, node->data.array_op.value);
            break;
        case NODE_CALL_EXPR:
        case NODE_CALL_STMT:
            semantic_visit_list(query, node->data.call.args);
            break;
        case NODE_INLINE_ASM:
            semantic_visit_list(query, node->data.inline_asm.templates);
            semantic_visit_list(query, node->data.inline_asm.outputs);
            semantic_visit_list(query, node->data.inline_asm.inputs);
            break;
        case NODE_ASM_OPERAND:
            semantic_visit_list(query, node->data.asm_operand.expression);
            break;
        case NODE_IF:
            semantic_visit_list(query, node->data.if_stmt.condition);
            semantic_visit_list(query, node->data.if_stmt.then_branch);
            semantic_visit_list(query, node->data.if_stmt.else_branch);
            break;
        case NODE_WHILE:
            semantic_visit_list(query, node->data.while_stmt.condition);
            semantic_visit_list(query, node->data.while_stmt.body);
            break;
        case NODE_FOR:
            semantic_visit_list(query, node->data.for_stmt.init);
            semantic_visit_list(query, node->data.for_stmt.condition);
            semantic_visit_list(query, node->data.for_stmt.increment);
            semantic_visit_list(query, node->data.for_stmt.body);
            break;
        case NODE_SWITCH:
            semantic_visit_list(query, node->data.switch_stmt.expression);
            semantic_visit_list(query, node->data.switch_stmt.cases);
            break;
        case NODE_CASE:
            semantic_visit_list(query, node->data.case_stmt.value);
            semantic_visit_list(query, node->data.case_stmt.body);
            break;
        case NODE_RETURN:
            semantic_visit_list(query, node->data.return_stmt.expression);
            break;
        case NODE_PRINT:
            semantic_visit_list(query, node->data.print_stmt.expression);
            break;
        case NODE_ASSIGN:
            semantic_visit_list(query, node->data.assign_stmt.expression);
            break;
        case NODE_BIN_OP:
            semantic_visit_list(query, node->data.bin_op.left);
            semantic_visit_list(query, node->data.bin_op.right);
            break;
        case NODE_UNARY_OP:
        case NODE_POSTFIX_OP:
            semantic_visit_list(query, node->data.unary_op.operand);
            break;
        case NODE_CAST:
            semantic_visit_list(query, node->data.cast_expr.expression);
            break;
        case NODE_SIZEOF:
            semantic_visit_list(query, node->data.sizeof_expr.expression);
            break;
        default:
            break;
    }
}

static void semantic_visit_list(SemanticQuery* query, const Node* node)
{
    for (const Node* current = node; current; current = current->next)
        semantic_visit_one(query, current);
}

static void semantic_print_span(FILE* out,
                                const Node* node,
                                const char* source,
                                size_t start,
                                size_t end)
{
    (void)source;
    fputs("{\"start\":{", out);
    fprintf(out, "\"line\":%d,\"column\":%d,\"byte\":%zu",
            node->line, node->col, start);
    fputs("},\"end\":{", out);
    fprintf(out, "\"line\":%d,\"column\":%d,\"byte\":%zu",
            node->line, node->col + (node->length > 0 ? node->length : 1), end);
    fputs("}}", out);
}

static void semantic_print_declaration_location(FILE* out, const Node* declaration)
{
    fputs("{\"file\":", out);
    semantic_json_escape(out, declaration && declaration->filename
        ? declaration->filename : "");
    fprintf(out, ",\"line\":%d,\"column\":%d}",
            declaration ? declaration->line : 1,
            declaration ? declaration->col : 1);
}

static void semantic_print_hover(FILE* out, const SemanticQuery* query)
{
    if (!query->hover_node)
    {
        fputs("null", out);
        return;
    }
    const Node* declaration =
        semantic_query_declaration(query, query->hover_node);
    char display[2048];
    if (!semantic_render_declaration(declaration, display, sizeof(display)))
    {
        fputs("null", out);
        return;
    }

    fputs("{\"name\":", out);
    semantic_json_escape(out, semantic_node_name(query->hover_node));
    fputs(",\"kind\":", out);
    semantic_json_escape(out, semantic_kind(declaration));
    fputs(",\"display\":", out);
    semantic_json_escape(out, display);
    fputs(",\"description\":", out);
    semantic_json_escape(out, semantic_description(declaration));
    fputs(",\"range\":", out);
    semantic_print_span(out, query->hover_node, query->source,
                        query->hover_start, query->hover_end);
    fputs(",\"declaration\":", out);
    semantic_print_declaration_location(out, declaration);
    fputc('}', out);
}

static void semantic_print_signature_parameters(FILE* out, const Node* declaration)
{
    fputc('[', out);
    bool first = true;
    if (declaration->type == NODE_FUNC_DEF)
    {
        for (const Node* parameter = declaration->data.func_def.params;
             parameter; parameter = parameter->next)
        {
            if (parameter->type != NODE_VAR_DECL) continue;
            char label[1024] = {0};
            size_t used = 0u;
            semantic_render_type(label, sizeof(label), &used,
                                 parameter->data.var_decl.type,
                                 parameter->data.var_decl.type_name,
                                 parameter->data.var_decl.ptr_base_type,
                                 parameter->data.var_decl.ptr_base_type_name,
                                 parameter->data.var_decl.ptr_depth,
                                 parameter->data.var_decl.func_sig);
            semantic_append(label, sizeof(label), &used, " %s",
                            parameter->data.var_decl.name ?
                                parameter->data.var_decl.name : "");
            if (!first) fputc(',', out);
            fputs("{\"label\":", out);
            semantic_json_escape(out, label);
            fputc('}', out);
            first = false;
        }
    }
    else if (declaration->type == NODE_VAR_DECL &&
             declaration->data.var_decl.type == TYPE_FUNC_PTR)
    {
        const FuncPtrSig* signature = declaration->data.var_decl.func_sig;
        for (int index = 0; signature && index < signature->param_count; ++index)
        {
            char label[1024] = {0};
            size_t used = 0u;
            semantic_render_type(label, sizeof(label), &used,
                                 signature->param_types
                                    ? signature->param_types[index] : TYPE_INT,
                                 NULL,
                                 signature->param_ptr_base_types
                                    ? signature->param_ptr_base_types[index] : TYPE_INT,
                                 signature->param_ptr_base_type_names
                                    ? signature->param_ptr_base_type_names[index] : NULL,
                                 signature->param_ptr_depths
                                    ? signature->param_ptr_depths[index] : 0,
                                 NULL);
            if (!first) fputc(',', out);
            fputs("{\"label\":", out);
            semantic_json_escape(out, label);
            fputc('}', out);
            first = false;
        }
    }
    fputc(']', out);
}

static void semantic_print_signature(FILE* out, const SemanticQuery* query)
{
    const Node* declaration = query->signature_decl;
    if (!query->signature_call || !semantic_signature_declaration(declaration))
    {
        fputs("null", out);
        return;
    }
    char label[2048];
    if (!semantic_render_declaration(declaration, label, sizeof(label)))
    {
        fputs("null", out);
        return;
    }

    int parameter_count = 0;
    bool variadic = false;
    if (declaration->type == NODE_FUNC_DEF)
    {
        for (const Node* parameter = declaration->data.func_def.params;
             parameter; parameter = parameter->next)
            if (parameter->type == NODE_VAR_DECL) ++parameter_count;
        variadic = declaration->data.func_def.is_variadic;
    }
    else
    {
        const FuncPtrSig* signature = declaration->data.var_decl.func_sig;
        parameter_count = signature ? signature->param_count : 0;
        variadic = signature ? signature->is_variadic : false;
    }
    int active = query->active_parameter;
    if (!variadic && parameter_count > 0 && active >= parameter_count)
        active = parameter_count - 1;
    if (active < 0) active = 0;

    fputs("{\"name\":", out);
    semantic_json_escape(out, semantic_node_name(query->signature_call));
    fputs(",\"label\":", out);
    semantic_json_escape(out, label);
    fprintf(out, ",\"active_parameter\":%d,\"variadic\":%s,\"parameters\":",
            active, variadic ? "true" : "false");
    semantic_print_signature_parameters(out, declaration);
    fputs(",\"declaration\":", out);
    semantic_print_declaration_location(out, declaration);
    fputc('}', out);
}

static const Node* semantic_selected_declaration(const SemanticQuery* query)
{
    return query && query->hover_node
        ? semantic_query_declaration(query, query->hover_node) : NULL;
}

static void semantic_print_definition(FILE* out, const SemanticQuery* query)
{
    const Node* declaration = semantic_selected_declaration(query);
    if (!declaration)
    {
        fputs("null", out);
        return;
    }
    semantic_print_symbol_location(out, query, declaration, NULL);
}

static void semantic_print_references(FILE* out, const SemanticQuery* query)
{
    const Node* declaration = semantic_selected_declaration(query);
    fputc('[', out);
    if (declaration)
    {
        SemanticQuery reference_query = *query;
        reference_query.reference_target = declaration;
        reference_query.references_out = out;
        reference_query.references_first = true;
        semantic_visit_one(&reference_query, query->program);
    }
    fputc(']', out);
}

bool driver_semantic_query_json_write(FILE* out,
                                      const char* compiler_version,
                                      const char* logical_file,
                                      const char* source,
                                      const Node* program,
                                      size_t position_byte)
{
    if (!out || !logical_file || !source || !program ||
        program->type != NODE_PROGRAM)
        return false;

    SemanticQuery query;
    memset(&query, 0, sizeof(query));
    query.logical_file = logical_file;
    query.source = source;
    query.program = program;
    query.source_size = strlen(source);
    query.position_byte = position_byte <= query.source_size
        ? position_byte : query.source_size;
    semantic_visit_one(&query, program);
    semantic_collect_completion(&query);

    fputs("{\"schema_version\":\"semantic-query-json-v1\",\"compiler_version\":", out);
    semantic_json_escape(out, compiler_version ? compiler_version : "");
    fputs(",\"file\":", out);
    semantic_json_escape(out, logical_file);
    fprintf(out,
            ",\"position_encoding\":\"utf-8-bytes\",\"position_byte\":%zu,\"symbol\":",
            query.position_byte);
    semantic_print_symbol_identity(out, semantic_selected_declaration(&query));
    fputs(",\"hover\":", out);
    semantic_print_hover(out, &query);
    fputs(",\"signature_help\":", out);
    semantic_print_signature(out, &query);
    fputs(",\"definition\":", out);
    semantic_print_definition(out, &query);
    fputs(",\"references\":", out);
    semantic_print_references(out, &query);
    fputs(",\"completion\":", out);
    semantic_print_completion(out, &query);
    fputs("}\n", out);
    return ferror(out) == 0;
}

bool driver_semantic_index_json_write(FILE* out,
                                      const char* compiler_version,
                                      const char* logical_file,
                                      const char* source,
                                      const Node* program)
{
    if (!out || !logical_file || !source || !program ||
        program->type != NODE_PROGRAM)
        return false;

    SemanticQuery query;
    memset(&query, 0, sizeof(query));
    query.logical_file = logical_file;
    query.source = source;
    query.program = program;
    query.source_size = strlen(source);
    query.index_out = out;
    query.index_first = true;

    fputs("{\"schema_version\":\"semantic-index-json-v1\",\"compiler_version\":",
          out);
    semantic_json_escape(out, compiler_version ? compiler_version : "");
    fputs(",\"file\":", out);
    semantic_json_escape(out, logical_file);
    fputs(",\"position_encoding\":\"utf-8-bytes\",\"occurrences\":[", out);
    semantic_visit_one(&query, program);
    fputs("]}\n", out);
    return ferror(out) == 0;
}
