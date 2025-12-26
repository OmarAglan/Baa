/**
 * @file codegen.c
 * @brief يقوم بتوليد كود التجميع (Assembly) للتركيب المعماري x86_64 مع دعم العمليات الحسابية والمنطقية والمصفوفات وحلقات التكرار.
 * @version 0.1.1 (Phase 3 - For Loop)
 */

#include "baa.h"
#include <string.h>

// --- نظام جدول الرموز (Symbol Table) ---

typedef enum { SCOPE_GLOBAL, SCOPE_LOCAL } ScopeType;

/**
 * @struct Symbol
 * @brief يمثل رمزاً (متغيراً) في جدول الرموز.
 */
typedef struct { 
    char name[32];     // اسم الرمز
    ScopeType scope;   // النطاق (عام أو محلي)
    int offset;        // الإزاحة من مؤشر القاعدة (RBP) للمتغيرات المحلية
} Symbol;

Symbol global_symbols[100]; int global_count = 0;
Symbol local_symbols[100]; int local_count = 0; 
int current_stack_offset = 0; // يتتبع موقع الـ RSP بالنسبة للـ RBP
int label_counter = 0;        // لإنشاء تسميات فريدة للجمل الشرطية وحلقات التكرار

/**
 * @brief إضافة متغير عام لجدول الرموز.
 */
void add_global(const char* name) {
    strcpy(global_symbols[global_count].name, name);
    global_symbols[global_count].scope = SCOPE_GLOBAL;
    global_symbols[global_count].offset = 0;
    global_count++;
}

/**
 * @brief تهيئة نطاق الوظيفة (تصفير المتغيرات المحلية).
 */
void enter_function_scope() { local_count = 0; current_stack_offset = 0; }

/**
 * @brief إضافة متغير محلي.
 * @param size عدد وحدات 64-بت المطلوبة (1 للأعداد الصحيحة، N للمصفوفات).
 */
void add_local(const char* name, int size) {
    // لحساب الإزاحة الأساسية للمصفوفة:
    // المصفوفة تُحجز ككتلة. الإزاحة المسجلة في الجدول ستشير إلى العنصر الأول (الفهرس 0).
    // مثال: المصفوفة حجم 2.
    // المكدس الحالي: -8.
    // نحجز 16 بايت. المكدس الجديد: -24.
    // العنصر 0 عند -16. العنصر 1 عند -24.
    // إذا سجلنا الإزاحة كـ -16:
    // عنوان العنصر i = القاعدة - (i * 8).
    // i=0: -16 - 0 = -16 (صحيح)
    // i=1: -16 - 8 = -24 (صحيح)
    
    // الصيغة: إزاحة الرمز = (الإزاحة الحالية - 8)
    // الإزاحة الحالية الجديدة = الإزاحة الحالية - (الحجم * 8)
    
    // ملاحظة: هذا يفترض أننا نملأ من الأعلى للأسفل وأن عنوان الأساس هو العنوان "الأعلى" في الذاكرة.
    
    // تصحيح المنطق البسيط:
    // نعتبر الرمز يشير إلى "بداية" الحجز في المكدس (أعلى عنوان).
    
    // 1. تحديد موقع الرمز (بداية الكتلة المحجوزة)
    // نعتبر الرمز يشير إلى "بداية" الحجز في المكدس (أعلى عنوان/أقل إزاحة سالبة)
    int symbol_offset = current_stack_offset - 8;
    
    // 2. تحديث مؤشر المكدس لحجز المساحة كاملة (المكدس ينمو للأسفل)
    current_stack_offset -= (size * 8);
    
    strcpy(local_symbols[local_count].name, name);
    local_symbols[local_count].scope = SCOPE_LOCAL;
    local_symbols[local_count].offset = symbol_offset;
    local_count++;
}

/**
 * @brief البحث عن رمز بالاسم في النطاق المحلي ثم العام.
 */
