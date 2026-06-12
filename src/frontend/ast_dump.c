/**
 * @file ast_dump.c
 * @brief طابعة بنيوية مستقرة لشجرة AST قبل التحليل الدلالي.
 */

#include "ast_dump.h"

#include <inttypes.h>

#define AST_DUMP_INDENT_WIDTH 2
#define AST_DUMP_ASCII_CONTROL_LIMIT 32u

static const char* ast_dump_node_type_name(NodeType type)
{
    switch (type) {
        case NODE_PROGRAM: return "NODE_PROGRAM";
        case NODE_FUNC_DEF: return "NODE_FUNC_DEF";
        case NODE_VAR_DECL: return "NODE_VAR_DECL";
        case NODE_TYPE_ALIAS: return "NODE_TYPE_ALIAS";
        case NODE_ENUM_DECL: return "NODE_ENUM_DECL";
        case NODE_STRUCT_DECL: return "NODE_STRUCT_DECL";
        case NODE_ENUM_MEMBER: return "NODE_ENUM_MEMBER";
        case NODE_UNION_DECL: return "NODE_UNION_DECL";
        case NODE_BLOCK: return "NODE_BLOCK";
        case NODE_RETURN: return "NODE_RETURN";
        case NODE_PRINT: return "NODE_PRINT";
        case NODE_IF: return "NODE_IF";
        case NODE_WHILE: return "NODE_WHILE";
        case NODE_FOR: return "NODE_FOR";
        case NODE_SWITCH: return "NODE_SWITCH";
        case NODE_CASE: return "NODE_CASE";
        case NODE_BREAK: return "NODE_BREAK";
        case NODE_CONTINUE: return "NODE_CONTINUE";
        case NODE_ASSIGN: return "NODE_ASSIGN";
        case NODE_CALL_STMT: return "NODE_CALL_STMT";
        case NODE_READ: return "NODE_READ";
        case NODE_INLINE_ASM: return "NODE_INLINE_ASM";
        case NODE_ASM_OPERAND: return "NODE_ASM_OPERAND";
        case NODE_ARRAY_DECL: return "NODE_ARRAY_DECL";
        case NODE_ARRAY_ASSIGN: return "NODE_ARRAY_ASSIGN";
        case NODE_ARRAY_ACCESS: return "NODE_ARRAY_ACCESS";
        case NODE_MEMBER_ACCESS: return "NODE_MEMBER_ACCESS";
        case NODE_MEMBER_ASSIGN: return "NODE_MEMBER_ASSIGN";
        case NODE_DEREF_ASSIGN: return "NODE_DEREF_ASSIGN";
        case NODE_BIN_OP: return "NODE_BIN_OP";
        case NODE_UNARY_OP: return "NODE_UNARY_OP";
        case NODE_POSTFIX_OP: return "NODE_POSTFIX_OP";
        case NODE_INT: return "NODE_INT";
        case NODE_FLOAT: return "NODE_FLOAT";
        case NODE_STRING: return "NODE_STRING";
        case NODE_RAW_STRING: return "NODE_RAW_STRING";
        case NODE_CHAR: return "NODE_CHAR";
        case NODE_BOOL: return "NODE_BOOL";
        case NODE_NULL: return "NODE_NULL";
        case NODE_CAST: return "NODE_CAST";
        case NODE_SIZEOF: return "NODE_SIZEOF";
        case NODE_VAR_REF: return "NODE_VAR_REF";
        case NODE_CALL_EXPR: return "NODE_CALL_EXPR";
        default: return "NODE_UNKNOWN";
    }
}

