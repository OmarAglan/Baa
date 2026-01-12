/**
 * @file parser.c
 * @brief يقوم بتحليل الوحدات اللفظية وبناء شجرة الإعراب المجردة (AST) باستخدام طريقة الانحدار العودي (Recursive Descent).
 * @version 0.2.7 (Constants & Immutability)
 */

#include "baa.h"

// كائن المحلل القواعدي الذي يدير الحالة الحالية والوحدة القادمة
Parser parser;

// --- دالات مساعدة ---

/**
 * @brief التقدم للوحدة اللفظية التالية.
 */
void advance() {
    parser.current = parser.next;
    
    while (true) {
        parser.next = lexer_next_token(parser.lexer);
        if (parser.next.type != TOKEN_INVALID) break;
        // إذا وجدنا وحدة غير صالحة، نبلغ عن الخطأ ونستمر في البحث
        error_report(parser.next, "Found invalid token.");
    }
}

/**
 * @brief تهيئة المحلل القواعدي ولبدء قراءة أول وحدتين.
 */
void init_parser(Lexer* l) {
    parser.lexer = l;
    parser.panic_mode = false;
    parser.had_error = false;
    advance(); // Load first token into parser.next (current is garbage initially)
    parser.current = parser.next; // Sync
    advance(); // Load next token
}

/**
 * @brief التأكد من أن الوحدة الحالية من نوع معين واستهلاكها، وإلا يتم إظهار خطأ.
 */
void eat(BaaTokenType type) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    if (parser.panic_mode) return;
    
    parser.panic_mode = true;
    
    // تحسين رسالة الخطأ
    // إذا كانت الوحدة الحالية لها قيمة نصية (مثل اسم متغير)، اعرضها
    // وإلا اعرض نوعها كنص
    const char* found_text = parser.current.value ? parser.current.value : token_type_to_str(parser.current.type);
    
    error_report(parser.current, "Expected '%s' but found '%s'", token_type_to_str(type), found_text);
}

/**
 * @brief التحقق مما إذا كانت الوحدة الحالية تمثل نوع بيانات (صحيح أو نص).
 */
bool is_type_keyword(BaaTokenType type) {
    return (type == TOKEN_KEYWORD_INT || type == TOKEN_KEYWORD_STRING);
}

/**
 * @brief تحويل وحدة النوع إلى قيمة DataType.
 */
DataType token_to_datatype(BaaTokenType type) {
    if (type == TOKEN_KEYWORD_STRING) return TYPE_STRING;
    return TYPE_INT;
}

/**
 * @brief محاولة التعافي من الخطأ بالقفز إلى بداية الجملة التالية.
 */
void synchronize() {
    parser.panic_mode = false;

    while (parser.current.type != TOKEN_EOF) {
        // إذا وجدنا فاصلة منقوطة، فغالباً انتهت الجملة السابقة
        if (parser.current.type == TOKEN_SEMICOLON || parser.current.type == TOKEN_DOT) {
            advance();
            return;
        }

        // إذا وجدنا كلمة مفتاحية تبدأ جملة جديدة
        switch (parser.current.type) {
            case TOKEN_KEYWORD_INT:
            case TOKEN_KEYWORD_STRING:
            case TOKEN_CONST:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_FOR:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
            case TOKEN_SWITCH:
            case TOKEN_BREAK:
            case TOKEN_CONTINUE:
                return;
            default:
                ;
        }

        advance();
    }
}

// --- تصريحات مسبقة ---
Node* parse_expression();
Node* parse_statement();
Node* parse_block();
Node* parse_primary();

// --- تحليل التعبيرات (حسب مستويات الأولوية) ---

