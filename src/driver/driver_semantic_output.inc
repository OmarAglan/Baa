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
                                           const SemanticQuery* query,
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
    semantic_json_escape(out, semantic_kind(query, declaration));
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
    semantic_json_escape(out, semantic_kind(query,
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
    semantic_print_symbol_identity(query->index_out, query, declaration);
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
    semantic_json_escape(out, semantic_kind(query, declaration));
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
