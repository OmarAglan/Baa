Node* parse_case() {
    Token tok_case = parser.current;
    Node* node = ast_node_new(NODE_CASE, tok_case);
    if (!node) return NULL;

    if (parser.current.type == TOKEN_DEFAULT) {
        eat(TOKEN_DEFAULT);
        eat(TOKEN_COLON);
        node->data.case_stmt.is_default = true;
        node->data.case_stmt.value = NULL;
    } else {
        eat(TOKEN_CASE);
        // قيمة case يجب أن تكون ثابتاً بسيطاً (رقم/حرف) ولا يجب أن تستهلك ':' كعامل وصول للأعضاء.
        if (parser.current.type == TOKEN_INT) {
            Token tok = parser.current;
            Node* v = ast_node_new(NODE_INT, tok);
            if (!v) return NULL;
            int64_t n = 0;
            (void)parse_int_token_checked(parser.current, &n);
            v->data.integer.value = n;
            eat(TOKEN_INT);
            node->data.case_stmt.value = v;
        }
        else if (parser.current.type == TOKEN_CHAR) {
            Token tok = parser.current;
            Node* v = ast_node_new(NODE_CHAR, tok);
            if (!v) return NULL;
            uint32_t cp = 0;
            if (!utf8_decode_one(parser.current.value, &cp)) {
                error_report(parser.current, "حرف UTF-8 غير صالح.");
                cp = (uint32_t)'?';
            }
            v->data.char_lit.value = (int)cp;
            eat(TOKEN_CHAR);
            node->data.case_stmt.value = v;
        }
        else {
            error_report(parser.current, "متوقع ثابت عددي أو حرفي بعد 'حالة'.");
            node->data.case_stmt.value = NULL;
        }
        eat(TOKEN_COLON);
        node->data.case_stmt.is_default = false;
    }

    Node* head_stmt = NULL;
    Node* tail_stmt = NULL;

    while (parser.current.type != TOKEN_CASE && 
           parser.current.type != TOKEN_DEFAULT && 
           parser.current.type != TOKEN_RBRACE && 
           parser.current.type != TOKEN_EOF) {
        
        Node* stmt = parse_statement();
        if (stmt) {
            if (head_stmt == NULL) { head_stmt = stmt; tail_stmt = stmt; }
            else { tail_stmt->next = stmt; tail_stmt = stmt; }
        } else {
            synchronize_mode(PARSER_SYNC_SWITCH); // تعافٍ داخل جملة اختر
        }
    }

    node->data.case_stmt.body = head_stmt;
    return node;
}

Node* parse_switch() {
    Token tok_switch = parser.current;
    eat(TOKEN_SWITCH);
    eat(TOKEN_LPAREN);
    Node* expr = parse_expression();
    eat(TOKEN_RPAREN);
    eat(TOKEN_LBRACE);

    Node* head_case = NULL;
    Node* tail_case = NULL;

    while (parser.current.type != TOKEN_RBRACE && parser.current.type != TOKEN_EOF) {
        if (parser.current.type == TOKEN_CASE || parser.current.type == TOKEN_DEFAULT) {
            Node* case_node = parse_case();
            if (head_case == NULL) { head_case = case_node; tail_case = case_node; }
            else { tail_case->next = case_node; tail_case = case_node; }
        } else {
            error_report(parser.current, "متوقع 'حالة' أو 'افتراضي' داخل 'اختر'.");
            synchronize_mode(PARSER_SYNC_SWITCH);
        }
    }
    eat(TOKEN_RBRACE);

    Node* node = ast_node_new(NODE_SWITCH, tok_switch);
    if (!node) return NULL;
    node->data.switch_stmt.expression = expr;
    node->data.switch_stmt.cases = head_case;
    return node;
}

Node* parse_block() {
    Token tok_lbrace = parser.current;
    eat(TOKEN_LBRACE);
    Node* head = NULL;
    Node* tail = NULL;
    while (parser.current.type != TOKEN_RBRACE && parser.current.type != TOKEN_EOF) {
        Node* stmt = parse_statement();
        if (stmt) {
            if (head == NULL) { head = stmt; tail = stmt; } 
            else { tail->next = stmt; tail = stmt; }
        } else {
            synchronize_mode(PARSER_SYNC_STATEMENT);
        }
    }
    eat(TOKEN_RBRACE);
    Node* block = ast_node_new(NODE_BLOCK, tok_lbrace);
    if (!block) return NULL;
    block->data.block.statements = head;
    return block;
}

