/**
 * @file analysis.c
 * @brief وحدة التحليل الدلالي (Semantic Analysis).
 * @details تقوم هذه الوحدة بالتحقق من صحة البرنامج قبل توليد الكود، بما في ذلك:
 *          - التحقق من الأنواع (Type Checking)
 *          - التحقق من النطاقات (Scope Analysis)
 *          - التحقق من الثوابت (Const Checking)
 *          - اكتشاف المتغيرات غير المستخدمة (Unused Variables)
 *          - اكتشاف الكود الميت (Dead Code)
 * @version 0.2.9
 */

#include "baa.h"

// ============================================================================
// جداول الرموز (Symbol Tables)
// ============================================================================

// نستخدم نفس الهيكلية البسيطة المستخدمة سابقاً في codegen لضمان التوافق
// في المستقبل، سيتم استبدال هذا بجداول تجزئة (Hash Maps) لدعم النطاقات المتداخلة بشكل أفضل

static Symbol global_symbols[100];
static int global_count = 0;

static Symbol local_symbols[100];
static int local_count = 0;

// مكدس النطاقات المحلية: نخزن قيمة local_count عند دخول نطاق جديد.
static int scope_stack[64];
static int scope_depth = 0;

static bool has_error = false;
static bool inside_loop = false;
static bool inside_switch = false;

// معلومات الملف الحالي (للتحذيرات)
static const char* current_filename = NULL;

// ============================================================================
// دوال مساعدة (Helper Functions)
// ============================================================================

/**
 * @brief الإبلاغ عن خطأ دلالي.
 * @note نستخدم وحدة (Token) فارغة لأن العقد لا تحتوي حالياً على معلومات السطر.
 */
