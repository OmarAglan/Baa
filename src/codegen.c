/**
 * @file codegen.c
 * @brief يقوم بتوليد كود التجميع (Assembly) للتركيب المعماري x86_64 مع دعم العمليات الحسابية والمنطقية والمصفوفات وحلقات التكرار والمتغيرات النصية والتحكم في الحلقات والشروط الممتدة وجمل الاختيار.
 * @version 0.1.3 (Switch Statement)
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
    DataType type;     // نوع البيانات (صحيح أو نص)
    int offset;        // الإزاحة من مؤشر القاعدة (RBP) للمتغيرات المحلية
} Symbol;

Symbol global_symbols[100]; int global_count = 0;
Symbol local_symbols[100]; int local_count = 0; 
int current_stack_offset = 0; // يتتبع موقع الـ RSP بالنسبة للـ RBP
int label_counter = 0;        // لإنشاء تسميات فريدة للجمل الشرطية وحلقات التكرار

// --- مكدس ملصقات الحلقات (Loop Label Stack) ---
int loop_continue_stack[100]; 
int loop_break_stack[100];    
int loop_depth = 0;           

void push_loop(int continue_label, int break_label) {
    if (loop_depth >= 100) { printf("Codegen Error: Loops nested too deeply\n"); exit(1); }
    loop_continue_stack[loop_depth] = continue_label;
    loop_break_stack[loop_depth] = break_label;
    loop_depth++;
}

// دالة خاصة لدفع نطاق جملة "اختر"
// جملة الاختيار تقبل "توقف" لكنها لا تقبل "استمر" (إلا إذا كانت داخل حلقة)
// لذا، ملصق "استمر" يرث قيمته من النطاق السابق
void push_switch(int break_label) {
    if (loop_depth >= 100) { printf("Codegen Error: Nesting too deep\n"); exit(1); }
    
    // وراثة ملصق الاستمرار من الحلقة المحيطة (إن وجدت)
    // إذا لم تكن هناك حلقة (Depth 0)، نضع 0 (غير صالح)
    int current_continue = (loop_depth > 0) ? loop_continue_stack[loop_depth - 1] : 0;
    
    loop_continue_stack[loop_depth] = current_continue;
    loop_break_stack[loop_depth] = break_label;
    loop_depth++;
}

void pop_loop() {
    if (loop_depth > 0) loop_depth--;
}

int get_current_continue_label() {
    if (loop_depth == 0 || loop_continue_stack[loop_depth - 1] == 0) { 
        printf("Codegen Error: 'continue' outside of loop\n"); exit(1); 
    }
    return loop_continue_stack[loop_depth - 1];
}

int get_current_break_label() {
    if (loop_depth == 0) { printf("Codegen Error: 'break' outside of loop or switch\n"); exit(1); }
    return loop_break_stack[loop_depth - 1];
}

/**
 * @brief إضافة متغير عام لجدول الرموز.
 */
