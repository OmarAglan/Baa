/**
 * @file parser.c
 * @brief يقوم بتحليل الوحدات اللفظية وبناء شجرة الإعراب المجردة (AST) باستخدام طريقة الانحدار العودي (Recursive Descent).
 * @version 0.1.1 (Phase 3 - For Loop)
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
    parser.next = lexer_next_token(parser.lexer);
}

/**
 * @brief تهيئة المحلل القواعدي ولبدء قراءة أول وحدتين.
 */
void init_parser(Lexer* l) {
    parser.lexer = l;
    parser.current = lexer_next_token(l);
    parser.next = lexer_next_token(l);
}

/**
 * @brief التأكد من أن الوحدة الحالية من نوع معين واستهلاكها، وإلا يتم إظهار خطأ.
 */
void eat(TokenType type) {
    if (parser.current.type == type) {
        advance();
    } else {
        printf("Parser Error: Unexpected token %d (Expected %d) at line %d\n", 
               parser.current.type, type, parser.current.line);
        exit(1);
    }
}

// --- تصريحات مسبقة ---
Node* parse_expression();
Node* parse_statement();
Node* parse_block();

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
        printf("Parser Error: Expected expression at line %d\n", parser.current.line);
        exit(1);
    }

    // التحقق من العمليات اللاحقة (Postfix Operators: ++, --)
    // مثال: س++ أو مصفوفة[0]++
    if (parser.current.type == TOKEN_INC || parser.current.type == TOKEN_DEC) {
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
        else if (parser.current.type == TOKEN_INC) op = UOP_INC; // ++س
        else op = UOP_DEC; // --س

        eat(parser.current.type);
        Node* operand = parse_unary();
        
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
    while (parser.current.type == TOKEN_STAR || parser.current.type == TOKEN_SLASH || parser.current.type == TOKEN_PERCENT) {
        OpType op;
        if (parser.current.type == TOKEN_STAR) op = OP_MUL;
        else if (parser.current.type == TOKEN_SLASH) op = OP_DIV;
        else op = OP_MOD;
        eat(parser.current.type);
        Node* right = parse_unary();
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
    while (parser.current.type == TOKEN_PLUS || parser.current.type == TOKEN_MINUS) {
        OpType op = (parser.current.type == TOKEN_PLUS) ? OP_ADD : OP_SUB;
        eat(parser.current.type);
        Node* right = parse_multiplicative();
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

// المستوى 4: عمليات المقارنة العلائقية (<، >، <=، >=)
Node* parse_relational() {
    Node* left = parse_additive();
    while (parser.current.type == TOKEN_LT || parser.current.type == TOKEN_GT ||
           parser.current.type == TOKEN_LTE || parser.current.type == TOKEN_GTE) {
        OpType op;
        if (parser.current.type == TOKEN_LT) op = OP_LT;
        else if (parser.current.type == TOKEN_GT) op = OP_GT;
        else if (parser.current.type == TOKEN_LTE) op = OP_LTE;
        else op = OP_GTE;
        eat(parser.current.type);
        Node* right = parse_additive();
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

// المستوى 5: عمليات التساوي (==، !=)
Node* parse_equality() {
    Node* left = parse_relational();
    while (parser.current.type == TOKEN_EQ || parser.current.type == TOKEN_NEQ) {
        OpType op = (parser.current.type == TOKEN_EQ) ? OP_EQ : OP_NEQ;
        eat(parser.current.type);
        Node* right = parse_relational();
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
    while (parser.current.type == TOKEN_AND) {
        eat(TOKEN_AND);
        Node* right = parse_equality();
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
    while (parser.current.type == TOKEN_OR) {
        eat(TOKEN_OR);
        Node* right = parse_logical_and();
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

/**
 * @brief تحليل كتلة من الكود { ... }.
 */
Node* parse_block() {
    eat(TOKEN_LBRACE);
    Node* head = NULL;
    Node* tail = NULL;
    while (parser.current.type != TOKEN_RBRACE && parser.current.type != TOKEN_EOF) {
        Node* stmt = parse_statement();
        if (head == NULL) { head = stmt; tail = stmt; } 
        else { tail->next = stmt; tail = stmt; }
    }
    eat(TOKEN_RBRACE);
    Node* block = malloc(sizeof(Node));
    block->type = NODE_BLOCK;
    block->data.block.statements = head;
    block->next = NULL;
    return block;
}

/**
 * @brief تحليل جملة برمجية واحدة.
 */
Node* parse_statement() {
    Node* stmt = malloc(sizeof(Node));
    stmt->next = NULL;

    // جملة الكتلة
    if (parser.current.type == TOKEN_LBRACE) { free(stmt); return parse_block(); }
    
    // جملة الإرجاع (إرجع تعبير.)
    else if (parser.current.type == TOKEN_RETURN) {
        eat(TOKEN_RETURN);
        stmt->type = NODE_RETURN;
        stmt->data.return_stmt.expression = parse_expression();
        eat(TOKEN_DOT);
        return stmt;
    } 
    
    // جملة الطباعة (اطبع تعبير.)
    else if (parser.current.type == TOKEN_PRINT) {
        eat(TOKEN_PRINT);
        stmt->type = NODE_PRINT;
        stmt->data.print_stmt.expression = parse_expression();
        eat(TOKEN_DOT);
        return stmt;
    }
    
    // جملة الشرط (إذا (تعبير) جملة)
    else if (parser.current.type == TOKEN_IF) {
        eat(TOKEN_IF);
        eat(TOKEN_LPAREN);
        Node* cond = parse_expression();
        eat(TOKEN_RPAREN);
        Node* then = parse_statement();
        stmt->type = NODE_IF;
        stmt->data.if_stmt.condition = cond;
        stmt->data.if_stmt.then_branch = then;
        return stmt;
    }
    
    // جملة التكرار (طالما (تعبير) جملة)
    else if (parser.current.type == TOKEN_WHILE) {
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
    // جملة التكرار "لكل" (For Loop)
    // التركيب: لكل ( تهيئة ؛ شرط ؛ زيادة ) جملة
    else if (parser.current.type == TOKEN_FOR) {
        eat(TOKEN_FOR);
        eat(TOKEN_LPAREN);

        // 1. التهيئة (Initialization)
        Node* init = NULL;
        if (parser.current.type == TOKEN_KEYWORD_INT) {
            // تعريف متغير داخل الحلقة (مثل: صحيح س = 0)
            eat(TOKEN_KEYWORD_INT);
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);
            eat(TOKEN_ASSIGN);
            Node* expr = parse_expression();
            eat(TOKEN_SEMICOLON); // الفاصلة المنقوطة العربية

            init = malloc(sizeof(Node));
            init->type = NODE_VAR_DECL;
            init->data.var_decl.name = name;
            init->data.var_decl.expression = expr;
            init->data.var_decl.is_global = false;
        } else {
            // تعيين قيمة (مثل: س = 0) أو تعبير آخر
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
                // تعبير عادي (أو فارغ)
                if (parser.current.type != TOKEN_SEMICOLON) {
                    init = parse_expression();
                }
                eat(TOKEN_SEMICOLON);
            }
        }

        // 2. الشرط (Condition)
        Node* cond = parse_expression();
        eat(TOKEN_SEMICOLON);

        // 3. الزيادة (Increment)
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
            incr = parse_expression(); // مثل س++
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
    
    // تعريف متغير محلي (صحيح س = 5.)
    else if (parser.current.type == TOKEN_KEYWORD_INT) {
        eat(TOKEN_KEYWORD_INT);
        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        // تعريف مصفوفة: صحيح س[5].
        if (parser.current.type == TOKEN_LBRACKET) {
            eat(TOKEN_LBRACKET);
            if (parser.current.type != TOKEN_INT) { printf("Parser Error: Array size must be int\n"); exit(1); }
            int size = atoi(parser.current.value);
            eat(TOKEN_INT);
            eat(TOKEN_RBRACKET);
            eat(TOKEN_DOT);

            stmt->type = NODE_ARRAY_DECL;
            stmt->data.array_decl.name = name;
            stmt->data.array_decl.size = size;
            stmt->data.array_decl.is_global = false;
            return stmt;
        }

        // تعريف متغير عادي
        eat(TOKEN_ASSIGN);
        Node* expr = parse_expression();
        eat(TOKEN_DOT);
        stmt->type = NODE_VAR_DECL;
        stmt->data.var_decl.name = name;
        stmt->data.var_decl.expression = expr;
        stmt->data.var_decl.is_global = false;
        return stmt;
    }
    
    // جملة التعيين أو استدعاء الدالة
    else if (parser.current.type == TOKEN_IDENTIFIER) {
        // تعيين مصفوفة: س[0] = 5.
        if (parser.next.type == TOKEN_LBRACKET) {
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);
            eat(TOKEN_LBRACKET);
            Node* index = parse_expression();
            eat(TOKEN_RBRACKET);
            // قد يكون تعبيراً (وصول للمصفوفة) متبوعاً بعملية لاحقة مثل ++
            // ولكن هنا نحن نتوقع جملة كاملة، غالباً تعيين أو عملية بمؤثر جانبي
            // إذا كان التالي هو =
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
            // إذا كان التالي ++ أو -- (س[0]++.)
            else if (parser.current.type == TOKEN_INC || parser.current.type == TOKEN_DEC) {
                // نعيد بناء عقدة الوصول للمصفوفة
                Node* access = malloc(sizeof(Node));
                access->type = NODE_ARRAY_ACCESS;
                access->data.array_op.name = name;
                access->data.array_op.index = index;
                access->data.array_op.value = NULL;
                
                UnaryOpType op = (parser.current.type == TOKEN_INC) ? UOP_INC : UOP_DEC;
                eat(parser.current.type);
                eat(TOKEN_DOT);
                
                // نعيدها كعقدة عملية لاحقة، المولد سيتعامل معها كجملة
                stmt->type = NODE_POSTFIX_OP;
                stmt->data.unary_op.operand = access;
                stmt->data.unary_op.op = op;
                return stmt;
            }
        }
        // تعيين متغير: س = 5.
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
        // استدعاء دالة: س().
        else if (parser.next.type == TOKEN_LPAREN) {
            Node* expr = parse_expression();
            eat(TOKEN_DOT);
            stmt->type = NODE_CALL_STMT;
            stmt->data.call.name = expr->data.call.name;
            stmt->data.call.args = expr->data.call.args;
            free(expr);
            return stmt;
        }
        // جملة تعبيرية (مثل س++.)
        else if (parser.next.type == TOKEN_INC || parser.next.type == TOKEN_DEC) {
            Node* expr = parse_expression();
            eat(TOKEN_DOT);
            // يمكننا إرجاع التعبير مباشرة، والمولد سيتعامل معه
            // ولكن للحفاظ على الاتساق، العقدة التي تم إرجاعها هي NODE_POSTFIX_OP
            // وهي نوع صالح من الجمل (تعبير له أثر جانبي)
            free(stmt); // التخلص من العقدة الفارغة
            return expr;
        }
    }
    printf("Parser Error: Unknown statement at line %d\n", parser.current.line);
    exit(1);
}

/**
 * @brief تحليل التصريحات ذات المستوى الأعلى (دوال أو متغيرات عامة).
 */
Node* parse_declaration() {
    if (parser.current.type == TOKEN_KEYWORD_INT) {
        eat(TOKEN_KEYWORD_INT);
        if (parser.current.type != TOKEN_IDENTIFIER) { printf("Parser Error: Expected Identifier\n"); exit(1); }
        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        // إذا وجد قوس، فهو تعريف دالة
        if (parser.current.type == TOKEN_LPAREN) {
            eat(TOKEN_LPAREN);
            Node* head_param = NULL;
            Node* tail_param = NULL;
            if (parser.current.type != TOKEN_RPAREN) {
                while (1) {
                    eat(TOKEN_KEYWORD_INT);
                    char* pname = strdup(parser.current.value);
                    eat(TOKEN_IDENTIFIER);
                    Node* param = malloc(sizeof(Node));
                    param->type = NODE_VAR_DECL;
                    param->data.var_decl.name = pname;
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
            Node* body = parse_block();
            Node* func = malloc(sizeof(Node));
            func->type = NODE_FUNC_DEF;
            func->data.func_def.name = name;
            func->data.func_def.params = head_param;
            func->data.func_def.body = body;
            func->next = NULL;
            return func;
        }
        // وإلا، فهو تعريف متغير عام
        else {
            Node* expr = NULL;
            if (parser.current.type == TOKEN_ASSIGN) { eat(TOKEN_ASSIGN); expr = parse_expression(); }
            eat(TOKEN_DOT);
            Node* var = malloc(sizeof(Node));
            var->type = NODE_VAR_DECL;
            var->data.var_decl.name = name;
            var->data.var_decl.expression = expr;
            var->data.var_decl.is_global = true;
            var->next = NULL;
            return var;
        }
    }
    printf("Parser Error: Unexpected token at Top Level\n");
    exit(1);
}

/**
 * @brief تحليل نص المصدر بالكامل وبناء الشجرة.
 */
Node* parse(Lexer* l) {
    init_parser(l);
    Node* head = NULL;
    Node* tail = NULL;
    while (parser.current.type != TOKEN_EOF) {
        Node* decl = parse_declaration();
        if (head == NULL) { head = decl; tail = decl; }
        else { tail->next = decl; tail = decl; }
    }
    Node* program = malloc(sizeof(Node));
    program->type = NODE_PROGRAM;
    program->data.program.declarations = head;
    return program;
}