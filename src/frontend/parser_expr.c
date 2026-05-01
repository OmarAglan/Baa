// --- تحليل التعبيرات (حسب مستويات الأولوية) ---

// المستوى 1: التعبيرات الأساسية واللاحقة (Postfix)
Node* parse_primary() {
    Node* node = NULL;

    if (parser.current.type == TOKEN_INT) {
        Token tok = parser.current;
        node = ast_node_new(NODE_INT, tok);
        if (!node) return NULL;
        int64_t v = 0;
        (void)parse_int_token_checked(parser.current, &v);
        node->data.integer.value = v;
        eat(TOKEN_INT);
    }
    else if (parser.current.type == TOKEN_FLOAT) {
        Token tok = parser.current;
        node = ast_node_new(NODE_FLOAT, tok);
        if (!node) return NULL;
        double v = 0.0;
        if (!parse_float_token_checked(parser.current, &v)) {
            error_report(parser.current, "صيغة العدد العشري غير صالحة.");
        }
        node->data.float_lit.value = v;
        uint64_t bits = 0;
        memcpy(&bits, &v, sizeof(bits));
        node->data.float_lit.bits = bits;
        eat(TOKEN_FLOAT);
    }
    else if (parser.current.type == TOKEN_STRING) {
        Token tok = parser.current;
        node = ast_node_new(NODE_STRING, tok);
        if (!node) return NULL;
        node->data.string_lit.value = strdup(parser.current.value);
        node->data.string_lit.id = -1;
        eat(TOKEN_STRING);
    }
    else if (parser.current.type == TOKEN_CHAR) {
        Token tok = parser.current;
        node = ast_node_new(NODE_CHAR, tok);
        if (!node) return NULL;
        uint32_t cp = 0;
        if (!utf8_decode_one(parser.current.value, &cp)) {
            error_report(parser.current, "حرف UTF-8 غير صالح.");
            cp = (uint32_t)'?';
        }
        node->data.char_lit.value = (int)cp;
        eat(TOKEN_CHAR);
    }
    else if (parser.current.type == TOKEN_TRUE) {
        Token tok = parser.current;
        node = ast_node_new(NODE_BOOL, tok);
        if (!node) return NULL;
        node->data.bool_lit.value = true;
        eat(TOKEN_TRUE);
    }
    else if (parser.current.type == TOKEN_FALSE) {
        Token tok = parser.current;
        node = ast_node_new(NODE_BOOL, tok);
        if (!node) return NULL;
        node->data.bool_lit.value = false;
        eat(TOKEN_FALSE);
    }
    else if (parser.current.type == TOKEN_KEYWORD_VOID) {
        Token tok = parser.current;
        node = ast_node_new(NODE_NULL, tok);
        if (!node) return NULL;
        eat(TOKEN_KEYWORD_VOID);
    }
    else if (parser.current.type == TOKEN_SIZEOF) {
        Token tok = parser.current;
        eat(TOKEN_SIZEOF);
        eat(TOKEN_LPAREN);

        node = ast_node_new(NODE_SIZEOF, tok);
        if (!node) return NULL;
        node->data.sizeof_expr.has_type_form = false;
        node->data.sizeof_expr.target_type = TYPE_INT;
        node->data.sizeof_expr.target_type_name = NULL;
        node->data.sizeof_expr.expression = NULL;
        node->data.sizeof_expr.size_bytes = 0;
        node->data.sizeof_expr.size_known = false;

        if (parser_current_starts_type()) {
            DataType dt = TYPE_INT;
            char* tn = NULL;
            DataType ptr_base = TYPE_INT;
            char* ptr_base_name = NULL;
            int ptr_depth = 0;
            FuncPtrSig* fsig = NULL;
            if (!parse_type_spec(&dt, &tn, &ptr_base, &ptr_base_name, &ptr_depth, &fsig)) {
                error_report(parser.current, "متوقع نوع أو تعبير داخل 'حجم(...)'.");
                free(tn);
                free(ptr_base_name);
            } else {
                if (dt == TYPE_FUNC_PTR) {
                    error_report(parser.current, "نوع 'حجم(دالة(...))' غير مدعوم حالياً.");
                    free(tn);
                    free(ptr_base_name);
                    parser_funcsig_free(fsig);
                } else {
                    node->data.sizeof_expr.has_type_form = true;
                    node->data.sizeof_expr.target_type = dt;
                    node->data.sizeof_expr.target_type_name = tn;
                    free(ptr_base_name);
                    parser_funcsig_free(fsig);
                }
            }
        } else {
            Node* expr = parse_expression();
            node->data.sizeof_expr.has_type_form = false;
            node->data.sizeof_expr.expression = expr;
        }

        eat(TOKEN_RPAREN);
    }
    else if (parser.current.type == TOKEN_IDENTIFIER) {
        Token tok_ident = parser.current;
        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        // استدعاء دالة: اسم(...)
        if (parser.current.type == TOKEN_LPAREN) {
            eat(TOKEN_LPAREN);
            Node* head_arg = NULL;
            Node* tail_arg = NULL;
            int arg_index = 0;
            bool is_variadic_next = (name && strcmp(name, "معامل_تالي") == 0);

            // تحليل الوسائط (مفصولة بفاصلة)
            if (parser.current.type != TOKEN_RPAREN) {
                while (1) {
                    Node* arg = NULL;

                    // دعم خاص لـ معامل_تالي(قائمة، نوع): المعامل الثاني يقبل "نوع" مباشرةً.
                    if (is_variadic_next && arg_index == 1 && parser_current_starts_type()) {
                        Token tok_type = parser.current;
                        DataType dt = TYPE_INT;
                        char* tn = NULL;
                        DataType ptr_base_type = TYPE_INT;
                        char* ptr_base_type_name = NULL;
                        int ptr_depth = 0;
                        FuncPtrSig* func_sig = NULL;

                        if (!parse_type_spec(&dt, &tn,
                                             &ptr_base_type, &ptr_base_type_name,
                                             &ptr_depth,
                                             &func_sig)) {
                            error_report(parser.current, "متوقع نوع كمعامل ثانٍ في 'معامل_تالي'.");
                        } else {
                            arg = ast_node_new(NODE_SIZEOF, tok_type);
                            if (!arg) {
                                free(tn);
                                free(ptr_base_type_name);
                                parser_funcsig_free(func_sig);
                                return NULL;
                            }
                            arg->data.sizeof_expr.has_type_form = true;
                            arg->data.sizeof_expr.target_type = dt;
                            arg->data.sizeof_expr.target_type_name = tn;
                            arg->data.sizeof_expr.expression = NULL;
                            arg->data.sizeof_expr.size_bytes = 0;
                            arg->data.sizeof_expr.size_known = false;
                        }

                        free(ptr_base_type_name);
                        parser_funcsig_free(func_sig);
                    }

                    if (!arg) {
                        arg = parse_expression();
                    }
                    if (!arg) break; // تعافٍ من الخطأ
                    arg->next = NULL;
                    if (head_arg == NULL) { head_arg = arg; tail_arg = arg; }
                    else { tail_arg->next = arg; tail_arg = arg; }
                    arg_index++;

                    if (parser.current.type == TOKEN_COMMA) eat(TOKEN_COMMA);
                    else break;
                }
            }
            eat(TOKEN_RPAREN);

            node = ast_node_new(NODE_CALL_EXPR, tok_ident);
            if (!node) return NULL;
            node->data.call.name = name;
            node->data.call.args = head_arg;
        }
        // الوصول لمصفوفة: اسم[فهرس][فهرس]...
        else if (parser.current.type == TOKEN_LBRACKET) {
            int index_count = 0;
            Node* indices = parse_array_indices(&index_count);

            node = ast_node_new(NODE_ARRAY_ACCESS, tok_ident);
            if (!node) return NULL;
            node->data.array_op.name = name;
            node->data.array_op.indices = indices;
            node->data.array_op.index_count = index_count;
            node->data.array_op.value = NULL;
        }
        // متغير عادي
        else {
            node = ast_node_new(NODE_VAR_REF, tok_ident);
            if (!node) return NULL;
            node->data.var_ref.name = name;
        }
    }
    else if (parser.current.type == TOKEN_LPAREN) {
        eat(TOKEN_LPAREN);
        node = parse_expression();
        eat(TOKEN_RPAREN);
    }
    else {
        if (!parser.panic_mode) {
            error_report_token_hint(parser.current,
                                    "ابدأ التعبير بعدد أو معرّف أو '(' أو قيمة منطقية.",
                                    "متوقع تعبير.");
            parser.panic_mode = true;
        }
        return NULL;
    }

    // الوصول للأعضاء/القيم المؤهلة: <expr>:<member> (يمكن تكراره للتعشيش)
    while (node && parser.current.type == TOKEN_COLON) {
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
        ma->data.member_access.base = node;
        ma->data.member_access.member = member;
        node = ma;
    }

    // التحقق من العمليات اللاحقة (Postfix Operators: ++, --)
    if (node && (parser.current.type == TOKEN_INC || parser.current.type == TOKEN_DEC)) {
        Token tok_op = parser.current;
        UnaryOpType op = (parser.current.type == TOKEN_INC) ? UOP_INC : UOP_DEC;
        eat(parser.current.type);

        Node* postfix = ast_node_new(NODE_POSTFIX_OP, tok_op);
        if (!postfix) return NULL;
        postfix->data.unary_op.operand = node;
        postfix->data.unary_op.op = op;
        return postfix;
    }

    return node;
}