// المستوى 1: التعبيرات الأساسية واللاحقة (Postfix)
Node* parse_primary() {
    Node* node = NULL;

    if (parser.current.type == TOKEN_INT) {
        node = malloc(sizeof(Node));
        node->type = NODE_INT;
        node->data.integer.value = atoi(parser.current.value);
        node->next = NULL;
        eat(TOKEN_INT);
    }
    else if (parser.current.type == TOKEN_STRING) {
        node = malloc(sizeof(Node));
        node->type = NODE_STRING;
        node->data.string_lit.value = strdup(parser.current.value);
        node->data.string_lit.id = -1;
        node->next = NULL;
        eat(TOKEN_STRING);
    }
    else if (parser.current.type == TOKEN_CHAR) {
        node = malloc(sizeof(Node));
        node->type = NODE_CHAR;
        node->data.char_lit.value = (int)parser.current.value[0];
        node->next = NULL;
        eat(TOKEN_CHAR);
    }
    else if (parser.current.type == TOKEN_IDENTIFIER) {
        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        // استدعاء دالة: اسم(...)
        if (parser.current.type == TOKEN_LPAREN) {
            eat(TOKEN_LPAREN);
            Node* head_arg = NULL;
            Node* tail_arg = NULL;

            // تحليل الوسائط (مفصولة بفاصلة)
            if (parser.current.type != TOKEN_RPAREN) {
                while (1) {
                    Node* arg = parse_expression();
                    if (!arg) break; // Error recovery
                    arg->next = NULL;
                    if (head_arg == NULL) { head_arg = arg; tail_arg = arg; }
                    else { tail_arg->next = arg; tail_arg = arg; }

                    if (parser.current.type == TOKEN_COMMA) eat(TOKEN_COMMA);
                    else break;
                }
            }
            eat(TOKEN_RPAREN);

            node = malloc(sizeof(Node));
            node->type = NODE_CALL_EXPR;
            node->data.call.name = name;
            node->data.call.args = head_arg;
            node->next = NULL;
        }
        // الوصول لمصفوفة: اسم[فهرس]
        else if (parser.current.type == TOKEN_LBRACKET) {
            eat(TOKEN_LBRACKET);
            Node* index = parse_expression();
            eat(TOKEN_RBRACKET);

            node = malloc(sizeof(Node));
            node->type = NODE_ARRAY_ACCESS;
            node->data.array_op.name = name;
            node->data.array_op.index = index;
            node->data.array_op.value = NULL;
            node->next = NULL;
        }
        // متغير عادي
        else {
            node = malloc(sizeof(Node));
            node->type = NODE_VAR_REF;
            node->data.var_ref.name = name;
            node->next = NULL;
        }
    }
    else if (parser.current.type == TOKEN_LPAREN) {
        eat(TOKEN_LPAREN);
        node = parse_expression();
        eat(TOKEN_RPAREN);
    }
    else {
        if (!parser.panic_mode) {
            error_report(parser.current, "Expected expression.");
            parser.panic_mode = true;
        }
        return NULL;
    }

    // التحقق من العمليات اللاحقة (Postfix Operators: ++, --)
    if (node && (parser.current.type == TOKEN_INC || parser.current.type == TOKEN_DEC)) {
        UnaryOpType op = (parser.current.type == TOKEN_INC) ? UOP_INC : UOP_DEC;
        eat(parser.current.type);

        Node* postfix = malloc(sizeof(Node));
        postfix->type = NODE_POSTFIX_OP;
        postfix->data.unary_op.operand = node;
        postfix->data.unary_op.op = op;
        postfix->next = NULL;
        return postfix;
    }

    return node;
}

// المستوى 1.5: العمليات الأحادية والبادئة (!, -, ++, --)
Node* parse_unary() {
    if (parser.current.type == TOKEN_MINUS || parser.current.type == TOKEN_NOT ||
        parser.current.type == TOKEN_INC || parser.current.type == TOKEN_DEC) {
        
        UnaryOpType op;
        if (parser.current.type == TOKEN_MINUS) op = UOP_NEG;
        else if (parser.current.type == TOKEN_NOT) op = UOP_NOT;
        else if (parser.current.type == TOKEN_INC) op = UOP_INC;
        else op = UOP_DEC;

        eat(parser.current.type);
        Node* operand = parse_unary();
        
        if (!operand) return NULL; // Error propagation

        Node* node = malloc(sizeof(Node));
        node->type = NODE_UNARY_OP;
        node->data.unary_op.operand = operand;
        node->data.unary_op.op = op;
        node->next = NULL;
        return node;
    }
    return parse_primary();
}

