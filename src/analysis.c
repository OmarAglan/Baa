/**
 * @file analysis.c
 * @brief المحلل الدلالي (Semantic Analyzer) - يقوم بفحص الأنواع وحل الرموز قبل توليد الكود.
 * @version 0.2.4
 * 
 * يدعم:
 * - جدول رموز متعدد النطاقات (Scoped Symbol Table)
 * - فحص الأنواع (Type Checking)
 * - حل الرموز (Symbol Resolution)
 */

#include "baa.h"
#include <string.h>

// ============================================================================
// تعريف جدول الرموز (Symbol Table)
// ============================================================================

Symbol global_symbols[100];
int global_count = 0;
Symbol local_symbols[100];
int local_count = 0;
int current_stack_offset = 0;

// ============================================================================
// نظام مكدس النطاقات (Scope Stack) - لدعم الكتل المتداخلة
// ============================================================================

// مكدس يحفظ عدد المتغيرات المحلية عند دخول كل نطاق
static int scope_stack[50];   // مكدس مستويات النطاق
static int scope_depth = 0;   // عمق النطاق الحالي

/**
 * @brief دخول نطاق جديد (كتلة { } ).
 */
static void enter_scope(void) {
    if (scope_depth >= 50) {
        fprintf(stderr, "Semantic Error: Nesting too deep (max 50 levels)\n");
        return;
    }
    // حفظ عدد المتغيرات الحالية قبل دخول النطاق الجديد
    scope_stack[scope_depth] = local_count;
    scope_depth++;
}

/**
 * @brief الخروج من النطاق الحالي (إزالة المتغيرات المحلية للنطاق).
 */
static void exit_scope(void) {
    if (scope_depth > 0) {
        scope_depth--;
        // استعادة عدد المتغيرات المحلية لما قبل النطاق
        local_count = scope_stack[scope_depth];
    }
}

// ============================================================================
// دالات جدول الرموز (Symbol Table Functions)
// ============================================================================

/**
 * @brief إضافة متغير عام لجدول الرموز.
 */
void add_global(const char* name, DataType type) {
    // التحقق من عدم وجود الرمز مسبقاً
    for (int i = 0; i < global_count; i++) {
        if (strcmp(global_symbols[i].name, name) == 0) {
            fprintf(stderr, "Semantic Error: Redefinition of global variable '%s'\n", name);
            return;
        }
    }
    
    strcpy(global_symbols[global_count].name, name);
    global_symbols[global_count].scope = SCOPE_GLOBAL;
    global_symbols[global_count].type = type;
    global_symbols[global_count].offset = 0;
    global_count++;
}

/**
 * @brief تهيئة نطاق الوظيفة (تصفير المتغيرات المحلية).
 */
void enter_function_scope(void) {
    local_count = 0;
    current_stack_offset = 0;
    scope_depth = 0; // إعادة تعيين مكدس النطاقات
}

/**
 * @brief إضافة متغير محلي.
 * @param size عدد وحدات 64-بت المطلوبة (1 للأعداد الصحيحة، N للمصفوفات).
 */
void add_local(const char* name, int size, DataType type) {
    // التحقق من عدم وجود الرمز في النطاق الحالي فقط
    int scope_start = (scope_depth > 0) ? scope_stack[scope_depth - 1] : 0;
    for (int i = scope_start; i < local_count; i++) {
        if (strcmp(local_symbols[i].name, name) == 0) {
            fprintf(stderr, "Semantic Error: Redefinition of variable '%s' in same scope\n", name);
            return;
        }
    }
    
    // 1. تحديد موقع الرمز (بداية الكتلة المحجوزة)
    int symbol_offset = current_stack_offset - 8;
    
    // 2. تحديث مؤشر المكدس لحجز المساحة كاملة (المكدس ينمو للأسفل)
    current_stack_offset -= (size * 8);
    
    strcpy(local_symbols[local_count].name, name);
    local_symbols[local_count].scope = SCOPE_LOCAL;
    local_symbols[local_count].type = type;
    local_symbols[local_count].offset = symbol_offset;
    local_count++;
}