// المستوى 1.5: العمليات الأحادية والبادئة (!, ~, -, ++, --)
Node* parse_unary() {
    if (parser.current.type == TOKEN_CAST) {
        Token tok_cast = parser.current;
        eat(TOKEN_CAST);
        eat(TOKEN_LT);

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
            error_report(parser.current, "متوقع نوع صحيح داخل تحويل 'كـ<...>'.");
            parser_funcsig_free(target_func_sig);
            target_func_sig = NULL;
        } else if (target_type == TYPE_FUNC_PTR) {
            error_report(parser.current, "التحويل الصريح إلى/من مؤشر دالة غير مدعوم حالياً.");
            parser_funcsig_free(target_func_sig);
            target_func_sig = NULL;
        }

        eat(TOKEN_GT);
        eat(TOKEN_LPAREN);
        Node* source_expr = parse_expression();
        eat(TOKEN_RPAREN);

        Node* node = ast_node_new(NODE_CAST, tok_cast);
        if (!node) {
            free(target_type_name);
            free(target_ptr_base_type_name);
            return NULL;
        }

        node->data.cast_expr.target_type = target_type;
        node->data.cast_expr.target_type_name = target_type_name;
        node->data.cast_expr.target_ptr_base_type = target_ptr_base_type;
        node->data.cast_expr.target_ptr_base_type_name = target_ptr_base_type_name;
        node->data.cast_expr.target_ptr_depth = target_ptr_depth;
        node->data.cast_expr.expression = source_expr;
        return node;
    }

    if (parser.current.type == TOKEN_MINUS || parser.current.type == TOKEN_NOT ||
        parser.current.type == TOKEN_TILDE || parser.current.type == TOKEN_AMP ||
        parser.current.type == TOKEN_STAR ||
        parser.current.type == TOKEN_INC || parser.current.type == TOKEN_DEC) {

        Token tok_op = parser.current;
        
        UnaryOpType op;
        if (parser.current.type == TOKEN_MINUS) op = UOP_NEG;
        else if (parser.current.type == TOKEN_NOT) op = UOP_NOT;
        else if (parser.current.type == TOKEN_TILDE) op = UOP_BIT_NOT;
        else if (parser.current.type == TOKEN_AMP) op = UOP_ADDR;
        else if (parser.current.type == TOKEN_STAR) op = UOP_DEREF;
        else if (parser.current.type == TOKEN_INC) op = UOP_INC;
        else op = UOP_DEC;

        eat(parser.current.type);
        Node* operand = parse_unary();
        
        if (!operand) return NULL; // Error propagation

        Node* node = ast_node_new(NODE_UNARY_OP, tok_op);
        if (!node) return NULL;
        node->data.unary_op.operand = operand;
        node->data.unary_op.op = op;
        return node;
    }
    return parse_primary();
}