void add_global(const char* name, DataType type) {
    strcpy(global_symbols[global_count].name, name);
    global_symbols[global_count].scope = SCOPE_GLOBAL;
    global_symbols[global_count].type = type;
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
void add_local(const char* name, int size, DataType type) {
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
        // تحميل عنوان النص (Pointer)
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
        fprintf(file, "    imul $8, %%rcx\n");      // Index * 8
        fprintf(file, "    neg %%rcx\n");           // -(Index * 8)
        fprintf(file, "    add $%d, %%rcx\n", sym->offset); // Offset + (-(Index*8))
        
        // تحميل القيمة من العنوان المحسوب
        fprintf(file, "    mov (%%rbp, %%rcx, 1), %%rax\n");
    }
    else if (node->type == NODE_CALL_EXPR) {
        // استدعاء دالة: اتباع اتفاقية Windows (RCX, RDX, R8, R9)
        const char* regs[] = {"%rcx", "%rdx", "%r8", "%r9"};
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
                // تمرير نوع المتغير العام
                add_global(decl->data.var_decl.name, decl->data.var_decl.type);
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
        // حجز مساحة للإطارات المتداخلة (272 بايت)
        fprintf(file, "    sub $272, %%rsp\n"); 

        // تفريغ المعاملات إلى المكدس (Spill Parameters)
        Node* param = node->data.func_def.params;
        const char* regs[] = {"%rcx", "%rdx", "%r8", "%r9"};
        int p_idx = 0;
        while (param != NULL) {
            // تسجيل المعاملات كمتغيرات محلية مع أنواعها
            add_local(param->data.var_decl.name, 1, param->data.var_decl.type);
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
    // معالجة جملة الاختيار (Switch Case)
    else if (node->type == NODE_SWITCH) {
        int label_end = label_counter++;
        int label_default = -1; // -1 يعني لا يوجد افتراضي
        
        // تسجيل ملصق النهاية لدعم "توقف" (break)
        push_switch(label_end);

        // 1. تقييم تعبير الاختيار
        gen_expr(node->data.switch_stmt.expression, file);
        
        // 2. المرور الأول: توليد المقارنات والقفزات (Pass 1)
        Node* curr_case = node->data.switch_stmt.cases;
        // سنحتاج لتخزين معرفات الملصقات لكل حالة لاستخدامها في المرور الثاني
        // لتبسيط الأمر (بدون مصفوفات ديناميكية)، سنعتمد على عداد label_counter المتزايد
        // سنولد معرف ملصق لكل حالة ونقوم بتوليد القفزة
        
        // مشكلة: كيف نعرف نفس معرف الملصق في المرور الثاني؟
        // حل: تخزين المعرفات في قائمة مؤقتة أو مصفوفة ثابتة الحجم
        int case_labels[256]; // حد أقصى 256 حالة في الجملة الواحدة
        int case_count = 0;

        while (curr_case != NULL) {
            if (curr_case->data.case_stmt.is_default) {
                label_default = label_counter++;
                case_labels[case_count++] = label_default;
            } else {
                int lbl = label_counter++;
                case_labels[case_count++] = lbl;
                
                // مقارنة القيمة
                // نفترض أن القيمة رقم صحيح أو حرف (كلاهما int)
                int val = 0;
                if (curr_case->data.case_stmt.value->type == NODE_INT) 
                    val = curr_case->data.case_stmt.value->data.integer.value;
                else if (curr_case->data.case_stmt.value->type == NODE_CHAR)
                    val = curr_case->data.case_stmt.value->data.char_lit.value;
                
                // مقارنة %rax بالقيمة
                fprintf(file, "    cmp $%d, %%rax\n", val);
                fprintf(file, "    je .Lcase_%d\n", lbl);
            }
            curr_case = curr_case->next;
        }

        // إذا لم يحدث تطابق، اقفز للافتراضي أو النهاية
        if (label_default != -1) {
            fprintf(file, "    jmp .Lcase_%d\n", label_default);
        } else {
            fprintf(file, "    jmp .Lend_%d\n", label_end);
        }

        // 3. المرور الثاني: توليد أجسام الحالات (Pass 2)
        curr_case = node->data.switch_stmt.cases;
        int i = 0;
        while (curr_case != NULL) {
            fprintf(file, ".Lcase_%d:\n", case_labels[i]);
            
            // توليد جمل الحالة
            Node* stmt = curr_case->data.case_stmt.body;
            while (stmt != NULL) {
                codegen(stmt, file);
                stmt = stmt->next;
            }
            
            // في C، التنفيذ يكمل للحالة التالية (Fallthrough)
            // إلا إذا وجد "توقف" (break) الذي تمت معالجته في NODE_BREAK
            
            curr_case = curr_case->next;
            i++;
        }

        // ملصق النهاية
        fprintf(file, ".Lend_%d:\n", label_end);
        pop_loop();
    }
    // تعريف متغير محلي (حجم 1)
    else if (node->type == NODE_VAR_DECL) {
        gen_expr(node->data.var_decl.expression, file);
        add_local(node->data.var_decl.name, 1, node->data.var_decl.type);
        int offset = lookup_symbol(node->data.var_decl.name)->offset;
        fprintf(file, "    mov %%rax, %d(%%rbp)\n", offset);
    }
    // تعريف مصفوفة (حجز مساحة فقط في جدول الرموز)
    else if (node->type == NODE_ARRAY_DECL) {
        // المصفوفات حالياً تدعم الأرقام الصحيحة فقط (TYPE_INT)
        add_local(node->data.array_decl.name, node->data.array_decl.size, TYPE_INT);
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
        Node temp; temp.type = NODE_CALL_EXPR; temp.data.call = node->data.call;
        gen_expr(&temp, file);
    }
    else if (node->type == NODE_RETURN) {
        gen_expr(node->data.return_stmt.expression, file);
        fprintf(file, "    leave\n    ret\n");
    }
    else if (node->type == NODE_BREAK) {
        int label_break = get_current_break_label();
        fprintf(file, "    jmp .Lend_%d\n", label_break);
    }
    else if (node->type == NODE_CONTINUE) {
        int label_continue = get_current_continue_label();
        // الاستمرار يقفز إلى ملصق "التحديث" في حلقات for، أو "البداية" في while
        // نستخدم نفس التسمية .Lcontinue_X في كلا الحالتين لتوحيد المعالجة
        fprintf(file, "    jmp .Lcontinue_%d\n", label_continue);
    }
    else if (node->type == NODE_PRINT) {
        gen_expr(node->data.print_stmt.expression, file);
        fprintf(file, "    mov %%rax, %%rdx\n");
        
        // اختيار صيغة الطباعة بناءً على نوع التعبير
        if (node->data.print_stmt.expression->type == NODE_STRING) {
            fprintf(file, "    lea fmt_str(%%rip), %%rcx\n");
        } 
        else if (node->data.print_stmt.expression->type == NODE_VAR_REF) {
            Symbol* sym = lookup_symbol(node->data.print_stmt.expression->data.var_ref.name);
            if (sym->type == TYPE_STRING) fprintf(file, "    lea fmt_str(%%rip), %%rcx\n");
            else fprintf(file, "    lea fmt_int(%%rip), %%rcx\n");
        } 
        else {
            fprintf(file, "    lea fmt_int(%%rip), %%rcx\n");
        }
        
        fprintf(file, "    sub $32, %%rsp\n");
        fprintf(file, "    call printf\n");
        fprintf(file, "    add $32, %%rsp\n");
    }
    // تحديث NODE_IF لدعم وإلا (else)
    else if (node->type == NODE_IF) {
        int label_else = label_counter++;
        int label_end = label_counter++;
        
        gen_expr(node->data.if_stmt.condition, file);
        fprintf(file, "    cmp $0, %%rax\n");
        // إذا كان الشرط خطأ، اقفز إلى ملصق "وإلا"
        fprintf(file, "    je .Lelse_%d\n", label_else);
        
        // تنفيذ الفرع "إذا"
        codegen(node->data.if_stmt.then_branch, file);
        // القفز إلى النهاية لتجاوز "وإلا"
        fprintf(file, "    jmp .Lend_%d\n", label_end);
        
        // ملصق "وإلا"
        fprintf(file, ".Lelse_%d:\n", label_else);
        // تنفيذ الفرع "وإلا" إذا وجد
        if (node->data.if_stmt.else_branch) {
            codegen(node->data.if_stmt.else_branch, file);
        }
        
        // نهاية الجملة
        fprintf(file, ".Lend_%d:\n", label_end);
    }
    else if (node->type == NODE_WHILE) {
        int label_start = label_counter++;
        int label_end = label_counter++;
        
        // دفع الملصقات للمكدس (start هو مكان الاستمرار في while)
        push_loop(label_start, label_end);

        // ملصق الاستمرار في while هو نفسه البداية
        fprintf(file, ".Lcontinue_%d:\n", label_start); 
        fprintf(file, ".Lstart_%d:\n", label_start);
        
        gen_expr(node->data.while_stmt.condition, file);
        fprintf(file, "    cmp $0, %%rax\n");
        fprintf(file, "    je .Lend_%d\n", label_end);
        
        codegen(node->data.while_stmt.body, file);
        
        fprintf(file, "    jmp .Lstart_%d\n", label_start);
        fprintf(file, ".Lend_%d:\n", label_end);
        
        pop_loop();
    }
    else if (node->type == NODE_FOR) {
        int label_start = label_counter++;
        int label_end = label_counter++;
        int label_continue = label_counter++; // ملصق خاص للاستمرار (التحديث)

        // دفع الملصقات للمكدس
        push_loop(label_continue, label_end);

        if (node->data.for_stmt.init) {
            if (node->data.for_stmt.init->type == NODE_VAR_DECL) {
                gen_expr(node->data.for_stmt.init->data.var_decl.expression, file);
                add_local(node->data.for_stmt.init->data.var_decl.name, 1, node->data.for_stmt.init->data.var_decl.type);
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

        if (node->data.for_stmt.condition) {
            gen_expr(node->data.for_stmt.condition, file);
            fprintf(file, "    cmp $0, %%rax\n");
            fprintf(file, "    je .Lend_%d\n", label_end);
        }

        codegen(node->data.for_stmt.body, file);

        // نقطة القفز لجملة "استمر"
        fprintf(file, ".Lcontinue_%d:\n", label_continue);

        if (node->data.for_stmt.increment) {
            if (node->data.for_stmt.increment->type == NODE_ASSIGN) {
                codegen(node->data.for_stmt.increment, file);
            } else {
                gen_expr(node->data.for_stmt.increment, file);
            }
        }

        fprintf(file, "    jmp .Lstart_%d\n", label_start);
        fprintf(file, ".Lend_%d:\n", label_end);
        
        pop_loop();
    }
}