// المستوى 2: عمليات الضرب والقسمة وباقي القسمة (*، /، %)
Node* parse_multiplicative() {
    Node* left = parse_unary();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_STAR || parser.current.type == TOKEN_SLASH || parser.current.type == TOKEN_PERCENT) {
        OpType op;
        if (parser.current.type == TOKEN_STAR) op = OP_MUL;
        else if (parser.current.type == TOKEN_SLASH) op = OP_DIV;
        else op = OP_MOD;
        
        eat(parser.current.type);
        Node* right = parse_unary();
        if (!right) return NULL;

        // **Constant Folding Optimization**
        if (left->type == NODE_INT && right->type == NODE_INT) {
            int result = 0;
            if (op == OP_MUL) {
                result = left->data.integer.value * right->data.integer.value;
            } else if (op == OP_DIV) {
                if (right->data.integer.value == 0) { 
                    error_report(parser.current, "Division by zero.");
                    result = 0; 
                } else {
                    result = left->data.integer.value / right->data.integer.value;
                }
            } else if (op == OP_MOD) {
                if (right->data.integer.value == 0) { 
                    error_report(parser.current, "Modulo by zero.");
                    result = 0;
                } else {
                    result = left->data.integer.value % right->data.integer.value;
                }
            }
            
            left->data.integer.value = result;
            free(right);
            continue;
        }

        Node* new_node = malloc(sizeof(Node));
        new_node->type = NODE_BIN_OP;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        new_node->next = NULL;
        left = new_node;
    }
    return left;
}

// المستوى 3: عمليات الجمع والطرح (+، -)
Node* parse_additive() {
    Node* left = parse_multiplicative();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_PLUS || parser.current.type == TOKEN_MINUS) {
        OpType op = (parser.current.type == TOKEN_PLUS) ? OP_ADD : OP_SUB;
        eat(parser.current.type);
        Node* right = parse_multiplicative();
        if (!right) return NULL;

        // **Constant Folding Optimization**
        if (left->type == NODE_INT && right->type == NODE_INT) {
            int result = 0;
            if (op == OP_ADD) {
                result = left->data.integer.value + right->data.integer.value;
            } else {
                result = left->data.integer.value - right->data.integer.value;
            }
            left->data.integer.value = result;
            free(right);
            continue;
        }

        Node* new_node = malloc(sizeof(Node));
        new_node->type = NODE_BIN_OP;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        new_node->next = NULL;
        left = new_node;
    }
    return left;
}

// المستوى 4: عمليات المقارنة العلائقية
Node* parse_relational() {
    Node* left = parse_additive();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_LT || parser.current.type == TOKEN_GT ||
           parser.current.type == TOKEN_LTE || parser.current.type == TOKEN_GTE) {
        OpType op;
        if (parser.current.type == TOKEN_LT) op = OP_LT;
        else if (parser.current.type == TOKEN_GT) op = OP_GT;
        else if (parser.current.type == TOKEN_LTE) op = OP_LTE;
        else op = OP_GTE;
        eat(parser.current.type);
        Node* right = parse_additive();
        if (!right) return NULL;

        Node* new_node = malloc(sizeof(Node));
        new_node->type = NODE_BIN_OP;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        new_node->next = NULL;
        left = new_node;
    }
    return left;
}