static const char* ast_dump_data_type_name(DataType type)
{
    switch (type) {
        case TYPE_INT: return "TYPE_INT";
        case TYPE_I8: return "TYPE_I8";
        case TYPE_I16: return "TYPE_I16";
        case TYPE_I32: return "TYPE_I32";
        case TYPE_U8: return "TYPE_U8";
        case TYPE_U16: return "TYPE_U16";
        case TYPE_U32: return "TYPE_U32";
        case TYPE_U64: return "TYPE_U64";
        case TYPE_STRING: return "TYPE_STRING";
        case TYPE_POINTER: return "TYPE_POINTER";
        case TYPE_FUNC_PTR: return "TYPE_FUNC_PTR";
        case TYPE_BOOL: return "TYPE_BOOL";
        case TYPE_CHAR: return "TYPE_CHAR";
        case TYPE_FLOAT: return "TYPE_FLOAT";
        case TYPE_VOID: return "TYPE_VOID";
        case TYPE_ENUM: return "TYPE_ENUM";
        case TYPE_STRUCT: return "TYPE_STRUCT";
        case TYPE_UNION: return "TYPE_UNION";
        default: return "TYPE_UNKNOWN";
    }
}

static const char* ast_dump_op_type_name(OpType op)
{
    switch (op) {
        case OP_ADD: return "OP_ADD";
        case OP_SUB: return "OP_SUB";
        case OP_MUL: return "OP_MUL";
        case OP_DIV: return "OP_DIV";
        case OP_MOD: return "OP_MOD";
        case OP_BIT_AND: return "OP_BIT_AND";
        case OP_BIT_OR: return "OP_BIT_OR";
        case OP_BIT_XOR: return "OP_BIT_XOR";
        case OP_SHL: return "OP_SHL";
        case OP_SHR: return "OP_SHR";
        case OP_EQ: return "OP_EQ";
        case OP_NEQ: return "OP_NEQ";
        case OP_LT: return "OP_LT";
        case OP_GT: return "OP_GT";
        case OP_LTE: return "OP_LTE";
        case OP_GTE: return "OP_GTE";
        case OP_AND: return "OP_AND";
        case OP_OR: return "OP_OR";
        default: return "OP_UNKNOWN";
    }
}

static const char* ast_dump_unary_op_name(UnaryOpType op)
{
    switch (op) {
        case UOP_NEG: return "UOP_NEG";
        case UOP_NOT: return "UOP_NOT";
        case UOP_BIT_NOT: return "UOP_BIT_NOT";
        case UOP_ADDR: return "UOP_ADDR";
        case UOP_DEREF: return "UOP_DEREF";
        case UOP_INC: return "UOP_INC";
        case UOP_DEC: return "UOP_DEC";
        default: return "UOP_UNKNOWN";
    }
}

static void ast_dump_indent(FILE* out, int depth)
{
    int spaces = depth * AST_DUMP_INDENT_WIDTH;
    for (int pos = 0; pos < spaces; pos++) {
        fputc(' ', out);
    }
}

static void ast_dump_escaped(FILE* out, const char* text)
{
    fputc('"', out);
    if (text) {
        const unsigned char* cursor = (const unsigned char*)text;
        while (*cursor) {
            unsigned char ch = *cursor++;
            switch (ch) {
                case '\\':
                    fputs("\\\\", out);
                    break;
                case '"':
                    fputs("\\\"", out);
                    break;
                case '\n':
                    fputs("\\n", out);
                    break;
                case '\r':
                    fputs("\\r", out);
                    break;
                case '\t':
                    fputs("\\t", out);
                    break;
                default:
                    if (ch < AST_DUMP_ASCII_CONTROL_LIMIT) {
                        fprintf(out, "\\x%02x", (unsigned int)ch);
                    } else {
                        fputc((int)ch, out);
                    }
                    break;
            }
        }
    }
    fputc('"', out);
}

