/**
 * @file driver_symbols.c
 * @brief Stable document declaration metadata for editor tooling.
 */

#include "driver_symbols.h"

#include <stdint.h>
#include <string.h>

static void symbols_json_escape(FILE* out, const char* text)
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

static const char* symbols_type_kind(DataType type)
{
    switch (type)
    {
        case TYPE_INT: return "int";
        case TYPE_I8: return "i8";
        case TYPE_I16: return "i16";
        case TYPE_I32: return "i32";
        case TYPE_U8: return "u8";
        case TYPE_U16: return "u16";
        case TYPE_U32: return "u32";
        case TYPE_U64: return "u64";
        case TYPE_STRING: return "string";
        case TYPE_POINTER: return "pointer";
        case TYPE_FUNC_PTR: return "function-pointer";
        case TYPE_BOOL: return "bool";
        case TYPE_CHAR: return "char";
        case TYPE_FLOAT: return "float";
        case TYPE_VOID: return "void";
        case TYPE_ENUM: return "enum";
        case TYPE_STRUCT: return "struct";
        case TYPE_UNION: return "union";
        default: return "unknown";
    }
}

static const char* symbols_type_display(DataType type, const char* named_type)
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
        case TYPE_POINTER: return "مؤشر";
        case TYPE_FUNC_PTR: return "دالة";
        case TYPE_BOOL: return "منطقي";
        case TYPE_CHAR: return "حرف";
        case TYPE_FLOAT: return "عشري";
        case TYPE_VOID: return "عدم";
        case TYPE_ENUM: return "تعداد";
        case TYPE_STRUCT: return "هيكل";
        case TYPE_UNION: return "اتحاد";
        default: return "غير_معروف";
    }
}

static void symbols_print_type(FILE* out,
                               DataType type,
                               const char* type_name,
                               DataType pointer_base,
                               const char* pointer_base_name,
                               int pointer_depth)
{
    fprintf(out, "{\"kind\":");
    symbols_json_escape(out, symbols_type_kind(type));
    fputs(",\"display\":", out);
    symbols_json_escape(out, symbols_type_display(type, type_name));
    if (type == TYPE_POINTER)
    {
        fputs(",\"pointer_depth\":", out);
        fprintf(out, "%d", pointer_depth > 0 ? pointer_depth : 1);
        fputs(",\"base_kind\":", out);
        symbols_json_escape(out, symbols_type_kind(pointer_base));
        fputs(",\"base_display\":", out);
        symbols_json_escape(out, symbols_type_display(pointer_base, pointer_base_name));
    }
    fputc('}', out);
}

static size_t symbols_byte_offset(const char* source, int line, int column)
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

static bool symbols_same_file(const Node* node, const char* logical_file)
{
    return node && node->filename && logical_file &&
           strcmp(node->filename, logical_file) == 0;
}

static const char* symbols_node_name(const Node* node)
{
    if (!node) return NULL;
    switch (node->type)
    {
        case NODE_FUNC_DEF: return node->data.func_def.name;
        case NODE_VAR_DECL: return node->data.var_decl.name;
        case NODE_ARRAY_DECL: return node->data.array_decl.name;
        case NODE_TYPE_ALIAS: return node->data.type_alias.name;
        case NODE_ENUM_DECL: return node->data.enum_decl.name;
        case NODE_ENUM_MEMBER: return node->data.enum_member.name;
        case NODE_STRUCT_DECL: return node->data.struct_decl.name;
        case NODE_UNION_DECL: return node->data.union_decl.name;
        default: return NULL;
    }
}

static const char* symbols_node_kind(const Node* node, const char* scope)
{
    if (!node) return NULL;
    switch (node->type)
    {
        case NODE_FUNC_DEF: return "function";
        case NODE_VAR_DECL:
            return scope && strcmp(scope, "parameter") == 0 ? "parameter" :
                   scope && strcmp(scope, "member") == 0 ? "field" : "variable";
        case NODE_ARRAY_DECL: return "array";
        case NODE_TYPE_ALIAS: return "type-alias";
        case NODE_ENUM_DECL: return "enum";
        case NODE_ENUM_MEMBER: return "enum-member";
        case NODE_STRUCT_DECL: return "struct";
        case NODE_UNION_DECL: return "union";
        default: return NULL;
    }
}

static const Node* symbols_children(const Node* node, const char** child_scope)
{
    if (child_scope) *child_scope = "member";
    if (!node) return NULL;
    switch (node->type)
    {
        case NODE_FUNC_DEF:
            if (child_scope) *child_scope = "parameter";
            return node->data.func_def.params;
        case NODE_ENUM_DECL: return node->data.enum_decl.members;
        case NODE_STRUCT_DECL: return node->data.struct_decl.fields;
        case NODE_UNION_DECL: return node->data.union_decl.fields;
        default: return NULL;
    }
}

