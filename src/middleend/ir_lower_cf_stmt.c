static IRValue* lower_condition_i1(IRLowerCtx* ctx, Node* cond_expr) {
    // Ensure the condition is i1 for br_cond.
    return lower_to_bool(ctx, lower_expr(ctx, cond_expr));
}

static void lower_if_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRBlock* then_bb = cf_create_block(ctx, "فرع_صواب");
    IRBlock* merge_bb = cf_create_block(ctx, "دمج");
    IRBlock* else_bb = NULL;

    if (stmt->data.if_stmt.else_branch) {
        else_bb = cf_create_block(ctx, "فرع_خطأ");
    }

    IRValue* cond = lower_condition_i1(ctx, stmt->data.if_stmt.condition);

    if (!ir_builder_is_block_terminated(ctx->builder)) {
        if (else_bb) {
            ir_builder_emit_br_cond(ctx->builder, cond, then_bb, else_bb);
        } else {
            ir_builder_emit_br_cond(ctx->builder, cond, then_bb, merge_bb);
        }
    }

    // then
    ir_builder_set_insert_point(ctx->builder, then_bb);
    ir_lower_scope_push(ctx);
    lower_stmt(ctx, stmt->data.if_stmt.then_branch);
    ir_lower_scope_pop(ctx);
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, merge_bb);
    }

    // else
    if (else_bb) {
        ir_builder_set_insert_point(ctx->builder, else_bb);
        ir_lower_scope_push(ctx);
        lower_stmt(ctx, stmt->data.if_stmt.else_branch);
        ir_lower_scope_pop(ctx);
        if (!ir_builder_is_block_terminated(ctx->builder)) {
            ir_builder_emit_br(ctx->builder, merge_bb);
        }
    }

    // merge
    ir_builder_set_insert_point(ctx->builder, merge_bb);
}

static void lower_while_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRBlock* header_bb = cf_create_block(ctx, "حلقة_تحقق");
    IRBlock* body_bb = cf_create_block(ctx, "حلقة_جسم");
    IRBlock* exit_bb = cf_create_block(ctx, "حلقة_نهاية");

    // Jump to header from current block
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, header_bb);
    }

    // header: evaluate condition
    ir_builder_set_insert_point(ctx->builder, header_bb);
    IRValue* cond = lower_condition_i1(ctx, stmt->data.while_stmt.condition);
    ir_builder_emit_br_cond(ctx->builder, cond, body_bb, exit_bb);

    // body
    cf_push(ctx, exit_bb, header_bb); // break -> exit, continue -> header
    ir_builder_set_insert_point(ctx->builder, body_bb);
    ir_lower_scope_push(ctx);
    lower_stmt(ctx, stmt->data.while_stmt.body);
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, header_bb);
    }
    ir_lower_scope_pop(ctx);
    cf_pop(ctx);

    // continue after loop
    ir_builder_set_insert_point(ctx->builder, exit_bb);
}

static void lower_for_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    // نطاق حلقة for: يمنع تسرب متغيرات init خارج الحلقة.
    ir_lower_scope_push(ctx);

    // Init runs in the current block
    if (stmt->data.for_stmt.init) {
        Node* init = stmt->data.for_stmt.init;
        if (init->type == NODE_VAR_DECL || init->type == NODE_ASSIGN || init->type == NODE_BLOCK) {
            lower_stmt(ctx, init);
        } else {
            (void)lower_expr(ctx, init);
        }
    }

    IRBlock* header_bb = cf_create_block(ctx, "لكل_تحقق");
    IRBlock* body_bb = cf_create_block(ctx, "لكل_جسم");
    IRBlock* inc_bb = cf_create_block(ctx, "لكل_زيادة");
    IRBlock* exit_bb = cf_create_block(ctx, "لكل_نهاية");

    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, header_bb);
    }

    // header
    ir_builder_set_insert_point(ctx->builder, header_bb);
    if (stmt->data.for_stmt.condition) {
        IRValue* cond = lower_condition_i1(ctx, stmt->data.for_stmt.condition);
        ir_builder_emit_br_cond(ctx->builder, cond, body_bb, exit_bb);
    } else {
        ir_builder_emit_br(ctx->builder, body_bb);
    }

    // body
    cf_push(ctx, exit_bb, inc_bb); // break -> exit, continue -> increment
    ir_builder_set_insert_point(ctx->builder, body_bb);
    lower_stmt(ctx, stmt->data.for_stmt.body);
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, inc_bb);
    }
    cf_pop(ctx);

    // increment
    ir_builder_set_insert_point(ctx->builder, inc_bb);
    if (stmt->data.for_stmt.increment) {
        Node* inc = stmt->data.for_stmt.increment;
        if (inc->type == NODE_ASSIGN || inc->type == NODE_VAR_DECL || inc->type == NODE_BLOCK) {
            lower_stmt(ctx, inc);
        } else {
            (void)lower_expr(ctx, inc);
        }
    }
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, header_bb);
    }

    ir_builder_set_insert_point(ctx->builder, exit_bb);

    ir_lower_scope_pop(ctx);
}