static void ast_dump_func_sig(FILE* out, const FuncPtrSig* sig)
{
    if (!sig) {
        fputs(" func_sig=<null>", out);
        return;
    }

    fprintf(out,
            " func_sig=(return=%s return_ptr_base=%s return_ptr_depth=%d",
            ast_dump_data_type_name(sig->return_type),
            ast_dump_data_type_name(sig->return_ptr_base_type),
            sig->return_ptr_depth);
    if (sig->return_ptr_base_type_name) {
        fputs(" return_ptr_base_name=", out);
        ast_dump_escaped(out, sig->return_ptr_base_type_name);
    }

    fprintf(out, " params=%d variadic=%d", sig->param_count, sig->is_variadic ? 1 : 0);
    for (int param_index = 0; param_index < sig->param_count; param_index++) {
        DataType param_type = sig->param_types ? sig->param_types[param_index] : TYPE_INT;
        DataType param_base = sig->param_ptr_base_types ?
                              sig->param_ptr_base_types[param_index] : TYPE_INT;
        int param_depth = sig->param_ptr_depths ? sig->param_ptr_depths[param_index] : 0;
        fprintf(out,
                " param%d=%s/base:%s/depth:%d",
                param_index,
                ast_dump_data_type_name(param_type),
                ast_dump_data_type_name(param_base),
                param_depth);
        if (sig->param_ptr_base_type_names && sig->param_ptr_base_type_names[param_index]) {
            fputs("/name:", out);
            ast_dump_escaped(out, sig->param_ptr_base_type_names[param_index]);
        }
    }
    fputc(')', out);
}

static void ast_dump_type_ref(FILE* out,
                              const char* label,
                              DataType type,
                              const char* type_name,
                              DataType ptr_base_type,
                              const char* ptr_base_type_name,
                              int ptr_depth,
                              const FuncPtrSig* func_sig)
{
    fprintf(out, " %s=%s", label, ast_dump_data_type_name(type));
    if (type_name) {
        fprintf(out, " %s_name=", label);
        ast_dump_escaped(out, type_name);
    }
    if (type == TYPE_POINTER || ptr_depth > 0) {
        fprintf(out,
                " %s_ptr_base=%s %s_ptr_depth=%d",
                label,
                ast_dump_data_type_name(ptr_base_type),
                label,
                ptr_depth);
        if (ptr_base_type_name) {
            fprintf(out, " %s_ptr_base_name=", label);
            ast_dump_escaped(out, ptr_base_type_name);
        }
    }
    if (type == TYPE_FUNC_PTR || func_sig) {
        ast_dump_func_sig(out, func_sig);
    }
}

static void ast_dump_dims(FILE* out, const Node* node)
{
    fputs(" dims=[", out);
    if (node && node->data.array_decl.dims) {
        for (int dim_index = 0; dim_index < node->data.array_decl.dim_count; dim_index++) {
            if (dim_index > 0) fputc(',', out);
            fprintf(out, "%d", node->data.array_decl.dims[dim_index]);
        }
    }
    fputc(']', out);
}

static void ast_dump_node(FILE* out, const Node* node, int depth);

static void ast_dump_child(FILE* out, const char* label, const Node* child, int depth)
{
    ast_dump_indent(out, depth);
    fprintf(out, "%s:\n", label);
    if (child) {
        ast_dump_node(out, child, depth + 1);
    } else {
        ast_dump_indent(out, depth + 1);
        fputs("<empty>\n", out);
    }
}

static void ast_dump_list(FILE* out, const char* label, const Node* head, int depth)
{
    ast_dump_indent(out, depth);
    fprintf(out, "%s:\n", label);
    if (!head) {
        ast_dump_indent(out, depth + 1);
        fputs("<empty>\n", out);
        return;
    }

    const Node* current = head;
    while (current) {
        ast_dump_node(out, current, depth + 1);
        current = current->next;
    }
}

static void ast_dump_array_op_children(FILE* out, const Node* node, int depth)
{
    ast_dump_list(out, "indices", node->data.array_op.indices, depth + 1);
    if (node->data.array_op.value) {
        ast_dump_child(out, "value", node->data.array_op.value, depth + 1);
    }
}