// ============================================================================
// تهيئة المصفوفات (Array Initializer Lists)
// ============================================================================

/**
 * @brief تحليل عنصر واحد داخل تهيئة المصفوفة (قيمة أو كتلة متداخلة).
 */
static void parse_array_initializer_item(Node** head, Node** tail, int* count);

/**
 * @brief تحليل كتلة تهيئة مصفوفة بالشكل: { ... } مع دعم التعشيش.
 */
static void parse_array_initializer_group(Node** head, Node** tail, int* count)
{
    eat(TOKEN_LBRACE);

    if (parser.current.type != TOKEN_RBRACE) {
        while (1) {
            parse_array_initializer_item(head, tail, count);

            if (parser.current.type == TOKEN_COMMA) {
                eat(TOKEN_COMMA);
                // السماح بفاصلة أخيرة قبل '}' كما في C.
                if (parser.current.type == TOKEN_RBRACE) break;
                continue;
            }
            break;
        }
    }

    eat(TOKEN_RBRACE);
}

/**
 * @brief تحليل عنصر واحد داخل تهيئة المصفوفة (قيمة أو كتلة متداخلة).
 */
static void parse_array_initializer_item(Node** head, Node** tail, int* count)
{
    if (parser.current.type == TOKEN_LBRACE) {
        parse_array_initializer_group(head, tail, count);
        return;
    }

    Node* expr = parse_expression();
    if (!expr) return;

    parser_list_append(head, tail, expr);
    if (count) (*count)++;
}

/**
 * @brief تحليل قائمة تهيئة مصفوفة بالشكل: = { expr, { ... }, ... }
 *
 * عند وجود '=' نعتبر أن التهيئة موجودة حتى لو كانت القائمة فارغة `{}`.
 *
 * @param out_count عدد عناصر التهيئة الفعلية
 * @param out_has_init هل تم العثور على '=' (تهيئة صريحة)
 * @return رأس قائمة العقد (قد تكون NULL عند `{}` أو عند عدم وجود تهيئة)
 */
static Node* parse_array_initializer_list(int* out_count, bool* out_has_init) {
    if (out_count) *out_count = 0;
    if (out_has_init) *out_has_init = false;

    if (parser.current.type != TOKEN_ASSIGN) {
        return NULL;
    }

    if (out_has_init) *out_has_init = true;
    eat(TOKEN_ASSIGN);

    Node* head = NULL;
    Node* tail = NULL;
    int count = 0;

    if (parser.current.type != TOKEN_LBRACE) {
        error_report(parser.current, "متوقع '{' بعد '=' في تهيئة المصفوفة.");
        return NULL;
    }

    parse_array_initializer_group(&head, &tail, &count);

    if (out_count) *out_count = count;
    return head;
}

/**
 * @brief تحليل تعريف اسم نوع بديل على المستوى العام.
 *
 * الصيغة:
 * نوع <اسم> = <نوع_موجود>.
 */
