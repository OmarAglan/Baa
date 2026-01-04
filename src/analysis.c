/**
 * @file analysis.c
 * @brief وحدة التحليل الدلالي (Semantic Analysis).
 * @details تقوم هذه الوحدة بالتحقق من صحة البرنامج قبل توليد الكود، بما في ذلك التحقق من الأنواع (Type Checking) والتحقق من النطاقات (Scope Analysis).
 * @version 0.2.4
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

static bool has_error = false;
static bool inside_loop = false;
static bool inside_switch = false;

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
    has_error = false;
    inside_loop = false;
    inside_switch = false;
}

/**
 * @brief إضافة رمز إلى النطاق الحالي.
 */
static void add_symbol(const char* name, ScopeType scope, DataType type) {
    if (scope == SCOPE_GLOBAL) {
        // التحقق من التكرار
        for (int i = 0; i < global_count; i++) {
            if (strcmp(global_symbols[i].name, name) == 0) {
                semantic_error("Redefinition of global variable '%s'.", name);
                return;
            }
        }
        if (global_count >= 100) { semantic_error("Too many global variables."); return; }
        
        strcpy(global_symbols[global_count].name, name);
        global_symbols[global_count].scope = SCOPE_GLOBAL;
        global_symbols[global_count].type = type;
        global_count++;
    } else {
        // التحقق من التكرار محلياً
        for (int i = 0; i < local_count; i++) {
            if (strcmp(local_symbols[i].name, name) == 0) {
                semantic_error("Redefinition of local variable '%s'.", name);
                return;
            }
        }
        if (local_count >= 100) { semantic_error("Too many local variables."); return; }

        strcpy(local_symbols[local_count].name, name);
        local_symbols[local_count].scope = SCOPE_LOCAL;
        local_symbols[local_count].type = type;
        local_count++;
    }
}

/**
 * @brief البحث عن رمز بالاسم.
 */
static Symbol* lookup(const char* name) {
    // البحث في المحلي أولاً
    for (int i = 0; i < local_count; i++) {
        if (strcmp(local_symbols[i].name, name) == 0) return &local_symbols[i];
    }
    // البحث في العام
    for (int i = 0; i < global_count; i++) {
        if (strcmp(global_symbols[i].name, name) == 0) return &global_symbols[i];
    }
    return NULL;
}

// ============================================================================
// دوال التحليل (Analysis Functions)
// ============================================================================