static void lower_break_stmt(IRLowerCtx* ctx) {
    if (!ctx || !ctx->builder) return;
    IRBlock* target = cf_current_break(ctx);
    if (!target) {
        ir_lower_report_error(ctx, NULL, "استعمال 'break' خارج حلقة/اختر.");
        return;
    }
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, target);
    }
}

static void lower_continue_stmt(IRLowerCtx* ctx) {
    if (!ctx || !ctx->builder) return;
    IRBlock* target = cf_current_continue(ctx);
    if (!target) {
        ir_lower_report_error(ctx, NULL, "استعمال 'continue' خارج حلقة.");
        return;
    }
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, target);
    }
}

static void lower_switch_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    // Gather cases
    int case_count = 0;
    int non_default_count = 0;
    Node* c = stmt->data.switch_stmt.cases;
    while (c) {
        case_count++;
        if (!c->data.case_stmt.is_default) non_default_count++;
        c = c->next;
    }

    IRBlock* end_bb = cf_create_block(ctx, "نهاية_اختر");

    // Switch doesn't define a continue target (inherits from outer loop, if any)
    cf_push(ctx, end_bb, cf_current_continue(ctx));

    IRBlock** case_blocks = NULL;
    Node** case_nodes = NULL;
    IRBlock** non_default_blocks = NULL;
    Node** non_default_nodes = NULL;
    IRBlock* default_bb = NULL;
    Node* default_node = NULL;

    if (case_count > 0) {
        case_blocks = (IRBlock**)malloc(sizeof(IRBlock*) * (size_t)case_count);
        case_nodes = (Node**)malloc(sizeof(Node*) * (size_t)case_count);
    }
    if (non_default_count > 0) {
        non_default_blocks = (IRBlock**)malloc(sizeof(IRBlock*) * (size_t)non_default_count);
        non_default_nodes = (Node**)malloc(sizeof(Node*) * (size_t)non_default_count);
    }

    // Create blocks for all cases in source order (for fallthrough)
    int idx = 0;
    int nd_idx = 0;
    c = stmt->data.switch_stmt.cases;
    while (c) {
        IRBlock* bb = NULL;
        if (c->data.case_stmt.is_default) {
            bb = cf_create_block(ctx, "افتراضي");
            default_bb = bb;
            default_node = c;
        } else {
            bb = cf_create_block(ctx, "حالة");
            if (non_default_blocks && non_default_nodes) {
                non_default_blocks[nd_idx] = bb;
                non_default_nodes[nd_idx] = c;
                nd_idx++;
            }
        }

        if (case_blocks && case_nodes) {
            case_blocks[idx] = bb;
            case_nodes[idx] = c;
            idx++;
        }

        c = c->next;
    }

    // Evaluate switch expression (should be integer per semantic rules)
    IRValue* sw_val = lower_expr(ctx, stmt->data.switch_stmt.expression);
    IRType* sw_type = sw_val && sw_val->type ? sw_val->type : IR_TYPE_I64_T;

    int sw_is_reg = sw_val && sw_val->kind == IR_VAL_REG;
    int sw_reg = sw_is_reg ? sw_val->data.reg_num : -1;
    int64_t sw_const = (!sw_is_reg && sw_val && sw_val->kind == IR_VAL_CONST_INT) ? sw_val->data.const_int : 0;

    // Dispatch comparisons (chain)
    if (non_default_count == 0) {
        // No explicit cases: jump to default or end.
        if (!ir_builder_is_block_terminated(ctx->builder)) {
            ir_builder_emit_br(ctx->builder, default_bb ? default_bb : end_bb);
        }
    } else {
        IRBlock* current_check = ir_builder_get_insert_block(ctx->builder);

        for (int i = 0; i < non_default_count; i++) {
            Node* case_node = non_default_nodes[i];
            IRBlock* true_bb = non_default_blocks[i];
            IRBlock* false_bb = NULL;

            if (i == non_default_count - 1) {
                false_bb = default_bb ? default_bb : end_bb;
            } else {
                false_bb = cf_create_block(ctx, "فحص");
            }

            // Compare sw == case_value
            IRValue* lhs = sw_is_reg ? ir_value_reg(sw_reg, sw_type) : ir_value_const_int(sw_const, sw_type);
            IRValue* rhs = lower_expr(ctx, case_node->data.case_stmt.value);
            int cmp_reg = ir_builder_emit_cmp_eq(ctx->builder, lhs, rhs);
            IRValue* cmp_val = ir_value_reg(cmp_reg, IR_TYPE_I1_T);

            ir_builder_emit_br_cond(ctx->builder, cmp_val, true_bb, false_bb);

            // Continue chain in the false block (only if it's a check block we created)
            if (i < non_default_count - 1) {
                ir_builder_set_insert_point(ctx->builder, false_bb);
                current_check = false_bb;
            } else {
                (void)current_check;
            }
        }
    }

    // Emit case bodies with fallthrough semantics (explicit br to next case block)
    for (int i = 0; i < case_count; i++) {
        IRBlock* bb = case_blocks[i];
        Node* case_node = case_nodes[i];

        ir_builder_set_insert_point(ctx->builder, bb);
        ir_lower_scope_push(ctx);
        lower_stmt_list(ctx, case_node->data.case_stmt.body);
        ir_lower_scope_pop(ctx);

        if (!ir_builder_is_block_terminated(ctx->builder)) {
            IRBlock* next_bb = (i + 1 < case_count) ? case_blocks[i + 1] : end_bb;
            ir_builder_emit_br(ctx->builder, next_bb);
        }
    }

    // Continue after switch
    ir_builder_set_insert_point(ctx->builder, end_bb);

    if (case_blocks) free(case_blocks);
    if (case_nodes) free(case_nodes);
    if (non_default_blocks) free(non_default_blocks);
    if (non_default_nodes) free(non_default_nodes);
    (void)default_node;

    cf_pop(ctx); // pop switch break target
}

