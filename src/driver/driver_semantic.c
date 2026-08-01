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

static bool semantic_node_in_list(const Node* list, const Node* target)
{
    for (const Node* node = list; node; node = node->next)
        if (node == target) return true;
    return false;
}

static const char* semantic_declaration_context_kind(
    const SemanticQuery* query,
    const Node* declaration)
{
    if (!query || !query->program ||
        query->program->type != NODE_PROGRAM || !declaration)
        return NULL;

    for (const Node* owner = query->program->data.program.declarations;
         owner; owner = owner->next)
    {
        if (owner->type == NODE_FUNC_DEF &&
            semantic_node_in_list(owner->data.func_def.params, declaration))
            return "parameter";
        if (owner->type == NODE_STRUCT_DECL &&
            semantic_node_in_list(owner->data.struct_decl.fields, declaration))
            return "field";
        if (owner->type == NODE_UNION_DECL &&
            semantic_node_in_list(owner->data.union_decl.fields, declaration))
            return "field";
    }
    return NULL;
}

static const char* semantic_kind(const SemanticQuery* query,
                                 const Node* declaration)
{
    if (!declaration) return "unknown";
    const char* context_kind =
        semantic_declaration_context_kind(query, declaration);
    if (context_kind) return context_kind;
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

#include "driver_semantic_completion.inc"

#include "driver_semantic_output.inc"

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
    semantic_print_symbol_identity(
        out, &query, semantic_selected_declaration(&query));
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