static void semantic_error(const char* message, ...) {
    has_error = true;
    fprintf(stderr, "[Semantic Error] ");
    
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

/**
 * @brief إعادة تعيين جداول الرموز.
 */
static void reset_analysis() {
    global_count = 0;
    local_count = 0;
    scope_depth = 0;
    has_error = false;
    inside_loop = false;
    inside_switch = false;
}

// ============================================================================
// إدارة النطاقات المحلية (Local Scope Management)
// ============================================================================

static void check_unused_local_variables_range(int start, int end);

static int current_scope_start(void) {
    if (scope_depth <= 0) return 0;
    return scope_stack[scope_depth - 1];
}

static void scope_push(void) {
    if (scope_depth >= (int)(sizeof(scope_stack) / sizeof(scope_stack[0]))) {
        semantic_error("Too many nested scopes.");
        return;
    }
    scope_stack[scope_depth++] = local_count;
}

static void scope_pop(void) {
    if (scope_depth <= 0) return;
    int start = scope_stack[--scope_depth];

    // تحذيرات المتغيرات غير المستخدمة لهذا النطاق فقط
    check_unused_local_variables_range(start, local_count);

    // إخراج رموز النطاق من الجدول (منطقيًا)
    local_count = start;
}

/**
 * @brief إضافة رمز إلى النطاق الحالي.
 */
static void add_symbol(const char* name, ScopeType scope, DataType type, bool is_const,
                       int decl_line, int decl_col, const char* decl_file) {
    if (!name) {
        semantic_error("اسم الرمز فارغ.");
        return;
    }

    size_t name_len = strlen(name);
    size_t name_cap = sizeof(global_symbols[0].name);
    if (name_len >= name_cap) {
        semantic_error("اسم الرمز طويل جداً: '%s' (الحد الأقصى %zu حرفاً).", name, name_cap - 1);
        return;
    }

    if (scope == SCOPE_GLOBAL) {
        // التحقق من التكرار
        for (int i = 0; i < global_count; i++) {
            if (strcmp(global_symbols[i].name, name) == 0) {
                semantic_error("Redefinition of global variable '%s'.", name);
                return;
            }
        }
        if (global_count >= 100) { semantic_error("Too many global variables."); return; }
        
        memcpy(global_symbols[global_count].name, name, name_len + 1);
        global_symbols[global_count].scope = SCOPE_GLOBAL;
        global_symbols[global_count].type = type;
        global_symbols[global_count].is_const = is_const;
        global_symbols[global_count].is_used = false;
        global_symbols[global_count].decl_line = decl_line;
        global_symbols[global_count].decl_col = decl_col;
        global_symbols[global_count].decl_file = decl_file;
        global_count++;
    } else {
        // التحقق من التكرار داخل النطاق الحالي فقط
        int start = current_scope_start();
        for (int i = start; i < local_count; i++) {
            if (strcmp(local_symbols[i].name, name) == 0) {
                semantic_error("Redefinition of local variable '%s'.", name);
                return;
            }
        }
        if (local_count >= 100) { semantic_error("Too many local variables."); return; }

        // تحذير إذا كان المتغير المحلي يحجب متغيراً عاماً
        for (int i = 0; i < global_count; i++) {
            if (strcmp(global_symbols[i].name, name) == 0) {
                warning_report(WARN_SHADOW_VARIABLE, decl_file, decl_line, decl_col,
                    "Local variable '%s' shadows global variable.", name);
                break;
            }
        }

        // تحذير إذا كان المتغير المحلي يحجب متغيراً محلياً خارجياً
        for (int i = 0; i < start; i++) {
            if (strcmp(local_symbols[i].name, name) == 0) {
                warning_report(WARN_SHADOW_VARIABLE, decl_file, decl_line, decl_col,
                    "Local variable '%s' shadows outer local variable.", name);
                break;
            }
        }

        memcpy(local_symbols[local_count].name, name, name_len + 1);
        local_symbols[local_count].scope = SCOPE_LOCAL;
        local_symbols[local_count].type = type;
        local_symbols[local_count].is_const = is_const;
        local_symbols[local_count].is_used = false;
        local_symbols[local_count].decl_line = decl_line;
        local_symbols[local_count].decl_col = decl_col;
        local_symbols[local_count].decl_file = decl_file;
        local_count++;
    }
}

/**
 * @brief البحث عن رمز بالاسم.
 * @param mark_used إذا كان true، يتم تعليم المتغير كمستخدم.
 */
static Symbol* lookup(const char* name, bool mark_used) {
    // البحث في المحلي أولاً (من الأحدث للأقدم لدعم النطاقات المتداخلة)
    for (int i = local_count - 1; i >= 0; i--) {
        if (strcmp(local_symbols[i].name, name) == 0) {
            if (mark_used) local_symbols[i].is_used = true;
            return &local_symbols[i];
        }
    }
    // البحث في العام
    for (int i = 0; i < global_count; i++) {
        if (strcmp(global_symbols[i].name, name) == 0) {
            if (mark_used) global_symbols[i].is_used = true;
            return &global_symbols[i];
        }
    }
    return NULL;
}

/**
 * @brief التحقق من المتغيرات غير المستخدمة في النطاق المحلي.
 */
static void check_unused_local_variables_range(int start, int end) {
    if (start < 0) start = 0;
    if (end < start) end = start;
    if (end > local_count) end = local_count;

    for (int i = start; i < end; i++) {
        if (!local_symbols[i].is_used) {
            warning_report(WARN_UNUSED_VARIABLE,
                local_symbols[i].decl_file,
                local_symbols[i].decl_line,
                local_symbols[i].decl_col,
                "Variable '%s' is declared but never used.",
                local_symbols[i].name);
        }
    }
}

// ملاحظة: تحذيرات غير المستخدم تُطلق عبر scope_pop() لكل نطاق.

/**
 * @brief التحقق من المتغيرات العامة غير المستخدمة.
 */
static void check_unused_global_variables(void) {
    for (int i = 0; i < global_count; i++) {
        if (!global_symbols[i].is_used) {
            warning_report(WARN_UNUSED_VARIABLE,
                global_symbols[i].decl_file,
                global_symbols[i].decl_line,
                global_symbols[i].decl_col,
                "Global variable '%s' is declared but never used.",
                global_symbols[i].name);
        }
    }
}

// ============================================================================
// دوال التحليل (Analysis Functions)
// ============================================================================

// تصريح مسبق للدالة العودية
static void analyze_node(Node* node);

/**
 * @brief تحويل نوع البيانات إلى نص.
 */
static const char* datatype_to_str(DataType type) {
    switch (type) {
        case TYPE_INT: return "INTEGER";
        case TYPE_STRING: return "STRING";
        case TYPE_BOOL: return "BOOLEAN";
        default: return "UNKNOWN";
    }
}

/**
 * @brief استنتاج نوع التعبير.
 * @return DataType نوع التعبير الناتج.
 */
static DataType infer_type(Node* node) {
    if (!node) return TYPE_INT; // افتراضي لتجنب الانهيار

    switch (node->type) {
        case NODE_INT:
        case NODE_CHAR:
            return TYPE_INT;
        
        case NODE_STRING:
            return TYPE_STRING;
        
        case NODE_BOOL:
            return TYPE_BOOL;
            
        case NODE_VAR_REF: {
            Symbol* sym = lookup(node->data.var_ref.name, true); // تعليم كمستخدم
            if (!sym) {
                semantic_error("Undefined variable '%s'.", node->data.var_ref.name);
                return TYPE_INT; // استرداد الخطأ
            }
            return sym->type;
        }

        case NODE_ARRAY_ACCESS: {
            // حالياً المصفوفات تدعم الأعداد الصحيحة فقط
            Symbol* sym = lookup(node->data.array_op.name, true); // تعليم كمستخدم
            if (!sym) {
                semantic_error("Undefined array '%s'.", node->data.array_op.name);
            }
            // تحقق من أن الفهرس رقمي
            if (infer_type(node->data.array_op.index) != TYPE_INT) {
                semantic_error("Array index must be an integer.");
            }
            return TYPE_INT;
        }

        case NODE_CALL_EXPR:
            // في الوقت الحالي نفترض أن جميع الدوال تعيد صحيح (INT)
            // TODO: دعم دوال ترجع نصوصاً عند تحديث تعريف الدوال
            return TYPE_INT;

        case NODE_BIN_OP: {
            DataType left = infer_type(node->data.bin_op.left);
            DataType right = infer_type(node->data.bin_op.right);
            
            // العمليات الحسابية تتطلب أعداداً صحيحة
            OpType op = node->data.bin_op.op;
            if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV || op == OP_MOD) {
                if (left != TYPE_INT || right != TYPE_INT) {
                    semantic_error("Arithmetic operations require INTEGER operands.");
                }
                return TYPE_INT;
            }
            
            // عمليات المقارنة تتطلب أنواعاً متوافقة وتعيد منطقي
            if (op == OP_EQ || op == OP_NEQ || op == OP_LT || op == OP_GT || op == OP_LTE || op == OP_GTE) {
                if (left != right) {
                    semantic_error("Comparison operations require matching types.");
                }
                return TYPE_BOOL;
            }
            
            // العمليات المنطقية (AND/OR) تتطلب منطقي أو صحيح وتعيد منطقي
            if (op == OP_AND || op == OP_OR) {
                // نقبل INT أو BOOL لأن C تعامل القيم غير الصفرية كـ true
                if ((left != TYPE_INT && left != TYPE_BOOL) || (right != TYPE_INT && right != TYPE_BOOL)) {
                    semantic_error("Logical operations require INTEGER or BOOLEAN operands.");
                }
                return TYPE_BOOL;
            }
            
            return TYPE_INT;
        }

        case NODE_UNARY_OP:
        case NODE_POSTFIX_OP: {
            if (infer_type(node->data.unary_op.operand) != TYPE_INT) {
                semantic_error("Unary operations require INTEGER operand.");
            }
            return TYPE_INT;
        }

        default:
            return TYPE_INT;
    }
}

