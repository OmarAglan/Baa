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

    query->completion_inside_function = active_function != NULL;
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

typedef struct
{
    bool valid;
    DataType type;
    const char* type_name;
    DataType pointer_base;
    const char* pointer_base_name;
    int pointer_depth;
    const FuncPtrSig* function_signature;
} SemanticCompletionType;

static SemanticCompletionType semantic_completion_parameter_type(
    const SemanticQuery* query)
{
    SemanticCompletionType result = {0};
    if (!query || !query->signature_decl || query->active_parameter < 0)
        return result;

    const Node* declaration = query->signature_decl;
    if (declaration->type == NODE_FUNC_DEF)
    {
        int index = 0;
        for (const Node* parameter = declaration->data.func_def.params;
             parameter; parameter = parameter->next)
        {
            if (parameter->type != NODE_VAR_DECL) continue;
            if (index++ != query->active_parameter) continue;
            result.valid = true;
            result.type = parameter->data.var_decl.type;
            result.type_name = parameter->data.var_decl.type_name;
            result.pointer_base = parameter->data.var_decl.ptr_base_type;
            result.pointer_base_name =
                parameter->data.var_decl.ptr_base_type_name;
            result.pointer_depth = parameter->data.var_decl.ptr_depth;
            result.function_signature = parameter->data.var_decl.func_sig;
            return result;
        }
        return result;
    }

    if (declaration->type == NODE_VAR_DECL &&
        declaration->data.var_decl.type == TYPE_FUNC_PTR)
    {
        const FuncPtrSig* signature = declaration->data.var_decl.func_sig;
        const int index = query->active_parameter;
        if (!signature || index >= signature->param_count) return result;
        result.valid = true;
        result.type = signature->param_types
            ? signature->param_types[index] : TYPE_INT;
        result.pointer_base = signature->param_ptr_base_types
            ? signature->param_ptr_base_types[index] : TYPE_INT;
        result.pointer_base_name = signature->param_ptr_base_type_names
            ? signature->param_ptr_base_type_names[index] : NULL;
        result.pointer_depth = signature->param_ptr_depths
            ? signature->param_ptr_depths[index] : 0;
    }
    return result;
}

static SemanticCompletionType semantic_completion_candidate_type(
    const Node* declaration)
{
    SemanticCompletionType result = {0};
    if (!declaration) return result;
    switch (declaration->type)
    {
        case NODE_VAR_DECL:
            result.valid = true;
            result.type = declaration->data.var_decl.type;
            result.type_name = declaration->data.var_decl.type_name;
            result.pointer_base = declaration->data.var_decl.ptr_base_type;
            result.pointer_base_name =
                declaration->data.var_decl.ptr_base_type_name;
            result.pointer_depth = declaration->data.var_decl.ptr_depth;
            result.function_signature = declaration->data.var_decl.func_sig;
            break;
        case NODE_FUNC_DEF:
            result.valid = true;
            result.type = declaration->data.func_def.return_type;
            result.pointer_base =
                declaration->data.func_def.return_ptr_base_type;
            result.pointer_base_name =
                declaration->data.func_def.return_ptr_base_type_name;
            result.pointer_depth = declaration->data.func_def.return_ptr_depth;
            result.function_signature =
                declaration->data.func_def.return_func_sig;
            break;
        default:
            break;
    }
    return result;
}

static bool semantic_completion_same_name(const char* left,
                                          const char* right)
{
    if (!left || !right) return left == right;
    return strcmp(left, right) == 0;
}

static bool semantic_completion_same_type(SemanticCompletionType left,
                                          SemanticCompletionType right)
{
    if (!left.valid || !right.valid || left.type != right.type) return false;
    if (left.type == TYPE_POINTER)
        return left.pointer_base == right.pointer_base &&
               left.pointer_depth == right.pointer_depth &&
               semantic_completion_same_name(
                   left.pointer_base_name, right.pointer_base_name);
    if (left.type == TYPE_ENUM || left.type == TYPE_STRUCT ||
        left.type == TYPE_UNION)
        return semantic_completion_same_name(left.type_name, right.type_name);
    if (left.type == TYPE_FUNC_PTR)
        return left.function_signature == right.function_signature;
    return true;
}

static int semantic_completion_relevance(
    const SemanticCompletionCandidate* candidate,
    SemanticCompletionType expected)
{
    int scope_rank = 40 - candidate->priority;
    if (scope_rank < 0) scope_rank = 0;
    if (!expected.valid) return scope_rank;

    const SemanticCompletionType actual =
        semantic_completion_candidate_type(candidate->declaration);
    if (semantic_completion_same_type(actual, expected)) return scope_rank;
    if (candidate->declaration &&
        (candidate->declaration->type == NODE_VAR_DECL ||
         candidate->declaration->type == NODE_ARRAY_DECL ||
         candidate->declaration->type == NODE_FUNC_DEF))
        return 40 + scope_rank;
    return 80 + scope_rank;
}

static void semantic_print_completion_context(
    FILE* out,
    const SemanticQuery* query,
    SemanticCompletionType expected)
{
    const char* kind = query->signature_decl
        ? "call-argument"
        : (query->completion_inside_function ? "statement" : "top-level");
    fputs("{\"kind\":", out);
    semantic_json_escape(out, kind);
    fputs(",\"ranking\":[", out);
    const char* const* ranking = NULL;
    size_t ranking_count = 0u;
    static const char* const call_ranking[] = {
        "variable", "parameter", "array", "value", "function",
        "keyword", "snippet", "type"
    };
    static const char* const statement_ranking[] = {
        "variable", "parameter", "array", "function", "keyword",
        "value", "snippet", "type"
    };
    static const char* const top_level_ranking[] = {
        "snippet", "type", "keyword", "function", "variable",
        "array", "value"
    };
    if (query->signature_decl)
    {
        ranking = call_ranking;
        ranking_count = sizeof(call_ranking) / sizeof(call_ranking[0]);
    }
    else if (query->completion_inside_function)
    {
        ranking = statement_ranking;
        ranking_count =
            sizeof(statement_ranking) / sizeof(statement_ranking[0]);
    }
    else
    {
        ranking = top_level_ranking;
        ranking_count =
            sizeof(top_level_ranking) / sizeof(top_level_ranking[0]);
    }
    for (size_t i = 0; i < ranking_count; ++i)
    {
        if (i > 0) fputc(',', out);
        semantic_json_escape(out, ranking[i]);
    }
    fputc(']', out);
    if (query->signature_decl)
    {
        fputs(",\"call_name\":", out);
        semantic_json_escape(out, semantic_node_name(query->signature_call));
        fprintf(out, ",\"active_parameter\":%d", query->active_parameter);
    }
    if (expected.valid)
    {
        char display[1024] = {0};
        size_t used = 0u;
        semantic_render_type(display, sizeof(display), &used,
                             expected.type, expected.type_name,
                             expected.pointer_base,
                             expected.pointer_base_name,
                             expected.pointer_depth,
                             expected.function_signature);
        fputs(",\"expected_type\":", out);
        semantic_json_escape(out, display);
    }
    fputc('}', out);
}

static void semantic_print_completion(FILE* out,
                                      const SemanticQuery* query)
{
    const SemanticCompletionType expected =
        semantic_completion_parameter_type(query);
    fputs("{\"context\":", out);
    semantic_print_completion_context(out, query, expected);
    fputs(",\"items\":[", out);
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
        semantic_json_escape(out, semantic_kind(query, declaration));
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
        fprintf(out, ",\"relevance\":%d",
                semantic_completion_relevance(candidate, expected));
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
