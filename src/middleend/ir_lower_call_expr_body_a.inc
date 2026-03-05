    if (!ctx || !ctx->builder || !expr) return ir_builder_const_i64(0);

    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);

    bool allow_format_builtins = true;
    if (expr->data.call.name) {
        const char* n0 = expr->data.call.name;
        if (find_local(ctx, n0)) allow_format_builtins = false;
        if (m && ir_module_find_global(m, n0)) allow_format_builtins = false;
        if (m) {
            IRFunc* f0 = ir_module_find_func(m, n0);
            if (f0 && !f0->is_prototype) allow_format_builtins = false;
        }
    }

    if (allow_format_builtins && expr->data.call.name) {
        const char* n = expr->data.call.name;
        if (strcmp(n, "اطبع_منسق") == 0) {
            return ir_lower_builtin_print_format_ar(ctx, expr);
        }
        if (strcmp(n, "نسق") == 0) {
            return ir_lower_builtin_format_string_ar(ctx, expr);
        }
        if (strcmp(n, "اقرأ_منسق") == 0) {
            return ir_lower_builtin_scan_format_ar(ctx, expr);
        }
        if (strcmp(n, "اقرأ_سطر") == 0) {
            // اقرأ_سطر() من stdin فقط. اقرأ_سطر(ملف) يبقى مسار ملفات (builtins v0.3.12).
            if (!expr->data.call.args) {
                return ir_lower_builtin_read_line_stdin_ar(ctx, expr);
            }
        }
        if (strcmp(n, "اقرأ_رقم") == 0) {
            return ir_lower_builtin_read_num_stdin_ar(ctx, expr);
        }
        if (strcmp(n, "بدء_معاملات") == 0) {
            return ir_lower_builtin_variadic_start(ctx, expr);
        }
        if (strcmp(n, "معامل_تالي") == 0) {
            return ir_lower_builtin_variadic_next(ctx, expr);
        }
        if (strcmp(n, "نهاية_معاملات") == 0) {
            return ir_lower_builtin_variadic_end(ctx, expr);
        }
        if (strcmp(n, "تأكد") == 0) {
            return ir_lower_builtin_assert(ctx, expr);
        }
        if (strcmp(n, "توقف_فوري") == 0) {
            return ir_lower_builtin_panic(ctx, expr);
        }
        if (strcmp(n, "كود_خطأ_النظام") == 0) {
            return ir_lower_builtin_errno_get(ctx, expr);
        }
        if (strcmp(n, "ضبط_كود_خطأ_النظام") == 0) {
            return ir_lower_builtin_errno_set(ctx, expr);
        }
        if (strcmp(n, "نص_كود_خطأ") == 0) {
            return ir_lower_builtin_errno_text(ctx, expr);
        }
    }

    // دوال السلاسل المدمجة في v0.3.9 (سلوك C-like مع أسماء عربية).
    bool allow_string_builtins = true;
    if (expr->data.call.name) {
        const char* n0 = expr->data.call.name;
        // إذا كان هناك رمز/تخزين يحمل نفس الاسم، يجب احترام shadowing وعدم تحويله إلى builtin.
        if (find_local(ctx, n0)) {
            allow_string_builtins = false;
        }
        if (m && ir_module_find_global(m, n0)) {
            allow_string_builtins = false;
        }
        if (m) {
            IRFunc* f0 = ir_module_find_func(m, n0);
            // إذا كانت الدالة مجرد prototype (مثلاً من ترويسة stdlib)، نسمح للـ builtin أن يطغى عليها.
            // أما إذا كانت لها جسم داخل الوحدة، فهذه دالة مستخدم ويجب احترامها.
            if (f0 && !f0->is_prototype) {
                allow_string_builtins = false;
            }
        }
    }