// المستوى 2: عمليات الضرب والقسمة وباقي القسمة (*، /، %)
Node* parse_multiplicative() {
    Node* left = parse_unary();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_STAR || parser.current.type == TOKEN_SLASH || parser.current.type == TOKEN_PERCENT) {
        Token tok_op = parser.current;
        OpType op;
        if (parser.current.type == TOKEN_STAR) op = OP_MUL;
        else if (parser.current.type == TOKEN_SLASH) op = OP_DIV;
        else op = OP_MOD;
        
        eat(parser.current.type);
        Node* right = parse_unary();
        if (!right) return NULL;

        // **Constant Folding Optimization**
        // ملاحظة: يجب أن تتطابق مع قواعد ترقيم الثوابت (ص٣٢ إذا كانت ضمن المدى، وإلا ص٦٤).
        // لذلك نُنفّذ الطي على 32-بت عندما يكون الطرفان ضمن مدى ص٣٢، وإلا على 64-بت.
        if (left->type == NODE_INT && right->type == NODE_INT) {
            int64_t a = left->data.integer.value;
            int64_t b = right->data.integer.value;
            int64_t result = 0;
            bool i32_mode = parser_int_literal_is_i32(a) && parser_int_literal_is_i32(b);

            if (i32_mode) {
                int32_t sa = (int32_t)a;
                int32_t sb = (int32_t)b;
                uint32_t ua = (uint32_t)sa;
                uint32_t ub = (uint32_t)sb;

                if (op == OP_MUL) {
                    uint32_t ur = (uint32_t)((uint64_t)ua * (uint64_t)ub);
                    result = parser_sign_extend_u32(ur);
                } else if (op == OP_DIV) {
                    if (sb == 0) {
                        error_report(parser.current, "قسمة على صفر.");
                        result = 0;
                    } else if (sa == INT32_MIN && sb == -1) {
                        // التفاف آمن (بدون UB)
                        result = (int64_t)INT32_MIN;
                    } else {
                        result = (int64_t)(sa / sb);
                    }
                } else if (op == OP_MOD) {
                    if (sb == 0) {
                        error_report(parser.current, "باقي قسمة على صفر.");
                        result = 0;
                    } else if (sa == INT32_MIN && sb == -1) {
                        result = 0;
                    } else {
                        result = (int64_t)(sa % sb);
                    }
                }
            } else {
                // 64-بت مع التفاف مكمل الاثنين (تجنب UB في C عند تجاوز signed).
                uint64_t ua = (uint64_t)a;
                uint64_t ub = (uint64_t)b;

                if (op == OP_MUL) {
                    result = (int64_t)(ua * ub);
                } else if (op == OP_DIV) {
                    if (b == 0) {
                        error_report(parser.current, "قسمة على صفر.");
                        result = 0;
                    } else if (a == INT64_MIN && b == -1) {
                        result = INT64_MIN;
                    } else {
                        result = a / b;
                    }
                } else if (op == OP_MOD) {
                    if (b == 0) {
                        error_report(parser.current, "باقي قسمة على صفر.");
                        result = 0;
                    } else if (a == INT64_MIN && b == -1) {
                        result = 0;
                    } else {
                        result = a % b;
                    }
                }
            }

            left->data.integer.value = result;
            free(right);
            continue;
        }

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        left = new_node;
    }
    return left;
}