// المستوى 5: عمليات التساوي
Node* parse_equality() {
    Node* left = parse_relational();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_EQ || parser.current.type == TOKEN_NEQ) {
        OpType op = (parser.current.type == TOKEN_EQ) ? OP_EQ : OP_NEQ;
        eat(parser.current.type);
        Node* right = parse_relational();
        if (!right) return NULL;

        Node* new_node = malloc(sizeof(Node));
        new_node->type = NODE_BIN_OP;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        new_node->next = NULL;
        left = new_node;
    }
    return left;
}

// المستوى 6: العملية المنطقية "و" (&&)
Node* parse_logical_and() {
    Node* left = parse_equality();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_AND) {
        eat(TOKEN_AND);
        Node* right = parse_equality();
        if (!right) return NULL;

        Node* new_node = malloc(sizeof(Node));
        new_node->type = NODE_BIN_OP;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = OP_AND;
        new_node->next = NULL;
        left = new_node;
    }
    return left;
}

// المستوى 7: العملية المنطقية "أو" (||)
Node* parse_logical_or() {
    Node* left = parse_logical_and();
    if (!left) return NULL;

    while (parser.current.type == TOKEN_OR) {
        eat(TOKEN_OR);
        Node* right = parse_logical_and();
        if (!right) return NULL;

        Node* new_node = malloc(sizeof(Node));
        new_node->type = NODE_BIN_OP;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = OP_OR;
        new_node->next = NULL;
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

Node* parse_case() {
    Node* node = malloc(sizeof(Node));
    node->type = NODE_CASE;
    node->next = NULL;

    if (parser.current.type == TOKEN_DEFAULT) {
        eat(TOKEN_DEFAULT);
        eat(TOKEN_COLON);
        node->data.case_stmt.is_default = true;
        node->data.case_stmt.value = NULL;
    } else {
        eat(TOKEN_CASE);
        node->data.case_stmt.value = parse_primary();
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
            synchronize(); // Panic recovery inside switch
        }
    }

    node->data.case_stmt.body = head_stmt;
    return node;
}

Node* parse_switch() {
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
            error_report(parser.current, "Expected 'case' or 'default' inside switch.");
            synchronize();
        }
    }
    eat(TOKEN_RBRACE);

    Node* node = malloc(sizeof(Node));
    node->type = NODE_SWITCH;
    node->data.switch_stmt.expression = expr;
    node->data.switch_stmt.cases = head_case;
    return node;
}

Node* parse_block() {
    eat(TOKEN_LBRACE);
    Node* head = NULL;
    Node* tail = NULL;
    while (parser.current.type != TOKEN_RBRACE && parser.current.type != TOKEN_EOF) {
        Node* stmt = parse_statement();
        if (stmt) {
            if (head == NULL) { head = stmt; tail = stmt; } 
            else { tail->next = stmt; tail = stmt; }
        } else {
            synchronize();
        }
    }
    eat(TOKEN_RBRACE);
    Node* block = malloc(sizeof(Node));
    block->type = NODE_BLOCK;
    block->data.block.statements = head;
    block->next = NULL;
    return block;
}