Symbol* lookup_symbol(const char* name) {
    // التحقق من المتغيرات المحلية أولاً (Shadowing)
    for (int i = 0; i < local_count; i++) if (strcmp(local_symbols[i].name, name) == 0) return &local_symbols[i];
    // التحقق من المتغيرات العامة
    for (int i = 0; i < global_count; i++) if (strcmp(global_symbols[i].name, name) == 0) return &global_symbols[i];
    printf("Codegen Error: Undefined symbol '%s'\n", name); exit(1);
}

// --- جدول النصوص (String Table) ---

typedef struct { char* content; int id; } StringEntry;
StringEntry string_table[100];
int string_count = 0;

/**
 * @brief تسجيل نص ثابت وتعيين معرف فريد له.
 */
int register_string(char* content) {
    for(int i=0; i<string_count; i++) {
        if(strcmp(string_table[i].content, content) == 0) return string_table[i].id;
    }
    string_table[string_count].content = content;
    string_table[string_count].id = string_count;
    return string_count++;
}

// --- تصريح مسبق للمولد ---
void codegen(Node* node, FILE* file);

// --- مولد التعبيرات (Expression Generator) ---

/**
 * @brief توليد كود التجميع لتعبير معين. النتيجة تخزن دائماً في مسجل %rax.
 */