// المستوى 3: عمليات الجمع والطرح (+، -)
Node* parse_additive() {
    Node* left = parse_multiplicative();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_PLUS || parser.current.type == TOKEN_MINUS) {
        Token tok_op = parser.current;
        OpType op = (parser.current.type == TOKEN_PLUS) ? OP_ADD : OP_SUB;
        eat(parser.current.type);
        Node* right = parse_multiplicative();
        if (!right) return NULL;

        // **Constant Folding Optimization**
        // مطابق لقواعد ترقيم الثوابت: i32 داخل المدى، وإلا i64.
        if (left->type == NODE_INT && right->type == NODE_INT) {
            int64_t a = left->data.integer.value;
            int64_t b = right->data.integer.value;
            int64_t result = 0;
            bool i32_mode = parser_int_literal_is_i32(a) && parser_int_literal_is_i32(b);

            if (i32_mode) {
                uint32_t ua = (uint32_t)(int32_t)a;
                uint32_t ub = (uint32_t)(int32_t)b;
                uint32_t ur = (op == OP_ADD) ? (ua + ub) : (ua - ub);
                result = parser_sign_extend_u32(ur);
            } else {
                uint64_t ua = (uint64_t)a;
                uint64_t ub = (uint64_t)b;
                result = (op == OP_ADD) ? (int64_t)(ua + ub) : (int64_t)(ua - ub);
            }

            left->data.integer.value = result;
            free(right);
            continue;
        }

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        left = new_node;
    }
    return left;
}