/**
 * @brief البحث عن رمز بالاسم في النطاق المحلي ثم العام.
 */
Symbol* lookup_symbol(const char* name) {
    // التحقق من المتغيرات المحلية أولاً (Shadowing)
    for (int i = 0; i < local_count; i++) {
        if (strcmp(local_symbols[i].name, name) == 0) {
            return &local_symbols[i];
        }
    }
    // التحقق من المتغيرات العامة
    for (int i = 0; i < global_count; i++) {
        if (strcmp(global_symbols[i].name, name) == 0) {
            return &global_symbols[i];
        }
    }
    return NULL; // لم يتم العثور على الرمز
}

// ============================================================================
// فحص الأنواع (Type Checking)
// ============================================================================

/**
 * @brief الحصول على نوع تعبير معين.
 * @return نوع البيانات للتعبير، أو -1 إذا غير معروف.
 */
static DataType get_expression_type(Node* node) {
    if (node == NULL) return TYPE_INT; // افتراضي
    
    switch (node->type) {
        case NODE_INT:
        case NODE_CHAR:
            return TYPE_INT;
            
        case NODE_STRING:
            return TYPE_STRING;
            
        case NODE_VAR_REF: {
            Symbol* sym = lookup_symbol(node->data.var_ref.name);
            if (sym != NULL) return sym->type;
            return TYPE_INT; // افتراضي في حالة الخطأ
        }
        
        case NODE_ARRAY_ACCESS:
            // المصفوفات حالياً تدعم int فقط
            return TYPE_INT;
            
        case NODE_BIN_OP:
            // العمليات الثنائية تعيد int دائماً
            return TYPE_INT;
            
        case NODE_UNARY_OP:
        case NODE_POSTFIX_OP:
            return TYPE_INT;
            
        case NODE_CALL_EXPR:
            // حالياً نفترض أن جميع الدوال تعيد int
            return TYPE_INT;
            
        default:
            return TYPE_INT;
    }
}

/**
 * @brief التحقق من توافق الأنواع بين المتغير والتعبير.
 * @return true إذا متوافقة، false إذا غير متوافقة.
 */
static bool check_type_compatibility(DataType var_type, DataType expr_type, const char* var_name) {
    if (var_type != expr_type) {
        const char* var_type_str = (var_type == TYPE_INT) ? "صحيح (int)" : "نص (string)";
        const char* expr_type_str = (expr_type == TYPE_INT) ? "صحيح (int)" : "نص (string)";
        fprintf(stderr, "Semantic Error: Type mismatch for '%s': cannot assign %s to %s\n", 
                var_name, expr_type_str, var_type_str);
        return false;
    }
    return true;
}

// ============================================================================
// التحليل الدلالي (Semantic Analysis)
// ============================================================================

// تصريح مسبق للدالات الداخلية
static bool analyze_node(Node* node);
static bool analyze_expression(Node* node);

/**
 * @brief تحليل تعبير للتحقق من صحة الرموز.
 */