void gen_expr(Node* node, FILE* file) {
    if (node->type == NODE_INT) {
        fprintf(file, "    mov $%d, %%rax\n", node->data.integer.value);
    }
    else if (node->type == NODE_STRING) {
        int id = register_string(node->data.string_lit.value);
        fprintf(file, "    lea .Lstr_%d(%%rip), %%rax\n", id);
    }
    else if (node->type == NODE_CHAR) {
        fprintf(file, "    mov $%d, %%rax\n", node->data.char_lit.value);
    }
    else if (node->type == NODE_VAR_REF) {
        Symbol* sym = lookup_symbol(node->data.var_ref.name);
        if (sym->scope == SCOPE_LOCAL) fprintf(file, "    mov %d(%%rbp), %%rax\n", sym->offset);
        else fprintf(file, "    mov %s(%%rip), %%rax\n", sym->name);
    }
    // الوصول للمصفوفة (قراءة)
    else if (node->type == NODE_ARRAY_ACCESS) {
        Symbol* sym = lookup_symbol(node->data.array_op.name);
        // 1. حساب الفهرس
        gen_expr(node->data.array_op.index, file);
        // نقل الفهرس إلى RCX
        fprintf(file, "    mov %%rax, %%rcx\n");
        
        // 2. حساب العنوان: RBP + Offset - (Index * 8)
        // نقوم بذلك يدوياً لأن عنونة x86 لا تدعم طرح الإزاحة الديناميكية بسهولة مع إزاحة سالبة ثابتة
        fprintf(file, "    imul $8, %%rcx\n");      // Index * 8
        fprintf(file, "    neg %%rcx\n");           // -(Index * 8)
        fprintf(file, "    add $%d, %%rcx\n", sym->offset); // Offset + (-(Index*8))
        
        // تحميل القيمة من العنوان المحسوب
        fprintf(file, "    mov (%%rbp, %%rcx, 1), %%rax\n");
    }
    else if (node->type == NODE_CALL_EXPR) {
        // استدعاء دالة: اتباع اتفاقية Windows (RCX, RDX, R8, R9)
        const char* regs[] = {"%%rcx", "%%rdx", "%%r8", "%%r9"};
        int arg_idx = 0;
        Node* arg = node->data.call.args;
        while (arg != NULL) {
            gen_expr(arg, file);
            fprintf(file, "    push %%rax\n");
            arg = arg->next;
            arg_idx++;
        }
        // سحب الوسائط إلى المسجلات بترتيب عكسي
        for (int i = arg_idx - 1; i >= 0; i--) {
            if (i < 4) fprintf(file, "    pop %s\n", regs[i]);
            else { printf("Error: Too many arguments\n"); exit(1); }
        }
        // تخصيص "Shadow Space" (مطلوب في Windows ABI)
        fprintf(file, "    sub $32, %%rsp\n"); 
        fprintf(file, "    call %s\n", node->data.call.name);
        fprintf(file, "    add $32, %%rsp\n");
    }
    // العمليات الأحادية واللاحقة
    else if (node->type == NODE_POSTFIX_OP) {
        // التعامل مع ++ و --
        Node* operand = node->data.unary_op.operand;
        
        if (operand->type == NODE_VAR_REF) {
            Symbol* sym = lookup_symbol(operand->data.var_ref.name);
            // تحميل القيمة الحالية
            if (sym->scope == SCOPE_LOCAL) fprintf(file, "    mov %d(%%rbp), %%rax\n", sym->offset);
            else fprintf(file, "    mov %s(%%rip), %%rax\n", sym->name);
            
            // حفظ القيمة الأصلية لاستخدامها كنتيجة للتعبير
            fprintf(file, "    mov %%rax, %%rdx\n"); 
            
            // التعديل
            if (node->data.unary_op.op == UOP_INC) fprintf(file, "    add $1, %%rdx\n");
            else fprintf(file, "    sub $1, %%rdx\n");
            
            // التخزين
            if (sym->scope == SCOPE_LOCAL) fprintf(file, "    mov %%rdx, %d(%%rbp)\n", sym->offset);
            else fprintf(file, "    mov %%rdx, %s(%%rip)\n", sym->name);
            
            // %rax لا يزال يحتوي على القيمة القديمة (Postfix)
        }
        else if (operand->type == NODE_ARRAY_ACCESS) {
            Symbol* sym = lookup_symbol(operand->data.array_op.name);
            
            // حساب العنوان (يخزن في RCX)
            gen_expr(operand->data.array_op.index, file);
            fprintf(file, "    mov %%rax, %%rcx\n");
            fprintf(file, "    imul $8, %%rcx\n");
            fprintf(file, "    neg %%rcx\n");
            fprintf(file, "    add $%d, %%rcx\n", sym->offset);
            
            // تحميل القيمة
            fprintf(file, "    mov (%%rbp, %%rcx, 1), %%rax\n");
            
            // نسخ للتعديل
            fprintf(file, "    mov %%rax, %%rdx\n");
            if (node->data.unary_op.op == UOP_INC) fprintf(file, "    add $1, %%rdx\n");
            else fprintf(file, "    sub $1, %%rdx\n");
            
            // التخزين
            fprintf(file, "    mov %%rdx, (%%rbp, %%rcx, 1)\n");
        }
    }
    else if (node->type == NODE_UNARY_OP) {
        gen_expr(node->data.unary_op.operand, file);
        if (node->data.unary_op.op == UOP_NEG) {
            fprintf(file, "    neg %%rax\n");
        } else if (node->data.unary_op.op == UOP_NOT) {
            fprintf(file, "    cmp $0, %%rax\n");
            fprintf(file, "    sete %%al\n");
            fprintf(file, "    movzbq %%al, %%rax\n");
        }
    }
    else if (node->type == NODE_BIN_OP) {
        // التعامل مع "Short-circuit" للعمليات المنطقية (AND/OR)
        if (node->data.bin_op.op == OP_AND) {
            int end_label = label_counter++;
            gen_expr(node->data.bin_op.left, file);
            fprintf(file, "    cmp $0, %%rax\n");
            fprintf(file, "    je .Lsc_%d\n", end_label); // إذا كان الطرف الأول خطأ، توقف
            gen_expr(node->data.bin_op.right, file); 
            fprintf(file, "    cmp $0, %%rax\n");
            fprintf(file, "    setne %%al\n");
            fprintf(file, "    movzbq %%al, %%rax\n");
            fprintf(file, ".Lsc_%d:\n", end_label);
            return;
        }
        if (node->data.bin_op.op == OP_OR) {
            int end_label = label_counter++;
            int true_label = label_counter++;
            gen_expr(node->data.bin_op.left, file);
            fprintf(file, "    cmp $0, %%rax\n");
            fprintf(file, "    jne .Ltrue_%d\n", true_label); // إذا كان الطرف الأول صح، القفز للنتيجة صح
            gen_expr(node->data.bin_op.right, file);
            fprintf(file, "    cmp $0, %%rax\n");
            fprintf(file, "    jne .Ltrue_%d\n", true_label);
            fprintf(file, "    mov $0, %%rax\n"); 
            fprintf(file, "    jmp .Lsc_%d\n", end_label);
            fprintf(file, ".Ltrue_%d:\n", true_label);
            fprintf(file, "    mov $1, %%rax\n");
            fprintf(file, ".Lsc_%d:\n", end_label);
            return;
        }

        // العمليات الثنائية المعيارية
        gen_expr(node->data.bin_op.right, file);
        fprintf(file, "    push %%rax\n");
        gen_expr(node->data.bin_op.left, file);
        fprintf(file, "    pop %%rbx\n");
        
        if (node->data.bin_op.op == OP_ADD) fprintf(file, "    add %%rbx, %%rax\n");
        else if (node->data.bin_op.op == OP_SUB) fprintf(file, "    sub %%rbx, %%rax\n");
        else if (node->data.bin_op.op == OP_MUL) fprintf(file, "    imul %%rbx, %%rax\n");
        else if (node->data.bin_op.op == OP_DIV) { fprintf(file, "    cqo\n    idiv %%rbx\n"); }
        else if (node->data.bin_op.op == OP_MOD) { fprintf(file, "    cqo\n    idiv %%rbx\n    mov %%rdx, %%rax\n"); }
        else {
            // عمليات المقارنة
            fprintf(file, "    cmp %%rbx, %%rax\n");
            if (node->data.bin_op.op == OP_EQ) fprintf(file, "    sete %%al\n");
            else if (node->data.bin_op.op == OP_NEQ) fprintf(file, "    setne %%al\n");
            else if (node->data.bin_op.op == OP_LT) fprintf(file, "    setl %%al\n");
            else if (node->data.bin_op.op == OP_GT) fprintf(file, "    setg %%al\n");
            else if (node->data.bin_op.op == OP_LTE) fprintf(file, "    setle %%al\n");
            else if (node->data.bin_op.op == OP_GTE) fprintf(file, "    setge %%al\n");
            fprintf(file, "    movzbq %%al, %%rax\n");
        }
    }
}