void lower_stmt_list(IRLowerCtx* ctx, Node* first_stmt) {
    Node* s = first_stmt;
    while (s) {
        lower_stmt(ctx, s);
        s = s->next;
    }
}

void lower_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    // افتراضي: اربط أي تعليمات صادرة من هذه الجملة بموقعها.
    ir_lower_set_loc(ctx->builder, stmt);

    switch (stmt->type) {
        case NODE_BLOCK:
            ir_lower_scope_push(ctx);
            lower_stmt_list(ctx, stmt->data.block.statements);
            ir_lower_scope_pop(ctx);
            return;

        case NODE_VAR_DECL:
            lower_var_decl(ctx, stmt);
            return;

        case NODE_ARRAY_DECL:
            lower_array_decl(ctx, stmt);
            return;

        case NODE_ASSIGN:
            lower_assign(ctx, stmt);
            return;

        case NODE_MEMBER_ASSIGN:
            lower_member_assign(ctx, stmt);
            return;

        case NODE_DEREF_ASSIGN:
            lower_deref_assign(ctx, stmt);
            return;

        case NODE_ARRAY_ASSIGN:
            lower_array_assign(ctx, stmt);
            return;

        case NODE_RETURN:
            lower_return(ctx, stmt);
            return;

        case NODE_PRINT:
            lower_print(ctx, stmt);
            return;

        case NODE_READ:
            lower_read(ctx, stmt);
            return;

        case NODE_INLINE_ASM:
            lower_inline_asm_stmt(ctx, stmt);
            return;

        case NODE_IF:
            lower_if_stmt(ctx, stmt);
            return;

        case NODE_WHILE:
            lower_while_stmt(ctx, stmt);
            return;

        case NODE_FOR:
            lower_for_stmt(ctx, stmt);
            return;

        case NODE_SWITCH:
            lower_switch_stmt(ctx, stmt);
            return;

        case NODE_BREAK:
            lower_break_stmt(ctx);
            return;

        case NODE_CONTINUE:
            lower_continue_stmt(ctx);
            return;

        case NODE_CALL_STMT: {
            // Lower call statement via expression lowering.
            // Construct a temporary NODE_CALL_EXPR-compatible view.
            Node temp;
            memset(&temp, 0, sizeof(temp));
            temp.type = NODE_CALL_EXPR;
            temp.data.call = stmt->data.call;
            temp.filename = stmt->filename;
            temp.line = stmt->line;
            temp.col = stmt->col;
            (void)lower_expr(ctx, &temp);
            return;
        }

        default:
            ir_lower_report_error(ctx, stmt, "عقدة جملة غير مدعومة (%d).", (int)stmt->type);
            return;
    }
}