static void symbols_print_node_type(FILE* out, const Node* node)
{
    switch (node->type)
    {
        case NODE_FUNC_DEF:
            fputs(",\"return_type\":", out);
            symbols_print_type(out,
                               node->data.func_def.return_type,
                               NULL,
                               node->data.func_def.return_ptr_base_type,
                               node->data.func_def.return_ptr_base_type_name,
                               node->data.func_def.return_ptr_depth);
            break;
        case NODE_VAR_DECL:
            fputs(",\"type\":", out);
            symbols_print_type(out,
                               node->data.var_decl.type,
                               node->data.var_decl.type_name,
                               node->data.var_decl.ptr_base_type,
                               node->data.var_decl.ptr_base_type_name,
                               node->data.var_decl.ptr_depth);
            break;
        case NODE_ARRAY_DECL:
            fputs(",\"element_type\":", out);
            symbols_print_type(out,
                               node->data.array_decl.element_type,
                               node->data.array_decl.element_type_name,
                               node->data.array_decl.element_ptr_base_type,
                               node->data.array_decl.element_ptr_base_type_name,
                               node->data.array_decl.element_ptr_depth);
            fprintf(out, ",\"array_rank\":%d", node->data.array_decl.dim_count);
            break;
        case NODE_TYPE_ALIAS:
            fputs(",\"target_type\":", out);
            symbols_print_type(out,
                               node->data.type_alias.target_type,
                               node->data.type_alias.target_type_name,
                               node->data.type_alias.target_ptr_base_type,
                               node->data.type_alias.target_ptr_base_type_name,
                               node->data.type_alias.target_ptr_depth);
            break;
        default: break;
    }
}

static void symbols_print_modifiers(FILE* out, const Node* node)
{
    bool is_const = false;
    bool is_static = false;
    bool is_extern = false;
    bool is_prototype = false;
    switch (node->type)
    {
        case NODE_FUNC_DEF:
            is_extern = node->data.func_def.is_extern;
            is_prototype = node->data.func_def.is_prototype;
            break;
        case NODE_VAR_DECL:
            is_const = node->data.var_decl.is_const;
            is_static = node->data.var_decl.is_static;
            is_extern = node->data.var_decl.is_extern;
            break;
        case NODE_ARRAY_DECL:
            is_const = node->data.array_decl.is_const;
            is_static = node->data.array_decl.is_static;
            is_extern = node->data.array_decl.is_extern;
            break;
        default: break;
    }
    fprintf(out,
            ",\"modifiers\":{\"const\":%s,\"static\":%s,\"extern\":%s,\"prototype\":%s}",
            is_const ? "true" : "false",
            is_static ? "true" : "false",
            is_extern ? "true" : "false",
            is_prototype ? "true" : "false");
}

static bool symbols_print_node(FILE* out,
                               const Node* node,
                               const char* logical_file,
                               const char* source,
                               const char* scope)
{
    const char* name = symbols_node_name(node);
    const char* kind = symbols_node_kind(node, scope);
    if (!name || !name[0] || !kind || !symbols_same_file(node, logical_file)) return false;

    const size_t start_byte = symbols_byte_offset(source, node->line, node->col);
    const size_t source_size = source ? strlen(source) : 0u;
    size_t end_byte = start_byte + (node->length > 0 ? (size_t)node->length : 1u);
    if (end_byte > source_size) end_byte = source_size;

    fputs("{\"name\":", out);
    symbols_json_escape(out, name);
    fputs(",\"kind\":", out);
    symbols_json_escape(out, kind);
    fputs(",\"scope\":", out);
    symbols_json_escape(out, scope ? scope : "global");
    fputs(",\"span\":{\"start\":{", out);
    fprintf(out, "\"line\":%d,\"column\":%d,\"byte\":%zu",
            node->line, node->col, start_byte);
    fputs("},\"end\":{", out);
    fprintf(out, "\"line\":%d,\"column\":%d,\"byte\":%zu",
            node->line, node->col + node->length, end_byte);
    fputs("}}", out);
    symbols_print_node_type(out, node);
    symbols_print_modifiers(out, node);

    const char* child_scope = "member";
    const Node* child = symbols_children(node, &child_scope);
    if (child)
    {
        fputs(",\"children\":[", out);
        bool first = true;
        for (; child; child = child->next)
        {
            if (!symbols_node_name(child) ||
                !symbols_same_file(child, logical_file)) continue;
            if (!first) fputc(',', out);
            if (symbols_print_node(out, child, logical_file, source, child_scope))
                first = false;
        }
        fputc(']', out);
    }
    fputc('}', out);
    return true;
}

bool driver_symbols_json_write(FILE* out,
                               const char* compiler_version,
                               const char* logical_file,
                               const char* source,
                               const Node* program)
{
    if (!out || !logical_file || !source || !program || program->type != NODE_PROGRAM)
        return false;

    fputs("{\"schema_version\":\"symbols-json-v1\",\"compiler_version\":", out);
    symbols_json_escape(out, compiler_version ? compiler_version : "");
    fputs(",\"file\":", out);
    symbols_json_escape(out, logical_file);
    fputs(",\"position_encoding\":\"utf-8-bytes\",\"symbols\":[", out);

    bool first = true;
    for (const Node* node = program->data.program.declarations; node; node = node->next)
    {
        if (!symbols_node_name(node) || !symbols_same_file(node, logical_file)) continue;
        if (!first) fputc(',', out);
        if (symbols_print_node(out, node, logical_file, source, "global")) first = false;
    }
    fputs("]}\n", out);
    return ferror(out) == 0;
}