static bool analyze_expression(Node* node) {
    if (node == NULL) return true;
    
    bool success = true;
    
    switch (node->type) {
        case NODE_INT:
        case NODE_STRING:
        case NODE_CHAR:
            // الثوابت صالحة دائماً
            break;
            
        case NODE_VAR_REF: {
            Symbol* sym = lookup_symbol(node->data.var_ref.name);
            if (sym == NULL) {
                fprintf(stderr, "Semantic Error: Undefined variable '%s'\n", node->data.var_ref.name);
                success = false;
            }
            break;
        }
        
        case NODE_ARRAY_ACCESS: {
            Symbol* sym = lookup_symbol(node->data.array_op.name);
            if (sym == NULL) {
                fprintf(stderr, "Semantic Error: Undefined array '%s'\n", node->data.array_op.name);
                success = false;
            }
            success = analyze_expression(node->data.array_op.index) && success;
            
            // التحقق من أن الفهرس من نوع int
            DataType idx_type = get_expression_type(node->data.array_op.index);
            if (idx_type != TYPE_INT) {
                fprintf(stderr, "Semantic Error: Array index must be integer for '%s'\n", node->data.array_op.name);
                success = false;
            }
            break;
        }
        
        case NODE_BIN_OP:
            success = analyze_expression(node->data.bin_op.left) && success;
            success = analyze_expression(node->data.bin_op.right) && success;
            
            // التحقق من أن العمليات الحسابية على أرقام فقط
            if (node->data.bin_op.op >= OP_ADD && node->data.bin_op.op <= OP_MOD) {
                DataType left_type = get_expression_type(node->data.bin_op.left);
                DataType right_type = get_expression_type(node->data.bin_op.right);
                if (left_type != TYPE_INT || right_type != TYPE_INT) {
                    fprintf(stderr, "Semantic Error: Arithmetic operations require integer operands\n");
                    success = false;
                }
            }
            break;
            
        case NODE_UNARY_OP:
        case NODE_POSTFIX_OP:
            success = analyze_expression(node->data.unary_op.operand) && success;
            break;
            
        case NODE_CALL_EXPR: {
            // التحقق من وجود الدالة (حالياً نسمح بأي اسم دالة)
            // في المستقبل يمكن التحقق من جدول الدوال
            Node* arg = node->data.call.args;
            while (arg != NULL) {
                success = analyze_expression(arg) && success;
                arg = arg->next;
            }
            break;
        }
        
        default:
            break;
    }
    
    return success;
}

/**
 * @brief تحليل عقدة AST واحدة.
 */