static Node* parse_type_alias_declaration(bool register_alias)
{
    if (!parser_current_is_type_alias_keyword()) {
        error_report(parser.current, "متوقع 'نوع' في بداية تعريف الاسم البديل.");
        synchronize_mode(PARSER_SYNC_DECLARATION);
        return NULL;
    }

    Token tok_kw = parser.current;
    eat(parser.current.type);

    if (parser.current.type != TOKEN_IDENTIFIER) {
        error_report(parser.current, "متوقع اسم بعد 'نوع'.");
        synchronize_mode(PARSER_SYNC_DECLARATION);
        return NULL;
    }

    Token tok_name = parser.current;
    char* alias_name = strdup(parser.current.value);
    eat(TOKEN_IDENTIFIER);

    if (!alias_name) {
        error_report(tok_name, "نفدت الذاكرة أثناء نسخ اسم النوع البديل.");
        synchronize_mode(PARSER_SYNC_DECLARATION);
        return NULL;
    }

    eat(TOKEN_ASSIGN);

    DataType target_type = TYPE_INT;
    char* target_type_name = NULL;
    DataType target_ptr_base_type = TYPE_INT;
    char* target_ptr_base_type_name = NULL;
    int target_ptr_depth = 0;
    FuncPtrSig* target_func_sig = NULL;
    if (!parse_type_spec(&target_type, &target_type_name,
                         &target_ptr_base_type, &target_ptr_base_type_name,
                         &target_ptr_depth,
                         &target_func_sig)) {
        error_report(parser.current, "متوقع نوع معروف بعد '=' في تعريف الاسم البديل.");
        free(alias_name);
        free(target_type_name);
        free(target_ptr_base_type_name);
        parser_funcsig_free(target_func_sig);
        synchronize_mode(PARSER_SYNC_DECLARATION);
        return NULL;
    }

    eat(TOKEN_DOT);

    Node* alias = ast_node_new(NODE_TYPE_ALIAS, tok_name);
    if (!alias) {
        free(alias_name);
        free(target_type_name);
        free(target_ptr_base_type_name);
        return NULL;
    }

    alias->data.type_alias.name = alias_name;
    alias->data.type_alias.target_type = target_type;
    alias->data.type_alias.target_type_name = target_type_name;
    alias->data.type_alias.target_ptr_base_type = target_ptr_base_type;
    alias->data.type_alias.target_ptr_base_type_name = target_ptr_base_type_name;
    alias->data.type_alias.target_ptr_depth = target_ptr_depth;
    alias->data.type_alias.target_func_sig = target_func_sig;

    if (register_alias) {
        (void)parser_type_alias_register(tok_kw, alias_name,
                                         target_type, target_type_name,
                                         target_ptr_base_type, target_ptr_base_type_name,
                                         target_ptr_depth,
                                         target_func_sig);
    }
    return alias;
}

static Node* parse_inline_asm_operand(bool is_output)
{
    if (parser.current.type != TOKEN_STRING) {
        error_report(parser.current, "متوقع قيداً نصياً في جملة 'مجمع'.");
        return NULL;
    }

    Token tok_constraint = parser.current;
    char* constraint = parser.current.value ? strdup(parser.current.value) : strdup("");
    eat(TOKEN_STRING);

    eat(TOKEN_LPAREN);
    Node* expr = parse_expression();
    eat(TOKEN_RPAREN);

    if (!constraint) {
        error_report(tok_constraint, "نفدت الذاكرة أثناء نسخ قيد جملة 'مجمع'.");
        return NULL;
    }

    Node* operand = ast_node_new(NODE_ASM_OPERAND, tok_constraint);
    if (!operand) {
        free(constraint);
        return NULL;
    }
    operand->data.asm_operand.constraint = constraint;
    operand->data.asm_operand.expression = expr;
    operand->data.asm_operand.is_output = is_output;
    return operand;
}

static Node* parse_inline_asm_operand_list(bool is_output)
{
    Node* head = NULL;
    Node* tail = NULL;

    while (parser.current.type != TOKEN_COLON &&
           parser.current.type != TOKEN_RBRACE &&
           parser.current.type != TOKEN_EOF) {
        if (parser.current.type != TOKEN_STRING) {
            error_report(parser.current, "متوقع قيداً نصياً في معاملات 'مجمع'.");
            break;
        }

        Node* operand = parse_inline_asm_operand(is_output);
        if (!operand) break;
        parser_list_append(&head, &tail, operand);

        if (parser.current.type == TOKEN_COMMA) {
            eat(TOKEN_COMMA);
            continue;
        }
        break;
    }

    return head;
}

static Node* parse_inline_asm_statement(void)
{
    Token tok_asm = parser.current;
    eat(TOKEN_ASM);
    eat(TOKEN_LBRACE);

    Node* templates = NULL;
    Node* templates_tail = NULL;
    while (parser.current.type == TOKEN_STRING) {
        Token tok_tpl = parser.current;
        Node* tpl = ast_node_new(NODE_STRING, tok_tpl);
        if (!tpl) return NULL;
        tpl->data.string_lit.value = parser.current.value ? strdup(parser.current.value) : strdup("");
        tpl->data.string_lit.id = -1;
        eat(TOKEN_STRING);

        if (!tpl->data.string_lit.value) {
            error_report(tok_tpl, "نفدت الذاكرة أثناء نسخ سطر جملة 'مجمع'.");
            return NULL;
        }

        parser_list_append(&templates, &templates_tail, tpl);

        if (parser.current.type == TOKEN_COMMA) {
            eat(TOKEN_COMMA);
        }
    }

    if (!templates) {
        error_report(parser.current, "جملة 'مجمع' تتطلب سطر تجميع نصياً واحداً على الأقل.");
    }

    Node* outputs = NULL;
    Node* inputs = NULL;

    if (parser.current.type == TOKEN_COLON) {
        eat(TOKEN_COLON);
        outputs = parse_inline_asm_operand_list(true);

        if (parser.current.type == TOKEN_COLON) {
            eat(TOKEN_COLON);
            inputs = parse_inline_asm_operand_list(false);
        }
    }

    eat(TOKEN_RBRACE);

    if (parser.current.type == TOKEN_DOT) {
        eat(TOKEN_DOT);
    }

    Node* stmt = ast_node_new(NODE_INLINE_ASM, tok_asm);
    if (!stmt) return NULL;
    stmt->data.inline_asm.templates = templates;
    stmt->data.inline_asm.outputs = outputs;
    stmt->data.inline_asm.inputs = inputs;
    return stmt;
}