/**
 * @brief نقطة الدخول الرئيسية لتوليد الكود للعقدة (دالة، جملة، إلخ).
 */
void codegen(Node* node, FILE* file) {
    if (node == NULL) return;

    if (node->type == NODE_PROGRAM) {
        // 1. قسم البيانات الثابتة (سلاسل النصوص وصيغ الطباعة)
        fprintf(file, ".section .rdata,\"dr\"\n");
        fprintf(file, "fmt_int: .asciz \"%%d\\n\"\n");
        fprintf(file, "fmt_str: .asciz \"%%s\\n\"\n");
        
        // 2. قسم البيانات العامة (Globals)
        fprintf(file, ".data\n");
        Node* decl = node->data.program.declarations;
        while (decl != NULL) {
            if (decl->type == NODE_VAR_DECL && decl->data.var_decl.is_global) {
                int init_value = 0;
                if (decl->data.var_decl.expression != NULL && decl->data.var_decl.expression->type == NODE_INT) {
                    init_value = decl->data.var_decl.expression->data.integer.value;
                }
                fprintf(file, "%s: .quad %d\n", decl->data.var_decl.name, init_value);
                add_global(decl->data.var_decl.name);
            }
            decl = decl->next;
        }

        // 3. قسم التعليمات البرمجية (Text Section)
        fprintf(file, ".text\n");
        decl = node->data.program.declarations;
        while (decl != NULL) {
            if (decl->type == NODE_FUNC_DEF) codegen(decl, file);
            decl = decl->next;
        }

        // 4. كتابة جدول النصوص في نهاية الملف
        fprintf(file, "\n.section .rdata,\"dr\"\n");
        for(int i=0; i<string_count; i++) {
            fprintf(file, ".Lstr_%d: .asciz \"%s\"\n", string_table[i].id, string_table[i].content);
        }
    }
    // تعريف الدالة
    else if (node->type == NODE_FUNC_DEF) {
        enter_function_scope();
        // التعامل مع دالة "الرئيسية" كنقطة دخول main
        if (strcmp(node->data.func_def.name, "الرئيسية") == 0) fprintf(file, ".globl main\nmain:\n");
        else fprintf(file, "%s:\n", node->data.func_def.name);

        // مقدمة الدالة (Prologue)
        fprintf(file, "    push %%rbp\n");
        fprintf(file, "    mov %%rsp, %%rbp\n");
        fprintf(file, "    sub $264, %%rsp\n"); // حجز مساحة للإطارات المتداخلة (256 + محاذاة)

        // تفريغ المعاملات إلى المكدس (Spill Parameters)
        Node* param = node->data.func_def.params;
        const char* regs[] = {"%%rcx", "%%rdx", "%%r8", "%%r9"};
        int p_idx = 0;
        while (param != NULL) {
            add_local(param->data.var_decl.name, 1); // المعاملات حجمها 1
            int offset = lookup_symbol(param->data.var_decl.name)->offset;
            if (p_idx < 4) fprintf(file, "    mov %s, %d(%%rbp)\n", regs[p_idx], offset);
            param = param->next;
            p_idx++;
        }
        
        // توليد كود جسم الدالة
        codegen(node->data.func_def.body, file);
        
        // خاتمة الدالة (Epilogue)
        fprintf(file, "    mov $0, %%rax\n"); // القيمة الافتراضية للإرجاع
        fprintf(file, "    leave\n    ret\n");
    }
    else if (node->type == NODE_BLOCK) {
        Node* stmt = node->data.block.statements;
        while (stmt != NULL) { codegen(stmt, file); stmt = stmt->next; }
    }
    // تعريف متغير محلي (حجم 1)
    else if (node->type == NODE_VAR_DECL) {
        gen_expr(node->data.var_decl.expression, file);
        add_local(node->data.var_decl.name, 1);
        int offset = lookup_symbol(node->data.var_decl.name)->offset;
        fprintf(file, "    mov %%rax, %d(%%rbp)\n", offset);
    }
    // تعريف مصفوفة (حجز مساحة فقط في جدول الرموز)
    else if (node->type == NODE_ARRAY_DECL) {
        // حجز مساحة المصفوفة في جدول الرموز
        add_local(node->data.array_decl.name, node->data.array_decl.size);
    }
    // تعيين مصفوفة (كتابة)
    else if (node->type == NODE_ARRAY_ASSIGN) {
        Symbol* sym = lookup_symbol(node->data.array_op.name);
        
        // 1. حساب القيمة وحفظها في المكدس
        gen_expr(node->data.array_op.value, file);
        fprintf(file, "    push %%rax\n");
        
        // 2. حساب الفهرس ونقله إلى RCX
        gen_expr(node->data.array_op.index, file);
        fprintf(file, "    mov %%rax, %%rcx\n");
        
        // 3. استرجاع القيمة إلى RAX
        fprintf(file, "    pop %%rax\n");
        
        // 4. حساب العنوان: RBP + Offset - (Index * 8)
        fprintf(file, "    imul $8, %%rcx\n");
        fprintf(file, "    neg %%rcx\n");
        fprintf(file, "    add $%d, %%rcx\n", sym->offset);
        
        // التخزين في الذاكرة
        fprintf(file, "    mov %%rax, (%%rbp, %%rcx, 1)\n");
    }
    else if (node->type == NODE_ASSIGN) {
        gen_expr(node->data.assign_stmt.expression, file);
        Symbol* sym = lookup_symbol(node->data.assign_stmt.name);
        if (sym->scope == SCOPE_LOCAL) fprintf(file, "    mov %%rax, %d(%%rbp)\n", sym->offset);
        else fprintf(file, "    mov %%rax, %s(%%rip)\n", sym->name);
    }
    else if (node->type == NODE_CALL_STMT) {
        // معاملة الاستدعاء كتعبير ولكن بدون استخدام القيمة الناتجة
        Node temp; temp.type = NODE_CALL_EXPR; temp.data.call = node->data.call;
        gen_expr(&temp, file);
    }
    else if (node->type == NODE_RETURN) {
        gen_expr(node->data.return_stmt.expression, file);
        fprintf(file, "    leave\n    ret\n");
    }
    else if (node->type == NODE_PRINT) {
        gen_expr(node->data.print_stmt.expression, file);
        fprintf(file, "    mov %%rax, %%rdx\n");
        // اختيار صيغة الطباعة المناسبة بناءً على النوع المستنتج
        if (node->data.print_stmt.expression->type == NODE_STRING) fprintf(file, "    lea fmt_str(%%rip), %%rcx\n");
        else fprintf(file, "    lea fmt_int(%%rip), %%rcx\n");
        fprintf(file, "    sub $32, %%rsp\n");
        fprintf(file, "    call printf\n");
        fprintf(file, "    add $32, %%rsp\n");
    }
    else if (node->type == NODE_IF) {
        int label_end = label_counter++;
        gen_expr(node->data.if_stmt.condition, file);
        fprintf(file, "    cmp $0, %%rax\n");
        fprintf(file, "    je .Lend_%d\n", label_end);
        codegen(node->data.if_stmt.then_branch, file);
        fprintf(file, ".Lend_%d:\n", label_end);
    }
    else if (node->type == NODE_WHILE) {
        int label_start = label_counter++;
        int label_end = label_counter++;
        fprintf(file, ".Lstart_%d:\n", label_start);
        gen_expr(node->data.while_stmt.condition, file);
        fprintf(file, "    cmp $0, %%rax\n");
        fprintf(file, "    je .Lend_%d\n", label_end);
        codegen(node->data.while_stmt.body, file);
        fprintf(file, "    jmp .Lstart_%d\n", label_start);
        fprintf(file, ".Lend_%d:\n", label_end);
    }
    // جملة التكرار "لكل" (For Loop) - جديد
    else if (node->type == NODE_FOR) {
        int label_start = label_counter++;
        int label_end = label_counter++;

        // 1. التهيئة (Initialization)
        if (node->data.for_stmt.init) {
            // يمكن أن تكون تعريف متغير أو تعيين
            if (node->data.for_stmt.init->type == NODE_VAR_DECL) {
                // تكرار منطق NODE_VAR_DECL
                gen_expr(node->data.for_stmt.init->data.var_decl.expression, file);
                add_local(node->data.for_stmt.init->data.var_decl.name, 1);
                int offset = lookup_symbol(node->data.for_stmt.init->data.var_decl.name)->offset;
                fprintf(file, "    mov %%rax, %d(%%rbp)\n", offset);
            } 
            else if (node->data.for_stmt.init->type == NODE_ASSIGN) {
                codegen(node->data.for_stmt.init, file);
            }
            else {
                gen_expr(node->data.for_stmt.init, file);
            }
        }

        fprintf(file, ".Lstart_%d:\n", label_start);

        // 2. الشرط (Condition)
        if (node->data.for_stmt.condition) {
            gen_expr(node->data.for_stmt.condition, file);
            fprintf(file, "    cmp $0, %%rax\n");
            fprintf(file, "    je .Lend_%d\n", label_end);
        }

        // 3. الجسم (Body)
        codegen(node->data.for_stmt.body, file);

        // 4. الزيادة (Increment)
        if (node->data.for_stmt.increment) {
            // قد تكون تعيين أو عملية لاحقة (++)
            if (node->data.for_stmt.increment->type == NODE_ASSIGN) {
                codegen(node->data.for_stmt.increment, file);
            } else {
                // إذا كانت تعبير (مثل س++)
                gen_expr(node->data.for_stmt.increment, file);
            }
        }

        fprintf(file, "    jmp .Lstart_%d\n", label_start);
        fprintf(file, ".Lend_%d:\n", label_end);
    }
}