// تصريح مسبق للدالة العودية
static void analyze_node(Node* node);

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
            
        case NODE_VAR_REF: {
            Symbol* sym = lookup(node->data.var_ref.name);
            if (!sym) {
                semantic_error("Undefined variable '%s'.", node->data.var_ref.name);
                return TYPE_INT; // استرداد الخطأ
            }
            return sym->type;
        }

        case NODE_ARRAY_ACCESS: {
            // حالياً المصفوفات تدعم الأعداد الصحيحة فقط
            Symbol* sym = lookup(node->data.array_op.name);
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
            
            // العمليات الحسابية والمنطقية تتطلب أعداداً صحيحة
            if (left != TYPE_INT || right != TYPE_INT) {
                semantic_error("Binary operations require INTEGER operands.");
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

static void analyze_node(Node* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM: {
            Node* decl = node->data.program.declarations;
            while (decl) {
                analyze_node(decl);
                decl = decl->next;
            }
            break;
        }

        case NODE_VAR_DECL: {
            // 1. التحقق من نوع التعبير المخصص (إن وجد)
            if (node->data.var_decl.expression) {
                DataType exprType = infer_type(node->data.var_decl.expression);
                if (exprType != node->data.var_decl.type) {
                    semantic_error("Type mismatch in declaration of '%s'. Expected %s but got %s.",
                        node->data.var_decl.name,
                        node->data.var_decl.type == TYPE_INT ? "INTEGER" : "STRING",
                        exprType == TYPE_INT ? "INTEGER" : "STRING");
                }
            }
            // 2. إضافة الرمز
            add_symbol(node->data.var_decl.name, 
                       node->data.var_decl.is_global ? SCOPE_GLOBAL : SCOPE_LOCAL,
                       node->data.var_decl.type);
            break;
        }

        case NODE_FUNC_DEF: {
            // الدخول في نطاق دالة جديدة (تصفير المحلي)
            local_count = 0;
            
            // إضافة المعاملات كمتغيرات محلية
            Node* param = node->data.func_def.params;
            while (param) {
                if (param->type == NODE_VAR_DECL) {
                     add_symbol(param->data.var_decl.name, SCOPE_LOCAL, param->data.var_decl.type);
                }
                param = param->next;
            }

            // تحليل جسم الدالة فقط إذا لم تكن نموذجاً أولياً
            if (!node->data.func_def.is_prototype) {
                analyze_node(node->data.func_def.body);
            }
            break;
        }

        case NODE_BLOCK: {
            Node* stmt = node->data.block.statements;
            while (stmt) {
                analyze_node(stmt);
                stmt = stmt->next;
            }
            break;
        }

        case NODE_ASSIGN: {
            Symbol* sym = lookup(node->data.assign_stmt.name);
            if (!sym) {
                semantic_error("Assignment to undefined variable '%s'.", node->data.assign_stmt.name);
            } else {
                DataType exprType = infer_type(node->data.assign_stmt.expression);
                if (exprType != sym->type) {
                    semantic_error("Type mismatch in assignment to '%s'.", node->data.assign_stmt.name);
                }
            }
            break;
        }

        case NODE_IF: {
            if (infer_type(node->data.if_stmt.condition) != TYPE_INT) {
                semantic_error("'if' condition must be an integer/boolean.");
            }
            analyze_node(node->data.if_stmt.then_branch);
            if (node->data.if_stmt.else_branch) {
                analyze_node(node->data.if_stmt.else_branch);
            }
            break;
        }

        case NODE_WHILE: {
            if (infer_type(node->data.while_stmt.condition) != TYPE_INT) {
                semantic_error("'while' condition must be an integer/boolean.");
            }
            bool prev_loop = inside_loop;
            inside_loop = true;
            analyze_node(node->data.while_stmt.body);
            inside_loop = prev_loop;
            break;
        }

        case NODE_FOR: {
            // For Loop Scope is tricky without nested scopes in symbol table.
            // For now, we treat init variables as function-local (like C89/Baa current implementation).
            if (node->data.for_stmt.init) analyze_node(node->data.for_stmt.init);
            if (node->data.for_stmt.condition) {
                if (infer_type(node->data.for_stmt.condition) != TYPE_INT) {
                    semantic_error("'for' condition must be an integer/boolean.");
                }
            }
            if (node->data.for_stmt.increment) analyze_node(node->data.for_stmt.increment);
            
            bool prev_loop = inside_loop;
            inside_loop = true;
            analyze_node(node->data.for_stmt.body);
            inside_loop = prev_loop;
            break;
        }

        case NODE_SWITCH: {
            if (infer_type(node->data.switch_stmt.expression) != TYPE_INT) {
                semantic_error("'switch' expression must be an integer.");
            }
            bool prev_switch = inside_switch;
            inside_switch = true;
            
            Node* current_case = node->data.switch_stmt.cases;
            while (current_case) {
                if (!current_case->data.case_stmt.is_default) {
                    if (infer_type(current_case->data.case_stmt.value) != TYPE_INT) {
                        semantic_error("'case' value must be an integer constant.");
                    }
                }
                analyze_node(current_case->data.case_stmt.body);
                current_case = current_case->next;
            }
            
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
            
        case NODE_CALL_STMT:
            // استنتاج النوع للتحقق من وجود الدالة
            infer_type((Node*)&node->data.call); // Hack cast to access call data in infer_type
            break;

        case NODE_ARRAY_DECL:
            add_symbol(node->data.array_decl.name, SCOPE_LOCAL, TYPE_INT);
            break;

        case NODE_ARRAY_ASSIGN: {
            Symbol* sym = lookup(node->data.array_op.name);
            if (!sym) {
                semantic_error("Assignment to undefined array '%s'.", node->data.array_op.name);
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
 */
bool analyze(Node* program) {
    reset_analysis();
    analyze_node(program);
    return !has_error;
}