/**
 * @brief التحقق مما إذا كانت الجملة توقف التدفق (return/break/continue).
 */
static bool is_terminating_statement(Node* node) {
    if (!node) return false;
    return (node->type == NODE_RETURN ||
            node->type == NODE_BREAK ||
            node->type == NODE_CONTINUE);
}

/**
 * @brief تحليل كتلة من الجمل مع اكتشاف الكود الميت.
 * @param statements قائمة الجمل
 * @param context سياق الكتلة (للرسائل)
 */
static void analyze_statements_with_dead_code_check(Node* statements, const char* context) {
    bool found_terminator = false;
    int terminator_line = 0;
    
    Node* stmt = statements;
    while (stmt) {
        if (found_terminator) {
            // كود ميت بعد return/break/continue
            warning_report(WARN_DEAD_CODE, current_filename, terminator_line, 1,
                "Unreachable code after '%s' statement.", context);
            // نستمر في التحليل لاكتشاف أخطاء أخرى
            found_terminator = false; // تجنب تحذيرات متعددة
        }
        
        analyze_node(stmt);
        
        if (is_terminating_statement(stmt)) {
            found_terminator = true;
            // TODO: الحصول على رقم السطر الفعلي من العقدة
            terminator_line = 0; // سيظهر كسطر 0 مؤقتاً
        }
        
        stmt = stmt->next;
    }
}