static void ast_dump_node(FILE* out, const Node* node, int depth)
{
    if (!node) {
        ast_dump_indent(out, depth);
        fputs("<empty>\n", out);
        return;
    }

    ast_dump_indent(out, depth);
    fputs(ast_dump_node_type_name(node->type), out);

    switch (node->type) {
        case NODE_FUNC_DEF:
            fputs(" name=", out);
            ast_dump_escaped(out, node->data.func_def.name);
            ast_dump_type_ref(out,
                              "return",
                              node->data.func_def.return_type,
                              NULL,
                              node->data.func_def.return_ptr_base_type,
                              node->data.func_def.return_ptr_base_type_name,
                              node->data.func_def.return_ptr_depth,
                              node->data.func_def.return_func_sig);
            fprintf(out,
                    " variadic=%d prototype=%d",
                    node->data.func_def.is_variadic ? 1 : 0,
                    node->data.func_def.is_prototype ? 1 : 0);
            break;
        case NODE_VAR_DECL:
            fputs(" name=", out);
            ast_dump_escaped(out, node->data.var_decl.name);
            ast_dump_type_ref(out,
                              "type",
                              node->data.var_decl.type,
                              node->data.var_decl.type_name,
                              node->data.var_decl.ptr_base_type,
                              node->data.var_decl.ptr_base_type_name,
                              node->data.var_decl.ptr_depth,
                              node->data.var_decl.func_sig);
            fprintf(out,
                    " global=%d const=%d static=%d",
                    node->data.var_decl.is_global ? 1 : 0,
                    node->data.var_decl.is_const ? 1 : 0,
                    node->data.var_decl.is_static ? 1 : 0);
            break;
        case NODE_TYPE_ALIAS:
            fputs(" name=", out);
            ast_dump_escaped(out, node->data.type_alias.name);
            ast_dump_type_ref(out,
                              "target",
                              node->data.type_alias.target_type,
                              node->data.type_alias.target_type_name,
                              node->data.type_alias.target_ptr_base_type,
                              node->data.type_alias.target_ptr_base_type_name,
                              node->data.type_alias.target_ptr_depth,
                              node->data.type_alias.target_func_sig);
            break;
        case NODE_ENUM_DECL:
            fputs(" name=", out);
            ast_dump_escaped(out, node->data.enum_decl.name);
            break;
        case NODE_STRUCT_DECL:
            fputs(" name=", out);
            ast_dump_escaped(out, node->data.struct_decl.name);
            break;
        case NODE_UNION_DECL:
            fputs(" name=", out);
            ast_dump_escaped(out, node->data.union_decl.name);
            break;
        case NODE_ENUM_MEMBER:
            fputs(" name=", out);
            ast_dump_escaped(out, node->data.enum_member.name);
            fprintf(out,
                    " has_value=%d value=%" PRId64,
                    node->data.enum_member.has_value ? 1 : 0,
                    node->data.enum_member.value);
            break;
        case NODE_MEMBER_ACCESS:
            fputs(" member=", out);
            ast_dump_escaped(out, node->data.member_access.member);
            fprintf(out, " via_pointer=%d", node->data.member_access.via_pointer ? 1 : 0);
            break;
        case NODE_ARRAY_DECL:
            fputs(" name=", out);
            ast_dump_escaped(out, node->data.array_decl.name);
            ast_dump_type_ref(out,
                              "element",
                              node->data.array_decl.element_type,
                              node->data.array_decl.element_type_name,
                              node->data.array_decl.element_ptr_base_type,
                              node->data.array_decl.element_ptr_base_type_name,
                              node->data.array_decl.element_ptr_depth,
                              NULL);
            fprintf(out,
                    " dim_count=%d total=%" PRId64 " global=%d const=%d static=%d has_init=%d init_count=%d",
                    node->data.array_decl.dim_count,
                    node->data.array_decl.total_elems,
                    node->data.array_decl.is_global ? 1 : 0,
                    node->data.array_decl.is_const ? 1 : 0,
                    node->data.array_decl.is_static ? 1 : 0,
                    node->data.array_decl.has_init ? 1 : 0,
                    node->data.array_decl.init_count);
            ast_dump_dims(out, node);
            break;
        case NODE_ARRAY_ASSIGN:
        case NODE_ARRAY_ACCESS:
            fputs(" name=", out);
            ast_dump_escaped(out, node->data.array_op.name);
            fprintf(out, " index_count=%d", node->data.array_op.index_count);
            break;
        case NODE_CALL_STMT:
        case NODE_CALL_EXPR:
            fputs(" name=", out);
            ast_dump_escaped(out, node->data.call.name);
            break;
        case NODE_ASM_OPERAND:
            fputs(" constraint=", out);
            ast_dump_escaped(out, node->data.asm_operand.constraint);
            fprintf(out, " output=%d", node->data.asm_operand.is_output ? 1 : 0);
            break;
        case NODE_CASE:
            fprintf(out, " default=%d", node->data.case_stmt.is_default ? 1 : 0);
            break;
        case NODE_READ:
            fputs(" var=", out);
            ast_dump_escaped(out, node->data.read_stmt.var_name);
            break;
        case NODE_ASSIGN:
            fputs(" name=", out);
            ast_dump_escaped(out, node->data.assign_stmt.name);
            break;
        case NODE_BIN_OP:
            fprintf(out, " op=%s", ast_dump_op_type_name(node->data.bin_op.op));
            break;
        case NODE_UNARY_OP:
        case NODE_POSTFIX_OP:
            fprintf(out, " op=%s", ast_dump_unary_op_name(node->data.unary_op.op));
            break;
        case NODE_INT:
            fprintf(out, " value=%" PRId64, node->data.integer.value);
            break;
        case NODE_FLOAT:
            fprintf(out,
                    " bits=0x%016" PRIx64,
                    (uint64_t)node->data.float_lit.bits);
            break;
        case NODE_STRING:
        case NODE_RAW_STRING:
            fputs(" value=", out);
            ast_dump_escaped(out, node->data.string_lit.value);
            break;
        case NODE_CHAR:
            fprintf(out, " value=%d", node->data.char_lit.value);
            break;
        case NODE_BOOL:
            fprintf(out, " value=%d", node->data.bool_lit.value ? 1 : 0);
            break;
        case NODE_SIZEOF:
            fprintf(out, " type_form=%d", node->data.sizeof_expr.has_type_form ? 1 : 0);
            if (node->data.sizeof_expr.has_type_form) {
                ast_dump_type_ref(out,
                                  "target",
                                  node->data.sizeof_expr.target_type,
                                  node->data.sizeof_expr.target_type_name,
                                  TYPE_INT,
                                  NULL,
                                  0,
                                  NULL);
            }
            break;
        case NODE_CAST:
            ast_dump_type_ref(out,
                              "target",
                              node->data.cast_expr.target_type,
                              node->data.cast_expr.target_type_name,
                              node->data.cast_expr.target_ptr_base_type,
                              node->data.cast_expr.target_ptr_base_type_name,
                              node->data.cast_expr.target_ptr_depth,
                              NULL);
            break;
        case NODE_VAR_REF:
            fputs(" name=", out);
            ast_dump_escaped(out, node->data.var_ref.name);
            break;
        default:
            break;
    }

    fputc('\n', out);

    switch (node->type) {
        case NODE_PROGRAM:
            ast_dump_list(out, "declarations", node->data.program.declarations, depth + 1);
            break;
        case NODE_FUNC_DEF:
            ast_dump_list(out, "params", node->data.func_def.params, depth + 1);
            ast_dump_child(out, "body", node->data.func_def.body, depth + 1);
            break;
        case NODE_VAR_DECL:
            ast_dump_child(out, "expression", node->data.var_decl.expression, depth + 1);
            break;
        case NODE_ENUM_DECL:
            ast_dump_list(out, "members", node->data.enum_decl.members, depth + 1);
            break;
        case NODE_STRUCT_DECL:
            ast_dump_list(out, "fields", node->data.struct_decl.fields, depth + 1);
            break;
        case NODE_UNION_DECL:
            ast_dump_list(out, "fields", node->data.union_decl.fields, depth + 1);
            break;
        case NODE_BLOCK:
            ast_dump_list(out, "statements", node->data.block.statements, depth + 1);
            break;
        case NODE_MEMBER_ACCESS:
            ast_dump_child(out, "base", node->data.member_access.base, depth + 1);
            break;
        case NODE_MEMBER_ASSIGN:
            ast_dump_child(out, "target", node->data.member_assign.target, depth + 1);
            ast_dump_child(out, "value", node->data.member_assign.value, depth + 1);
            break;
        case NODE_DEREF_ASSIGN:
            ast_dump_child(out, "target", node->data.deref_assign.target, depth + 1);
            ast_dump_child(out, "value", node->data.deref_assign.value, depth + 1);
            break;
        case NODE_ARRAY_DECL:
            ast_dump_list(out, "init_values", node->data.array_decl.init_values, depth + 1);
            break;
        case NODE_ARRAY_ASSIGN:
        case NODE_ARRAY_ACCESS:
            ast_dump_array_op_children(out, node, depth);
            break;
        case NODE_CALL_STMT:
        case NODE_CALL_EXPR:
            ast_dump_list(out, "args", node->data.call.args, depth + 1);
            break;
        case NODE_INLINE_ASM:
            ast_dump_list(out, "templates", node->data.inline_asm.templates, depth + 1);
            ast_dump_list(out, "outputs", node->data.inline_asm.outputs, depth + 1);
            ast_dump_list(out, "inputs", node->data.inline_asm.inputs, depth + 1);
            break;
        case NODE_ASM_OPERAND:
            ast_dump_child(out, "expression", node->data.asm_operand.expression, depth + 1);
            break;
        case NODE_IF:
            ast_dump_child(out, "condition", node->data.if_stmt.condition, depth + 1);
            ast_dump_child(out, "then", node->data.if_stmt.then_branch, depth + 1);
            ast_dump_child(out, "else", node->data.if_stmt.else_branch, depth + 1);
            break;
        case NODE_WHILE:
            ast_dump_child(out, "condition", node->data.while_stmt.condition, depth + 1);
            ast_dump_child(out, "body", node->data.while_stmt.body, depth + 1);
            break;
        case NODE_FOR:
            ast_dump_child(out, "init", node->data.for_stmt.init, depth + 1);
            ast_dump_child(out, "condition", node->data.for_stmt.condition, depth + 1);
            ast_dump_child(out, "increment", node->data.for_stmt.increment, depth + 1);
            ast_dump_child(out, "body", node->data.for_stmt.body, depth + 1);
            break;
        case NODE_SWITCH:
            ast_dump_child(out, "expression", node->data.switch_stmt.expression, depth + 1);
            ast_dump_list(out, "cases", node->data.switch_stmt.cases, depth + 1);
            break;
        case NODE_CASE:
            ast_dump_child(out, "value", node->data.case_stmt.value, depth + 1);
            ast_dump_list(out, "body", node->data.case_stmt.body, depth + 1);
            break;
        case NODE_RETURN:
            ast_dump_child(out, "expression", node->data.return_stmt.expression, depth + 1);
            break;
        case NODE_PRINT:
            ast_dump_child(out, "expression", node->data.print_stmt.expression, depth + 1);
            break;
        case NODE_ASSIGN:
            ast_dump_child(out, "expression", node->data.assign_stmt.expression, depth + 1);
            break;
        case NODE_BIN_OP:
            ast_dump_child(out, "left", node->data.bin_op.left, depth + 1);
            ast_dump_child(out, "right", node->data.bin_op.right, depth + 1);
            break;
        case NODE_UNARY_OP:
        case NODE_POSTFIX_OP:
            ast_dump_child(out, "operand", node->data.unary_op.operand, depth + 1);
            break;
        case NODE_SIZEOF:
            if (!node->data.sizeof_expr.has_type_form) {
                ast_dump_child(out, "expression", node->data.sizeof_expr.expression, depth + 1);
            }
            break;
        case NODE_CAST:
            ast_dump_child(out, "expression", node->data.cast_expr.expression, depth + 1);
            break;
        default:
            break;
    }
}

void ast_dump_print(const Node* root, FILE* out)
{
    FILE* target = out ? out : stdout;
    ast_dump_node(target, root, 0);
}