static bool analyze_node(Node* node) {
    if (node == NULL) return true;
    
    bool success = true;
    
    switch (node->type) {
        case NODE_PROGRAM: {
            // المرور الأول: تسجيل جميع المتغيرات العامة والدوال
            Node* decl = node->data.program.declarations;
            while (decl != NULL) {
                if (decl->type == NODE_VAR_DECL && decl->data.var_decl.is_global) {
                    add_global(decl->data.var_decl.name, decl->data.var_decl.type);
                }
                decl = decl->next;
            }
            
            // المرور الثاني: تحليل أجسام الدوال
            decl = node->data.program.declarations;
            while (decl != NULL) {
                success = analyze_node(decl) && success;
                decl = decl->next;
            }
            break;
        }
        
        case NODE_FUNC_DEF: {
            enter_function_scope();
            
            // تسجيل المعاملات كمتغيرات محلية
            Node* param = node->data.func_def.params;
            while (param != NULL) {
                add_local(param->data.var_decl.name, 1, param->data.var_decl.type);
                param = param->next;
            }
            
            // تحليل جسم الدالة
            success = analyze_node(node->data.func_def.body) && success;
            break;
        }
        
        case NODE_BLOCK: {
            // دخول نطاق جديد للكتلة
            enter_scope();
            
            Node* stmt = node->data.block.statements;
            while (stmt != NULL) {
                success = analyze_node(stmt) && success;
                stmt = stmt->next;
            }
            
            // الخروج من النطاق (إزالة المتغيرات المحلية)
            exit_scope();
            break;
        }
        
        case NODE_VAR_DECL:
            if (!node->data.var_decl.is_global) {
                success = analyze_expression(node->data.var_decl.expression) && success;
                
                // فحص توافق الأنواع
                if (node->data.var_decl.expression != NULL) {
                    DataType expr_type = get_expression_type(node->data.var_decl.expression);
                    if (!check_type_compatibility(node->data.var_decl.type, expr_type, node->data.var_decl.name)) {
                        success = false;
                    }
                }
                
                add_local(node->data.var_decl.name, 1, node->data.var_decl.type);
            }
            break;
            
        case NODE_ARRAY_DECL:
            add_local(node->data.array_decl.name, node->data.array_decl.size, TYPE_INT);
            break;
            
        case NODE_ASSIGN: {
            Symbol* sym = lookup_symbol(node->data.assign_stmt.name);
            if (sym == NULL) {
                fprintf(stderr, "Semantic Error: Undefined variable '%s'\n", node->data.assign_stmt.name);
                success = false;
            } else {
                success = analyze_expression(node->data.assign_stmt.expression) && success;
                
                // فحص توافق الأنواع
                DataType expr_type = get_expression_type(node->data.assign_stmt.expression);
                if (!check_type_compatibility(sym->type, expr_type, node->data.assign_stmt.name)) {
                    success = false;
                }
            }
            break;
        }
        
        case NODE_ARRAY_ASSIGN: {
            Symbol* sym = lookup_symbol(node->data.array_op.name);
            if (sym == NULL) {
                fprintf(stderr, "Semantic Error: Undefined array '%s'\n", node->data.array_op.name);
                success = false;
            }
            success = analyze_expression(node->data.array_op.index) && success;
            success = analyze_expression(node->data.array_op.value) && success;
            
            // التحقق من أن القيمة من نوع int (المصفوفات تدعم int فقط حالياً)
            DataType val_type = get_expression_type(node->data.array_op.value);
            if (val_type != TYPE_INT) {
                fprintf(stderr, "Semantic Error: Array '%s' only accepts integer values\n", node->data.array_op.name);
                success = false;
            }
            break;
        }
        
        case NODE_IF:
            success = analyze_expression(node->data.if_stmt.condition) && success;
            success = analyze_node(node->data.if_stmt.then_branch) && success;
            if (node->data.if_stmt.else_branch) {
                success = analyze_node(node->data.if_stmt.else_branch) && success;
            }
            break;
            
        case NODE_WHILE:
            success = analyze_expression(node->data.while_stmt.condition) && success;
            success = analyze_node(node->data.while_stmt.body) && success;
            break;
            
        case NODE_FOR:
            // إنشاء نطاق جديد لحلقة for (لأن المتغير المعرف في init يكون محلياً للحلقة)
            enter_scope();
            
            if (node->data.for_stmt.init) {
                success = analyze_node(node->data.for_stmt.init) && success;
            }
            if (node->data.for_stmt.condition) {
                success = analyze_expression(node->data.for_stmt.condition) && success;
            }
            if (node->data.for_stmt.increment) {
                // الزيادة قد تكون تعبيراً أو جملة تعيين
                if (node->data.for_stmt.increment->type == NODE_ASSIGN) {
                    success = analyze_node(node->data.for_stmt.increment) && success;
                } else {
                    success = analyze_expression(node->data.for_stmt.increment) && success;
                }
            }
            success = analyze_node(node->data.for_stmt.body) && success;
            
            exit_scope();
            break;
            
        case NODE_SWITCH:
            success = analyze_expression(node->data.switch_stmt.expression) && success;
            {
                Node* c = node->data.switch_stmt.cases;
                while (c != NULL) {
                    success = analyze_node(c) && success;
                    c = c->next;
                }
            }
            break;
            
        case NODE_CASE: {
            Node* stmt = node->data.case_stmt.body;
            while (stmt != NULL) {
                success = analyze_node(stmt) && success;
                stmt = stmt->next;
            }
            break;
        }
        
        case NODE_RETURN:
            success = analyze_expression(node->data.return_stmt.expression) && success;
            break;
            
        case NODE_PRINT:
            success = analyze_expression(node->data.print_stmt.expression) && success;
            break;
            
        case NODE_CALL_STMT: {
            Node* arg = node->data.call.args;
            while (arg != NULL) {
                success = analyze_expression(arg) && success;
                arg = arg->next;
            }
            break;
        }
        
        case NODE_BREAK:
        case NODE_CONTINUE:
            // لا يحتاج تحليل إضافي
            break;
            
        default:
            break;
    }
    
    return success;
}

/**
 * @brief نقطة الدخول للتحليل الدلالي.
 */
bool analyze(Node* ast) {
    // إعادة تعيين جدول الرموز
    global_count = 0;
    local_count = 0;
    current_stack_offset = 0;
    scope_depth = 0;
    
    return analyze_node(ast);
}