Node* parse_statement() {
    if (parser.current.type == TOKEN_LBRACE) return parse_block();
    
    Node* stmt = malloc(sizeof(Node));
    stmt->next = NULL;

    if (parser.current.type == TOKEN_SWITCH) {
        free(stmt);
        return parse_switch();
    }

    if (parser.current.type == TOKEN_RETURN) {
        eat(TOKEN_RETURN);
        stmt->type = NODE_RETURN;
        stmt->data.return_stmt.expression = parse_expression();
        eat(TOKEN_DOT);
        return stmt;
    } 
    
    if (parser.current.type == TOKEN_BREAK) {
        eat(TOKEN_BREAK);
        eat(TOKEN_DOT);
        stmt->type = NODE_BREAK;
        return stmt;
    }

    if (parser.current.type == TOKEN_CONTINUE) {
        eat(TOKEN_CONTINUE);
        eat(TOKEN_DOT);
        stmt->type = NODE_CONTINUE;
        return stmt;
    }

    if (parser.current.type == TOKEN_PRINT) {
        eat(TOKEN_PRINT);
        stmt->type = NODE_PRINT;
        stmt->data.print_stmt.expression = parse_expression();
        eat(TOKEN_DOT);
        return stmt;
    }
    
    if (parser.current.type == TOKEN_IF) {
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

        stmt->type = NODE_IF;
        stmt->data.if_stmt.condition = cond;
        stmt->data.if_stmt.then_branch = then_branch;
        stmt->data.if_stmt.else_branch = else_branch;
        return stmt;
    }
    
    if (parser.current.type == TOKEN_WHILE) {
        eat(TOKEN_WHILE);
        eat(TOKEN_LPAREN);
        Node* cond = parse_expression();
        eat(TOKEN_RPAREN);
        Node* body = parse_statement();
        stmt->type = NODE_WHILE;
        stmt->data.while_stmt.condition = cond;
        stmt->data.while_stmt.body = body;
        return stmt;
    }

    if (parser.current.type == TOKEN_FOR) {
        eat(TOKEN_FOR);
        eat(TOKEN_LPAREN);

        Node* init = NULL;
        if (is_type_keyword(parser.current.type)) {
            DataType dt = token_to_datatype(parser.current.type);
            eat(parser.current.type);
            
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);
            eat(TOKEN_ASSIGN);
            Node* expr = parse_expression();
            eat(TOKEN_SEMICOLON);

            init = malloc(sizeof(Node));
            init->type = NODE_VAR_DECL;
            init->data.var_decl.name = name;
            init->data.var_decl.type = dt;
            init->data.var_decl.expression = expr;
            init->data.var_decl.is_global = false;
        } else {
            if (parser.current.type == TOKEN_IDENTIFIER && parser.next.type == TOKEN_ASSIGN) {
                char* name = strdup(parser.current.value);
                eat(TOKEN_IDENTIFIER);
                eat(TOKEN_ASSIGN);
                Node* expr = parse_expression();
                eat(TOKEN_SEMICOLON);

                init = malloc(sizeof(Node));
                init->type = NODE_ASSIGN;
                init->data.assign_stmt.name = name;
                init->data.assign_stmt.expression = expr;
            } else {
                if (parser.current.type != TOKEN_SEMICOLON) {
                    init = parse_expression();
                }
                eat(TOKEN_SEMICOLON);
            }
        }

        Node* cond = parse_expression();
        eat(TOKEN_SEMICOLON);

        Node* incr = NULL;
        if (parser.current.type == TOKEN_IDENTIFIER && parser.next.type == TOKEN_ASSIGN) {
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);
            eat(TOKEN_ASSIGN);
            Node* expr = parse_expression();
            
            incr = malloc(sizeof(Node));
            incr->type = NODE_ASSIGN;
            incr->data.assign_stmt.name = name;
            incr->data.assign_stmt.expression = expr;
        } else {
            incr = parse_expression();
        }
        eat(TOKEN_RPAREN);

        Node* body = parse_statement();

        stmt->type = NODE_FOR;
        stmt->data.for_stmt.init = init;
        stmt->data.for_stmt.condition = cond;
        stmt->data.for_stmt.increment = incr;
        stmt->data.for_stmt.body = body;
        return stmt;
    }
    
    // التحقق من وجود كلمة ثابت (const)
    bool is_const = false;
    if (parser.current.type == TOKEN_CONST) {
        is_const = true;
        eat(TOKEN_CONST);
    }

    if (is_type_keyword(parser.current.type)) {
        DataType dt = token_to_datatype(parser.current.type);
        eat(parser.current.type);
        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        if (parser.current.type == TOKEN_LBRACKET) {
            if (dt != TYPE_INT) { error_report(parser.current, "Only int arrays are supported."); exit(1); }
            eat(TOKEN_LBRACKET);
            int size = 0;
            if (parser.current.type == TOKEN_INT) {
                size = atoi(parser.current.value);
                eat(TOKEN_INT);
            } else {
                error_report(parser.current, "Array size must be integer.");
            }
            eat(TOKEN_RBRACKET);
            eat(TOKEN_DOT);

            stmt->type = NODE_ARRAY_DECL;
            stmt->data.array_decl.name = name;
            stmt->data.array_decl.size = size;
            stmt->data.array_decl.is_global = false;
            stmt->data.array_decl.is_const = is_const;
            return stmt;
        }

        // الثوابت يجب أن يكون لها قيمة ابتدائية
        if (is_const && parser.current.type != TOKEN_ASSIGN) {
            error_report(parser.current, "Constant must be initialized.");
        }

        eat(TOKEN_ASSIGN);
        Node* expr = parse_expression();
        eat(TOKEN_DOT);
        stmt->type = NODE_VAR_DECL;
        stmt->data.var_decl.name = name;
        stmt->data.var_decl.type = dt;
        stmt->data.var_decl.expression = expr;
        stmt->data.var_decl.is_global = false;
        stmt->data.var_decl.is_const = is_const;
        return stmt;
    }
    
    if (parser.current.type == TOKEN_IDENTIFIER) {
        if (parser.next.type == TOKEN_LBRACKET) {
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);
            eat(TOKEN_LBRACKET);
            Node* index = parse_expression();
            eat(TOKEN_RBRACKET);
            
            if (parser.current.type == TOKEN_ASSIGN) {
                eat(TOKEN_ASSIGN);
                Node* value = parse_expression();
                eat(TOKEN_DOT);
                stmt->type = NODE_ARRAY_ASSIGN;
                stmt->data.array_op.name = name;
                stmt->data.array_op.index = index;
                stmt->data.array_op.value = value;
                return stmt;
            }
            else if (parser.current.type == TOKEN_INC || parser.current.type == TOKEN_DEC) {
                Node* access = malloc(sizeof(Node));
                access->type = NODE_ARRAY_ACCESS;
                access->data.array_op.name = name;
                access->data.array_op.index = index;
                access->data.array_op.value = NULL;
                
                UnaryOpType op = (parser.current.type == TOKEN_INC) ? UOP_INC : UOP_DEC;
                eat(parser.current.type);
                eat(TOKEN_DOT);
                
                stmt->type = NODE_POSTFIX_OP;
                stmt->data.unary_op.operand = access;
                stmt->data.unary_op.op = op;
                return stmt;
            }
        }
        else if (parser.next.type == TOKEN_ASSIGN) {
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);
            eat(TOKEN_ASSIGN);
            Node* expr = parse_expression();
            eat(TOKEN_DOT);
            stmt->type = NODE_ASSIGN;
            stmt->data.assign_stmt.name = name;
            stmt->data.assign_stmt.expression = expr;
            return stmt;
        } 
        else if (parser.next.type == TOKEN_LPAREN) {
            Node* expr = parse_expression();
            eat(TOKEN_DOT);
            stmt->type = NODE_CALL_STMT;
            stmt->data.call.name = expr->data.call.name;
            stmt->data.call.args = expr->data.call.args;
            free(expr);
            return stmt;
        }
        else if (parser.next.type == TOKEN_INC || parser.next.type == TOKEN_DEC) {
            Node* expr = parse_expression();
            eat(TOKEN_DOT);
            free(stmt);
            return expr;
        }
    }
    
    error_report(parser.current, "Unexpected token or statement.");
    free(stmt);
    return NULL;
}