Node* parse_statement() {
    if (parser.current.type == TOKEN_LBRACE) return parse_block();

    if (parser_current_is_type_alias_keyword() && parser.next.type == TOKEN_IDENTIFIER) {
        error_report(parser.current, "تعريف 'نوع' مسموح فقط على المستوى العام.");
        return parse_type_alias_declaration(false);
    }

    if (parser.current.type == TOKEN_SWITCH) {
        return parse_switch();
    }

    if (parser.current.type == TOKEN_ASM) {
        return parse_inline_asm_statement();
    }

    if (parser.current.type == TOKEN_RETURN) {
        Token tok = parser.current;
        eat(TOKEN_RETURN);
        Node* stmt = ast_node_new(NODE_RETURN, tok);
        if (!stmt) return NULL;
        if (parser.current.type == TOKEN_DOT) {
            stmt->data.return_stmt.expression = NULL;
        } else {
            stmt->data.return_stmt.expression = parse_expression();
        }
        eat(TOKEN_DOT);
        return stmt;
    }

    if (parser.current.type == TOKEN_BREAK) {
        Token tok = parser.current;
        eat(TOKEN_BREAK);
        eat(TOKEN_DOT);
        Node* stmt = ast_node_new(NODE_BREAK, tok);
        return stmt;
    }

    if (parser.current.type == TOKEN_CONTINUE) {
        Token tok = parser.current;
        eat(TOKEN_CONTINUE);
        eat(TOKEN_DOT);
        Node* stmt = ast_node_new(NODE_CONTINUE, tok);
        return stmt;
    }

    if (parser.current.type == TOKEN_PRINT) {
        Token tok = parser.current;
        eat(TOKEN_PRINT);
        Node* stmt = ast_node_new(NODE_PRINT, tok);
        if (!stmt) return NULL;
        stmt->data.print_stmt.expression = parse_expression();
        eat(TOKEN_DOT);
        return stmt;
    }

    if (parser.current.type == TOKEN_READ) {
        Token tok = parser.current;
        eat(TOKEN_READ);

        if (parser.current.type != TOKEN_IDENTIFIER) {
            error_report(parser.current, "متوقع اسم متغير بعد 'اقرأ'.");
            return NULL;
        }

        Node* stmt = ast_node_new(NODE_READ, tok);
        if (!stmt) return NULL;

        stmt->data.read_stmt.var_name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);
        eat(TOKEN_DOT);
        return stmt;
    }

    if (parser.current.type == TOKEN_IF) {
        Token tok = parser.current;
        eat(TOKEN_IF);
        eat(TOKEN_LPAREN);
        Node* cond = parse_expression();
        eat(TOKEN_RPAREN);
        Node* then_branch = parse_statement();

        Node* else_branch = NULL;
        if (parser.current.type == TOKEN_ELSE) {
            eat(TOKEN_ELSE);
            else_branch = parse_statement();
        }

        Node* stmt = ast_node_new(NODE_IF, tok);
        if (!stmt) return NULL;
        stmt->data.if_stmt.condition = cond;
        stmt->data.if_stmt.then_branch = then_branch;
        stmt->data.if_stmt.else_branch = else_branch;
        return stmt;
    }

    if (parser.current.type == TOKEN_WHILE) {
        Token tok = parser.current;
        eat(TOKEN_WHILE);
        eat(TOKEN_LPAREN);
        Node* cond = parse_expression();
        eat(TOKEN_RPAREN);
        Node* body = parse_statement();

        Node* stmt = ast_node_new(NODE_WHILE, tok);
        if (!stmt) return NULL;
        stmt->data.while_stmt.condition = cond;
        stmt->data.while_stmt.body = body;
        return stmt;
    }

    if (parser.current.type == TOKEN_FOR) {
        Token tok_for = parser.current;
        eat(TOKEN_FOR);
        eat(TOKEN_LPAREN);

        Node* init = NULL;
        if (parser_is_decl_qualifier(parser.current.type) || parser_current_starts_type()) {
            ParserDeclQualifiers init_q;
            parser_parse_decl_qualifiers(&init_q);

            if (!parser_current_starts_type()) {
                error_report(parser.current, "متوقع نوع في تهيئة حلقة 'لكل'.");
                synchronize_mode(PARSER_SYNC_STATEMENT);
                return NULL;
            }

            Token tok_type = parser.current;
            DataType dt = TYPE_INT;
            char* type_name = NULL;
            DataType ptr_base_type = TYPE_INT;
            char* ptr_base_type_name = NULL;
            int ptr_depth = 0;
            FuncPtrSig* func_sig = NULL;
            (void)tok_type;

            if (!parse_type_spec(&dt, &type_name,
                                 &ptr_base_type, &ptr_base_type_name,
                                 &ptr_depth,
                                 &func_sig)) {
                error_report(parser.current, "متوقع نوع في تهيئة حلقة 'لكل'.");
                synchronize_mode(PARSER_SYNC_STATEMENT);
                return NULL;
            }

            if (dt == TYPE_STRUCT || dt == TYPE_UNION) {
                error_report(parser.current, "تعريف الهيكل داخل تهيئة 'لكل' غير مدعوم حالياً.");
            }

            Token tok_name = parser.current;
            if (parser.current.type != TOKEN_IDENTIFIER) {
                error_report(parser.current, "متوقع اسم معرّف في تهيئة حلقة 'لكل'.");
                free(type_name);
                free(ptr_base_type_name);
                parser_funcsig_free(func_sig);
                synchronize_mode(PARSER_SYNC_STATEMENT);
                return NULL;
            }
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);

            if (init_q.is_const && !init_q.is_static && parser.current.type != TOKEN_ASSIGN) {
                error_report(parser.current, "الثابت يجب تهيئته.");
            }

            Node* expr = NULL;
            if (parser.current.type == TOKEN_ASSIGN) {
                eat(TOKEN_ASSIGN);
                expr = parse_expression();
            } else if (!init_q.is_static) {
                error_report(parser.current, "متوقع '=' في تهيئة حلقة 'لكل'.");
                eat(TOKEN_ASSIGN);
                expr = parse_expression();
            }
            eat(TOKEN_SEMICOLON);

            init = ast_node_new(NODE_VAR_DECL, tok_name);
            if (!init) {
                free(type_name);
                free(ptr_base_type_name);
                parser_funcsig_free(func_sig);
                return NULL;
            }
            init->data.var_decl.name = name;
            init->data.var_decl.type = dt;
            init->data.var_decl.type_name = type_name;
            init->data.var_decl.ptr_base_type = ptr_base_type;
            init->data.var_decl.ptr_base_type_name = ptr_base_type_name;
            init->data.var_decl.ptr_depth = ptr_depth;
            init->data.var_decl.func_sig = func_sig;
            init->data.var_decl.expression = expr;
            init->data.var_decl.is_global = false;
            init->data.var_decl.is_const = init_q.is_const;
            init->data.var_decl.is_static = init_q.is_static;
        } else {
            if (parser.current.type == TOKEN_IDENTIFIER && parser.next.type == TOKEN_ASSIGN) {
                Token tok_name = parser.current;
                char* name = strdup(parser.current.value);
                eat(TOKEN_IDENTIFIER);
                eat(TOKEN_ASSIGN);
                Node* expr = parse_expression();
                eat(TOKEN_SEMICOLON);

                init = ast_node_new(NODE_ASSIGN, tok_name);
                if (!init) return NULL;
                init->data.assign_stmt.name = name;
                init->data.assign_stmt.expression = expr;
            } else {
                if (parser.current.type != TOKEN_SEMICOLON) {
                    init = parse_expression();
                }
                eat(TOKEN_SEMICOLON);
            }
        }

        Node* cond = NULL;
        if (parser.current.type != TOKEN_SEMICOLON) {
            cond = parse_expression();
        }
        eat(TOKEN_SEMICOLON);

        Node* incr = NULL;
        if (parser.current.type == TOKEN_IDENTIFIER && parser.next.type == TOKEN_ASSIGN) {
            Token tok_name = parser.current;
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);
            eat(TOKEN_ASSIGN);
            Node* expr = parse_expression();

            incr = ast_node_new(NODE_ASSIGN, tok_name);
            if (!incr) return NULL;
            incr->data.assign_stmt.name = name;
            incr->data.assign_stmt.expression = expr;
        } else {
            incr = parse_expression();
        }
        eat(TOKEN_RPAREN);

        Node* body = parse_statement();

        Node* stmt = ast_node_new(NODE_FOR, tok_for);
        if (!stmt) return NULL;
        stmt->data.for_stmt.init = init;
        stmt->data.for_stmt.condition = cond;
        stmt->data.for_stmt.increment = incr;
        stmt->data.for_stmt.body = body;
        return stmt;
    }

    ParserDeclQualifiers decl_q;
    bool has_decl_qual = parser_is_decl_qualifier(parser.current.type);
    if (has_decl_qual) {
        parser_parse_decl_qualifiers(&decl_q);
    } else {
        memset(&decl_q, 0, sizeof(decl_q));
    }

    if (parser_current_is_type_alias_keyword() && parser.next.type == TOKEN_IDENTIFIER) {
        if (decl_q.is_const || decl_q.is_static) {
            error_report(parser.current, "تعريف اسم النوع البديل لا يقبل 'ثابت' أو 'ساكن'.");
        }
        error_report(parser.current, "تعريف 'نوع' مسموح فقط على المستوى العام.");
        return parse_type_alias_declaration(false);
    }

    if (has_decl_qual || parser_current_starts_type()) {
        if (!parser_current_starts_type()) {
            error_report(parser.current, "متوقع نوع بعد واصفات التعريف.");
            synchronize_mode(PARSER_SYNC_STATEMENT);
            return NULL;
        }

        bool is_const = decl_q.is_const;
        bool is_static = decl_q.is_static;
        Token tok_type = parser.current;
        DataType dt = TYPE_INT;
        char* type_name = NULL;
        DataType ptr_base_type = TYPE_INT;
        char* ptr_base_type_name = NULL;
        int ptr_depth = 0;
        FuncPtrSig* func_sig = NULL;
        (void)tok_type;

        if (!parse_type_spec(&dt, &type_name,
                             &ptr_base_type, &ptr_base_type_name,
                             &ptr_depth,
                             &func_sig)) {
            error_report(parser.current, "متوقع نوع.");
            synchronize_mode(PARSER_SYNC_STATEMENT);
            return NULL;
        }

        Token tok_name = parser.current;
        if (parser.current.type != TOKEN_IDENTIFIER) {
            error_report(parser.current, "متوقع اسم معرّف بعد النوع.");
            free(type_name);
            free(ptr_base_type_name);
            parser_funcsig_free(func_sig);
            synchronize_mode(PARSER_SYNC_STATEMENT);
            return NULL;
        }
        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        if (dt == TYPE_VOID) {
            error_report(tok_type, "لا يمكن تعريف متغير من نوع 'عدم'.");
        }

        if (parser.current.type == TOKEN_LBRACKET) {
            if (dt == TYPE_FUNC_PTR) {
                error_report(parser.current, "مصفوفات من نوع مؤشر دالة غير مدعومة بعد.");
                free(type_name);
                free(ptr_base_type_name);
                parser_funcsig_free(func_sig);
                free(name);
                synchronize_mode(PARSER_SYNC_STATEMENT);
                return NULL;
            }
            int* dims = NULL;
            int dim_count = 0;
            int64_t total_elems = 0;
            if (!parse_array_dimensions(&dims, &dim_count, &total_elems)) {
                free(type_name);
                free(ptr_base_type_name);
                parser_funcsig_free(func_sig);
                free(name);
                synchronize_mode(PARSER_SYNC_STATEMENT);
                return NULL;
            }

            int init_count = 0;
            bool has_init = false;
            Node* init_vals = parse_array_initializer_list(&init_count, &has_init);

            eat(TOKEN_DOT);

            Node* stmt = ast_node_new(NODE_ARRAY_DECL, tok_name);
            if (!stmt) {
                free(dims);
                free(type_name);
                free(name);
                parser_funcsig_free(func_sig);
                return NULL;
            }
            stmt->data.array_decl.name = name;
            stmt->data.array_decl.element_type = dt;
            stmt->data.array_decl.element_type_name = type_name;
            stmt->data.array_decl.element_ptr_base_type = ptr_base_type;
            stmt->data.array_decl.element_ptr_base_type_name = ptr_base_type_name;
            stmt->data.array_decl.element_ptr_depth = ptr_depth;
            stmt->data.array_decl.element_struct_size = 0;
            stmt->data.array_decl.element_struct_align = 0;
            stmt->data.array_decl.dims = dims;
            stmt->data.array_decl.dim_count = dim_count;
            stmt->data.array_decl.total_elems = total_elems;
            stmt->data.array_decl.is_global = false;
            stmt->data.array_decl.is_const = is_const;
            stmt->data.array_decl.is_static = is_static;
            stmt->data.array_decl.has_init = has_init;
            stmt->data.array_decl.init_values = init_vals;
            stmt->data.array_decl.init_count = init_count;
            parser_funcsig_free(func_sig);
            return stmt;
        }

        // تعريف هيكل (محلي): هيكل <T> <name>.
        if (dt == TYPE_STRUCT || dt == TYPE_UNION) {
            if (is_const) {
                error_report(decl_q.tok_const, "لا يمكن تعريف نوع مركب ثابت بدون تهيئة (غير مدعوم حالياً).");
            }
            eat(TOKEN_DOT);

            Node* stmt = ast_node_new(NODE_VAR_DECL, tok_name);
            if (!stmt) { free(type_name); return NULL; }
            stmt->data.var_decl.name = name;
            stmt->data.var_decl.type = dt;
            stmt->data.var_decl.type_name = type_name;
            stmt->data.var_decl.ptr_base_type = ptr_base_type;
            stmt->data.var_decl.ptr_base_type_name = ptr_base_type_name;
            stmt->data.var_decl.ptr_depth = ptr_depth;
            stmt->data.var_decl.func_sig = func_sig;
            stmt->data.var_decl.expression = NULL;
            stmt->data.var_decl.is_global = false;
            stmt->data.var_decl.is_const = is_const;
            stmt->data.var_decl.is_static = is_static;
            return stmt;
        }

        if (is_const && !is_static && parser.current.type != TOKEN_ASSIGN) {
            error_report(parser.current, "الثابت يجب تهيئته.");
        }

        Node* expr = NULL;
        if (parser.current.type == TOKEN_ASSIGN) {
            eat(TOKEN_ASSIGN);
            expr = parse_expression();
        } else if (!is_static) {
            eat(TOKEN_ASSIGN);
            expr = parse_expression();
        }
        eat(TOKEN_DOT);

        Node* stmt = ast_node_new(NODE_VAR_DECL, tok_name);
        if (!stmt) { free(type_name); return NULL; }
        stmt->data.var_decl.name = name;
        stmt->data.var_decl.type = dt;
        stmt->data.var_decl.type_name = type_name;
        stmt->data.var_decl.ptr_base_type = ptr_base_type;
        stmt->data.var_decl.ptr_base_type_name = ptr_base_type_name;
        stmt->data.var_decl.ptr_depth = ptr_depth;
        stmt->data.var_decl.func_sig = func_sig;
        stmt->data.var_decl.expression = expr;
        stmt->data.var_decl.is_global = false;
        stmt->data.var_decl.is_const = is_const;
        stmt->data.var_decl.is_static = is_static;
        return stmt;
    }

    if (parser.current.type == TOKEN_STAR) {
        Token tok_star = parser.current;
        Node* target = parse_unary();
        if (target && target->type == NODE_UNARY_OP && target->data.unary_op.op == UOP_DEREF &&
            parser.current.type == TOKEN_ASSIGN) {
            eat(TOKEN_ASSIGN);
            Node* value = parse_expression();
            eat(TOKEN_DOT);

            Node* stmt = ast_node_new(NODE_DEREF_ASSIGN, tok_star);
            if (!stmt) return NULL;
            stmt->data.deref_assign.target = target;
            stmt->data.deref_assign.value = value;
            return stmt;
        }

        eat(TOKEN_DOT);
        return target;
    }

    if (parser.current.type == TOKEN_IDENTIFIER) {
        Token tok_name = parser.current;

        // إسناد إلى عضو: س:حقل = ... .
        if (parser.next.type == TOKEN_COLON) {
            Node* target = parse_expression();
            eat(TOKEN_ASSIGN);
            Node* value = parse_expression();
            eat(TOKEN_DOT);

            Node* stmt = ast_node_new(NODE_MEMBER_ASSIGN, tok_name);
            if (!stmt) return NULL;
            stmt->data.member_assign.target = target;
            stmt->data.member_assign.value = value;
            return stmt;
        }

        if (parser.next.type == TOKEN_LBRACKET) {
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);
            int index_count = 0;
            Node* indices = parse_array_indices(&index_count);

            // إسناد إلى عضو عنصر مصفوفة: س[0]:حقل = ...
            if (parser.current.type == TOKEN_COLON) {
                Node* target = ast_node_new(NODE_ARRAY_ACCESS, tok_name);
                if (!target) return NULL;
                target->data.array_op.name = name;
                target->data.array_op.indices = indices;
                target->data.array_op.index_count = index_count;
                target->data.array_op.value = NULL;

                while (parser.current.type == TOKEN_COLON) {
                    Token tok_colon = parser.current;
                    eat(TOKEN_COLON);
                    if (parser.current.type != TOKEN_IDENTIFIER) {
                        error_report(parser.current, "متوقع اسم عضو بعد ':'.");
                        break;
                    }
                    char* member = strdup(parser.current.value);
                    eat(TOKEN_IDENTIFIER);

                    Node* ma = ast_node_new(NODE_MEMBER_ACCESS, tok_colon);
                    if (!ma) return NULL;
                    ma->data.member_access.base = target;
                    ma->data.member_access.member = member;
                    target = ma;
                }

                eat(TOKEN_ASSIGN);
                Node* value = parse_expression();
                eat(TOKEN_DOT);

                Node* stmt = ast_node_new(NODE_MEMBER_ASSIGN, tok_name);
                if (!stmt) return NULL;
                stmt->data.member_assign.target = target;
                stmt->data.member_assign.value = value;
                return stmt;
            }

            if (parser.current.type == TOKEN_ASSIGN) {
                eat(TOKEN_ASSIGN);
                Node* value = parse_expression();
                eat(TOKEN_DOT);

                Node* stmt = ast_node_new(NODE_ARRAY_ASSIGN, tok_name);
                if (!stmt) return NULL;
                stmt->data.array_op.name = name;
                stmt->data.array_op.indices = indices;
                stmt->data.array_op.index_count = index_count;
                stmt->data.array_op.value = value;
                return stmt;
            } else if (parser.current.type == TOKEN_INC || parser.current.type == TOKEN_DEC) {
                // array[index]++.
                Node* access = ast_node_new(NODE_ARRAY_ACCESS, tok_name);
                if (!access) return NULL;
                access->data.array_op.name = name;
                access->data.array_op.indices = indices;
                access->data.array_op.index_count = index_count;
                access->data.array_op.value = NULL;

                Token tok_op = parser.current;
                UnaryOpType op = (parser.current.type == TOKEN_INC) ? UOP_INC : UOP_DEC;
                eat(parser.current.type);
                eat(TOKEN_DOT);

                Node* stmt = ast_node_new(NODE_POSTFIX_OP, tok_op);
                if (!stmt) return NULL;
                stmt->data.unary_op.operand = access;
                stmt->data.unary_op.op = op;
                return stmt;
            }
        } else if (parser.next.type == TOKEN_ASSIGN) {
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);
            eat(TOKEN_ASSIGN);
            Node* expr = parse_expression();
            eat(TOKEN_DOT);

            Node* stmt = ast_node_new(NODE_ASSIGN, tok_name);
            if (!stmt) return NULL;
            stmt->data.assign_stmt.name = name;
            stmt->data.assign_stmt.expression = expr;
            return stmt;
        } else if (parser.next.type == TOKEN_LPAREN) {
            Node* expr = parse_expression();
            eat(TOKEN_DOT);

            Node* stmt = ast_node_new(NODE_CALL_STMT, tok_name);
            if (!stmt) return NULL;
            stmt->data.call.name = expr->data.call.name;
            stmt->data.call.args = expr->data.call.args;
            free(expr);
            return stmt;
        } else if (parser.next.type == TOKEN_INC || parser.next.type == TOKEN_DEC) {
            Node* expr = parse_expression();
            eat(TOKEN_DOT);
            return expr;
        }
    }

    if (!parser.panic_mode) {
        error_report(parser.current, "وحدة أو جملة غير متوقعة.");
        parser.panic_mode = true;
    }
    return NULL;
}