// المستوى 4: عمليات الإزاحة البتية (<<، >>)
Node* parse_shift() {
    Node* left = parse_additive();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_SHL || parser.current.type == TOKEN_SHR) {
        Token tok_op = parser.current;
        OpType op = (parser.current.type == TOKEN_SHL) ? OP_SHL : OP_SHR;
        eat(parser.current.type);
        Node* right = parse_additive();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        left = new_node;
    }

    return left;
}

// المستوى 5: عمليات المقارنة العلائقية
Node* parse_relational() {
    Node* left = parse_shift();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_LT || parser.current.type == TOKEN_GT ||
           parser.current.type == TOKEN_LTE || parser.current.type == TOKEN_GTE) {
        Token tok_op = parser.current;
        OpType op;
        if (parser.current.type == TOKEN_LT) op = OP_LT;
        else if (parser.current.type == TOKEN_GT) op = OP_GT;
        else if (parser.current.type == TOKEN_LTE) op = OP_LTE;
        else op = OP_GTE;
        eat(parser.current.type);
        Node* right = parse_shift();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        left = new_node;
    }
    return left;
}

// المستوى 6: عمليات التساوي
Node* parse_equality() {
    Node* left = parse_relational();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_EQ || parser.current.type == TOKEN_NEQ) {
        Token tok_op = parser.current;
        OpType op = (parser.current.type == TOKEN_EQ) ? OP_EQ : OP_NEQ;
        eat(parser.current.type);
        Node* right = parse_relational();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        left = new_node;
    }
    return left;
}

// المستوى 7: العملية البتية AND (&)
Node* parse_bitwise_and() {
    Node* left = parse_equality();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_AMP) {
        Token tok_op = parser.current;
        eat(TOKEN_AMP);
        Node* right = parse_equality();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = OP_BIT_AND;
        left = new_node;
    }
    return left;
}

// المستوى 8: العملية البتية XOR (^)
Node* parse_bitwise_xor() {
    Node* left = parse_bitwise_and();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_CARET) {
        Token tok_op = parser.current;
        eat(TOKEN_CARET);
        Node* right = parse_bitwise_and();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = OP_BIT_XOR;
        left = new_node;
    }
    return left;
}

// المستوى 9: العملية البتية OR (|)
Node* parse_bitwise_or() {
    Node* left = parse_bitwise_xor();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_PIPE) {
        Token tok_op = parser.current;
        eat(TOKEN_PIPE);
        Node* right = parse_bitwise_xor();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = OP_BIT_OR;
        left = new_node;
    }
    return left;
}

// المستوى 10: العملية المنطقية "و" (&&)
Node* parse_logical_and() {
    Node* left = parse_bitwise_or();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_AND) {
        Token tok_op = parser.current;
        eat(TOKEN_AND);
        Node* right = parse_bitwise_or();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = OP_AND;
        left = new_node;
    }
    return left;
}

// المستوى 11: العملية المنطقية "أو" (||)
Node* parse_logical_or() {
    Node* left = parse_logical_and();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_OR) {
        Token tok_op = parser.current;
        eat(TOKEN_OR);
        Node* right = parse_logical_and();
        if (!right) return NULL;

        Node* new_node = ast_node_new(NODE_BIN_OP, tok_op);
        if (!new_node) return NULL;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = OP_OR;
        left = new_node;
    }
    return left;
}

/**
 * @brief نقطة الدخول الرئيسية لتحليل التعبيرات.
 */
Node* parse_expression() {
    return parse_logical_or();
}

// --- تحليل الجمل البرمجية (Statements) ---