Node* parse_declaration() {
    // التحقق من وجود كلمة ثابت (const) للتصريحات العامة
    bool is_const = false;
    if (parser.current.type == TOKEN_CONST) {
        is_const = true;
        eat(TOKEN_CONST);
    }

    if (is_type_keyword(parser.current.type)) {
        DataType dt = token_to_datatype(parser.current.type);
        eat(parser.current.type);
        
        char* name = NULL;
        if (parser.current.type == TOKEN_IDENTIFIER) {
            name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);
        } else {
            error_report(parser.current, "Expected identifier after type.");
            synchronize();
            return NULL;
        }

        if (parser.current.type == TOKEN_LPAREN) {
            // الدوال لا يمكن أن تكون ثوابت
            if (is_const) {
                error_report(parser.current, "Functions cannot be declared as const.");
            }
            eat(TOKEN_LPAREN);
            Node* head_param = NULL;
            Node* tail_param = NULL;
            if (parser.current.type != TOKEN_RPAREN) {
                while (1) {
                    if (!is_type_keyword(parser.current.type)) { 
                        error_report(parser.current, "Expected type for parameter."); 
                    }
                    DataType param_dt = token_to_datatype(parser.current.type);
                    eat(parser.current.type);
                    
                    char* pname = NULL;
                    if (parser.current.type == TOKEN_IDENTIFIER) {
                        pname = strdup(parser.current.value);
                        eat(TOKEN_IDENTIFIER);
                    }

                    Node* param = malloc(sizeof(Node));
                    param->type = NODE_VAR_DECL;
                    param->data.var_decl.name = pname;
                    param->data.var_decl.type = param_dt;
                    param->data.var_decl.expression = NULL;
                    param->data.var_decl.is_global = false;
                    param->next = NULL;
                    if (head_param == NULL) { head_param = param; tail_param = param; }
                    else { tail_param->next = param; tail_param = param; }
                    
                    if (parser.current.type == TOKEN_COMMA) eat(TOKEN_COMMA);
                    else break;
                }
            }
            eat(TOKEN_RPAREN);

            // التحقق مما إذا كان نموذجاً أولياً (ينتهي بنقطة) أو تعريفاً كاملاً (ينتهي بكتلة)
            Node* body = NULL;
            bool is_proto = false;

            if (parser.current.type == TOKEN_DOT) {
                eat(TOKEN_DOT);
                is_proto = true;
            } else {
                body = parse_block();
                is_proto = false;
            }

            Node* func = malloc(sizeof(Node));
            func->type = NODE_FUNC_DEF;
            func->data.func_def.name = name;
            func->data.func_def.return_type = dt;
            func->data.func_def.params = head_param;
            func->data.func_def.body = body;
            func->data.func_def.is_prototype = is_proto;
            func->next = NULL;
            return func;
        }
        else {
            // الثوابت العامة يجب أن يكون لها قيمة ابتدائية
            if (is_const && parser.current.type != TOKEN_ASSIGN) {
                error_report(parser.current, "Constant must be initialized.");
            }

            Node* expr = NULL;
            if (parser.current.type == TOKEN_ASSIGN) { eat(TOKEN_ASSIGN); expr = parse_expression(); }
            eat(TOKEN_DOT);
            Node* var = malloc(sizeof(Node));
            var->type = NODE_VAR_DECL;
            var->data.var_decl.name = name;
            var->data.var_decl.type = dt;
            var->data.var_decl.expression = expr;
            var->data.var_decl.is_global = true;
            var->data.var_decl.is_const = is_const;
            var->next = NULL;
            return var;
        }
    }
    
    error_report(parser.current, "Expected declaration (function or global variable).");
    synchronize();
    return NULL;
}

Node* parse(Lexer* l) {
    init_parser(l);
    
    // تهيئة نظام الأخطاء بمؤشر المصدر للطباعة
    error_init(l->state.source);

    Node* head = NULL;
    Node* tail = NULL;
    while (parser.current.type != TOKEN_EOF) {
        Node* decl = parse_declaration();
        if (decl) {
            if (head == NULL) { head = decl; tail = decl; }
            else { tail->next = decl; tail = decl; }
        }
        // If decl is NULL, synchronize() was likely called inside parse_declaration
    }
    Node* program = malloc(sizeof(Node));
    program->type = NODE_PROGRAM;
    program->data.program.declarations = head;
    return program;
}