static void analyze_node(Node* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM: {
            Node* decl = node->data.program.declarations;
            while (decl) {
                analyze_node(decl);
                decl = decl->next;
            }
            // في نهاية البرنامج، التحقق من المتغيرات العامة غير المستخدمة
            check_unused_global_variables();
            break;
        }

        case NODE_VAR_DECL: {
            // 1. التحقق من نوع التعبير المخصص (إن وجد)
            if (node->data.var_decl.expression) {
                DataType exprType = infer_type(node->data.var_decl.expression);
                DataType declType = node->data.var_decl.type;
                
                // السماح بتعيين BOOL إلى INT والعكس (التوافق الضمني)
                bool compatible = (exprType == declType) ||
                                  (exprType == TYPE_BOOL && declType == TYPE_INT) ||
                                  (exprType == TYPE_INT && declType == TYPE_BOOL);
                
                if (!compatible) {
                    semantic_error("Type mismatch in declaration of '%s'. Expected %s but got %s.",
                        node->data.var_decl.name,
                        datatype_to_str(declType),
                        datatype_to_str(exprType));
                }
            }
            // 2. التحقق من أن الثوابت لها قيم ابتدائية
            if (node->data.var_decl.is_const && !node->data.var_decl.expression) {
                semantic_error("Constant '%s' must be initialized.", node->data.var_decl.name);
            }
            // 3. إضافة الرمز (مع معلومات الموقع للتحذيرات)
            // TODO: الحصول على رقم السطر الفعلي من العقدة
            add_symbol(node->data.var_decl.name,
                       node->data.var_decl.is_global ? SCOPE_GLOBAL : SCOPE_LOCAL,
                       node->data.var_decl.type,
                       node->data.var_decl.is_const,
                       1, 1, current_filename); // مؤقتاً: سطر 1، عمود 1
            break;
        }

        case NODE_FUNC_DEF: {
            // الدخول في نطاق دالة جديدة (تصفير المحلي + إنشاء نطاق)
            local_count = 0;
            scope_depth = 0;
            scope_push();

            // إضافة المعاملات كمتغيرات محلية (المعاملات ليست ثوابت افتراضياً)
            Node* param = node->data.func_def.params;
            while (param) {
                if (param->type == NODE_VAR_DECL) {
                    // المعاملات تُعتبر "مستخدمة" ضمنياً (لتجنب تحذيرات خاطئة)
                    add_symbol(param->data.var_decl.name, SCOPE_LOCAL, param->data.var_decl.type, false,
                               1, 1, current_filename);
                    // تعليم المعامل كمستخدم مباشرة
                    local_symbols[local_count - 1].is_used = true;
                }
                param = param->next;
            }

            // تحليل جسم الدالة فقط إذا لم تكن نموذجاً أولياً
            if (!node->data.func_def.is_prototype) {
                analyze_node(node->data.func_def.body);
            }

            // خروج نطاق الدالة (يشمل تحذيرات غير المستخدم)
            scope_pop();
            break;
        }

        case NODE_BLOCK: {
            // دخول/خروج نطاق كتلة
            scope_push();
            analyze_statements_with_dead_code_check(node->data.block.statements, "return/break");
            scope_pop();
            break;
        }

        case NODE_ASSIGN: {
            Symbol* sym = lookup(node->data.assign_stmt.name, true); // تعليم كمستخدم
            if (!sym) {
                semantic_error("Assignment to undefined variable '%s'.", node->data.assign_stmt.name);
            } else {
                // التحقق من الثوابت: لا يمكن إعادة تعيين قيمة ثابت
                if (sym->is_const) {
                    semantic_error("Cannot reassign constant '%s'.", node->data.assign_stmt.name);
                }
                DataType exprType = infer_type(node->data.assign_stmt.expression);
                if (exprType != sym->type) {
                    semantic_error("Type mismatch in assignment to '%s'.", node->data.assign_stmt.name);
                }
            }
            break;
        }

        case NODE_IF: {
            DataType condType = infer_type(node->data.if_stmt.condition);
            if (condType != TYPE_INT && condType != TYPE_BOOL) {
                semantic_error("'if' condition must be an integer/boolean.");
            }

            // كل فرع يعتبر نطاقاً مستقلاً حتى بدون أقواس
            scope_push();
            analyze_node(node->data.if_stmt.then_branch);
            scope_pop();

            if (node->data.if_stmt.else_branch) {
                scope_push();
                analyze_node(node->data.if_stmt.else_branch);
                scope_pop();
            }
            break;
        }

        case NODE_WHILE: {
            DataType condType = infer_type(node->data.while_stmt.condition);
            if (condType != TYPE_INT && condType != TYPE_BOOL) {
                semantic_error("'while' condition must be an integer/boolean.");
            }
            bool prev_loop = inside_loop;
            inside_loop = true;

            scope_push();
            analyze_node(node->data.while_stmt.body);
            scope_pop();

            inside_loop = prev_loop;
            break;
        }

        case NODE_FOR: {
            // نطاق حلقة for يشمل init/cond/incr/body لمنع تسرب متغيرات init خارج الحلقة.
            scope_push();

            if (node->data.for_stmt.init) analyze_node(node->data.for_stmt.init);
            if (node->data.for_stmt.condition) {
                DataType condType = infer_type(node->data.for_stmt.condition);
                if (condType != TYPE_INT && condType != TYPE_BOOL) {
                    semantic_error("'for' condition must be an integer/boolean.");
                }
            }
            if (node->data.for_stmt.increment) analyze_node(node->data.for_stmt.increment);

            bool prev_loop = inside_loop;
            inside_loop = true;
            analyze_node(node->data.for_stmt.body);
            inside_loop = prev_loop;

            scope_pop();
            break;
        }

        case NODE_SWITCH: {
            if (infer_type(node->data.switch_stmt.expression) != TYPE_INT) {
                semantic_error("'switch' expression must be an integer.");
            }
            bool prev_switch = inside_switch;
            inside_switch = true;

            // نطاق switch
            scope_push();
            
            Node* current_case = node->data.switch_stmt.cases;
            while (current_case) {
                if (!current_case->data.case_stmt.is_default) {
                    if (infer_type(current_case->data.case_stmt.value) != TYPE_INT) {
                        semantic_error("'case' value must be an integer constant.");
                    }
                }
                // جسم الحالة عبارة عن قائمة جمل
                scope_push();
                analyze_statements_with_dead_code_check(current_case->data.case_stmt.body, "return/break");
                scope_pop();
                current_case = current_case->next;
            }

            scope_pop();
            
            inside_switch = prev_switch;
            break;
        }

        case NODE_BREAK:
            if (!inside_loop && !inside_switch) {
                semantic_error("'break' used outside of loop or switch.");
            }
            break;

        case NODE_CONTINUE:
            if (!inside_loop) {
                semantic_error("'continue' used outside of loop.");
            }
            break;

        case NODE_RETURN:
            // TODO: Check against function return type
            if (node->data.return_stmt.expression) {
                infer_type(node->data.return_stmt.expression);
            }
            break;

        case NODE_PRINT:
            infer_type(node->data.print_stmt.expression);
            break;
        
        case NODE_READ: {
            // التحقق من أن المتغير معرف وقابل للتعديل
            Symbol* sym = lookup(node->data.read_stmt.var_name, true);
            if (!sym) {
                semantic_error("Reading into undefined variable '%s'.", node->data.read_stmt.var_name);
            } else {
                if (sym->is_const) {
                    semantic_error("Cannot read into constant variable '%s'.", node->data.read_stmt.var_name);
                }
                // حالياً نقبل القراءة فقط في المتغيرات الصحيحة
                if (sym->type != TYPE_INT) {
                    semantic_error("'اقرأ' currently only supports INTEGER variables.");
                }
            }
            break;
        }
            
        case NODE_CALL_STMT:
            // استنتاج النوع للتحقق من وجود الدالة
            infer_type((Node*)&node->data.call); // Hack cast to access call data in infer_type
            break;

        case NODE_ARRAY_DECL:
            add_symbol(node->data.array_decl.name, SCOPE_LOCAL, TYPE_INT, node->data.array_decl.is_const,
                       1, 1, current_filename);
            break;

        case NODE_ARRAY_ASSIGN: {
            Symbol* sym = lookup(node->data.array_op.name, true); // تعليم كمستخدم
            if (!sym) {
                semantic_error("Assignment to undefined array '%s'.", node->data.array_op.name);
            } else {
                // التحقق من الثوابت: لا يمكن تعديل عناصر مصفوفة ثابتة
                if (sym->is_const) {
                    semantic_error("Cannot modify constant array '%s'.", node->data.array_op.name);
                }
            }
            if (infer_type(node->data.array_op.index) != TYPE_INT) {
                semantic_error("Array index must be integer.");
            }
            if (infer_type(node->data.array_op.value) != TYPE_INT) {
                semantic_error("Array value must be integer (Strings not supported in arrays yet).");
            }
            break;
        }

        default:
            break;
    }
}

/**
 * @brief نقطة الدخول الرئيسية للتحليل.
 * @param program عقدة البرنامج
 * @return true إذا لم تحدث أخطاء
 */
bool analyze(Node* program) {
    reset_analysis();
    
    // تعيين اسم الملف الحالي (مؤقتاً نستخدم "source")
    current_filename = "source";
    
    analyze_node(program);
    return !has_error;
}
