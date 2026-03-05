// ============================================================================
// IR Printing (for debugging with --dump-ir)
// ============================================================================

static const char* ir_print_sep(int use_arabic) {
    return use_arabic ? "، " : ", ";
}

// Print any ASCII digits in a UTF-8 label/number as Arabic-Indic digits.
// This does not attempt to parse UTF-8; it only substitutes bytes '0'..'9'.
static void ir_print_ascii_digits_as_arabic(FILE* out, const char* s) {
    if (!out || !s) return;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c >= '0' && c <= '9') {
            const char* d = arabic_digits[c - '0'];
            fputs(d, out);
        } else {
            fputc((int)c, out);
        }
        s++;
    }
}

static void ir_print_int64(FILE* out, int64_t v, int use_arabic) {
    if (!use_arabic) {
        fprintf(out, "%lld", (long long)v);
        return;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", (long long)v);
    ir_print_ascii_digits_as_arabic(out, buf);
}

static void ir_print_label(FILE* out, const char* label, int use_arabic) {
    if (!label) {
        fputs(use_arabic ? "كتلة_؟" : "block?", out);
        return;
    }
    if (!use_arabic) {
        fputs(label, out);
        return;
    }
    ir_print_ascii_digits_as_arabic(out, label);
}

static void ir_type_print(IRType* type, FILE* out, int use_arabic) {
    if (!type) {
        fputs(use_arabic ? "فراغ" : "void", out);
        return;
    }

    switch (type->kind) {
        case IR_TYPE_PTR:
            if (use_arabic) {
                fputs("مؤشر[", out);
                ir_type_print(type->data.pointee, out, use_arabic);
                fputs("]", out);
            } else {
                fputs("ptr<", out);
                ir_type_print(type->data.pointee, out, use_arabic);
                fputs(">", out);
            }
            return;

        case IR_TYPE_ARRAY: {
            if (use_arabic) {
                fputs("مصفوفة[", out);
                ir_type_print(type->data.array.element, out, use_arabic);
                fputs("، ", out);
                char num_buf[64];
                fputs(int_to_arabic_numerals(type->data.array.count, num_buf), out);
                fputs("]", out);
            } else {
                fprintf(out, "array<");
                ir_type_print(type->data.array.element, out, use_arabic);
                fprintf(out, ", %d>", type->data.array.count);
            }
            return;
        }

        case IR_TYPE_FUNC: {
            if (use_arabic) fputs("دالة(", out);
            else fputs("func(", out);

            const char* sep = ir_print_sep(use_arabic);
            for (int i = 0; i < type->data.func.param_count; i++) {
                if (i > 0) fputs(sep, out);
                ir_type_print(type->data.func.params[i], out, use_arabic);
            }

            fputs(") -> ", out);
            ir_type_print(type->data.func.ret, out, use_arabic);
            return;
        }

        default:
            // Primitive types use the existing conversion helpers.
            fputs(use_arabic ? ir_type_to_arabic(type) : ir_type_to_english(type), out);
            return;
    }
}

static int ir_param_index_for_reg(IRFunc* func, int reg_num) {
    if (!func) return -1;
    for (int i = 0; i < func->param_count; i++) {
        if (func->params[i].reg == reg_num) return i;
    }
    return -1;
}

// Used to print parameter registers as %معامل<n> (instead of %م<n>) when possible.
static IRFunc* g_ir_print_current_func = NULL;

static void ir_print_dest_reg(FILE* out, int reg_num, int use_arabic) {
    char num_buf[64];
    fputc('%', out);
    if (use_arabic) {
        fputs("م", out);
        fputs(int_to_arabic_numerals(reg_num, num_buf), out);
    } else {
        fprintf(out, "r%d", reg_num);
    }
}

static void ir_value_print_ex(IRValue* val, FILE* out, int use_arabic) {
    if (!val) {
        fputs("null", out);
        return;
    }

    char num_buf[64];

    switch (val->kind) {
        case IR_VAL_NONE:
            fputs(use_arabic ? "فراغ" : "void", out);
            return;

        case IR_VAL_REG: {
            int reg = val->data.reg_num;
            int param_index = ir_param_index_for_reg(g_ir_print_current_func, reg);

            fputc('%', out);
            if (use_arabic) {
                if (param_index >= 0) {
                    fputs("معامل", out);
                    fputs(int_to_arabic_numerals(param_index, num_buf), out);
                } else {
                    fputs("م", out);
                    fputs(int_to_arabic_numerals(reg, num_buf), out);
                }
            } else {
                if (param_index >= 0) fprintf(out, "arg%d", param_index);
                else fprintf(out, "r%d", reg);
            }
            return;
        }

        case IR_VAL_CONST_INT:
            ir_print_int64(out, val->data.const_int, use_arabic);
            return;

        case IR_VAL_CONST_STR:
            if (use_arabic) {
                fputs("@نص", out);
                fputs(int_to_arabic_numerals(val->data.const_str.id, num_buf), out);
            } else {
                fprintf(out, "@str%d", val->data.const_str.id);
            }
            return;

        case IR_VAL_BAA_STR:
            if (use_arabic) {
                fputs("@نص_باء", out);
                fputs(int_to_arabic_numerals(val->data.const_str.id, num_buf), out);
            } else {
                fprintf(out, "@bs%d", val->data.const_str.id);
            }
            return;

        case IR_VAL_BLOCK:
            fputc('%', out);
            if (val->data.block && val->data.block->label) {
                ir_print_label(out, val->data.block->label, use_arabic);
            } else if (val->data.block) {
                if (use_arabic) {
                    fputs("كتلة_", out);
                    fputs(int_to_arabic_numerals(val->data.block->id, num_buf), out);
                } else {
                    fprintf(out, "block%d", val->data.block->id);
                }
            } else {
                fputs(use_arabic ? "كتلة_؟" : "block?", out);
            }
            return;

        case IR_VAL_FUNC:
        case IR_VAL_GLOBAL:
            fputc('@', out);
            fputs(val->data.global_name ? val->data.global_name : "???", out);
            return;

        default:
            fputs("???", out);
            return;
    }
}

/**
 * Print a value
 */
void ir_value_print(IRValue* val, FILE* out, int use_arabic) {
    ir_value_print_ex(val, out, use_arabic);
}

/**
 * Print an instruction
 */
void ir_inst_print(IRInst* inst, FILE* out, int use_arabic) {
    if (!inst) return;

    const char* sep = ir_print_sep(use_arabic);

    // Print destination if it exists
    if (inst->dest >= 0) {
        fputs("    ", out);
        ir_print_dest_reg(out, inst->dest, use_arabic);
        fputs(" = ", out);
    } else {
        fputs("    ", out);
    }

    // Print opcode name
    fputs(use_arabic ? ir_op_to_arabic(inst->op) : ir_op_to_english(inst->op), out);

    // Many opcodes have custom formatting to match the IR spec.
    switch (inst->op) {
        case IR_OP_BR:
            fputc(' ', out);
            if (inst->operand_count >= 1) ir_value_print_ex(inst->operands[0], out, use_arabic);
            fputc('\n', out);
            return;

        case IR_OP_BR_COND:
            fputc(' ', out);
            if (inst->operand_count >= 1) ir_value_print_ex(inst->operands[0], out, use_arabic);
            if (inst->operand_count >= 2) { fputs(sep, out); ir_value_print_ex(inst->operands[1], out, use_arabic); }
            if (inst->operand_count >= 3) { fputs(sep, out); ir_value_print_ex(inst->operands[2], out, use_arabic); }
            fputc('\n', out);
            return;

        case IR_OP_RET:
            if (inst->operand_count == 0 || !inst->operands[0]) {
                fputc('\n', out);
                return;
            }
            fputc(' ', out);
            ir_type_print(inst->type ? inst->type : (inst->operands[0] ? inst->operands[0]->type : NULL), out, use_arabic);
            fputc(' ', out);
            ir_value_print_ex(inst->operands[0], out, use_arabic);
            fputc('\n', out);
            return;

        case IR_OP_CALL:
            fputc(' ', out);
            if (inst->call_target) {
                fputc('@', out);
                fputs(inst->call_target, out);
            } else if (inst->call_callee) {
                ir_value_print_ex(inst->call_callee, out, use_arabic);
            } else {
                fputs("???", out);
            }
            fputc('(', out);
            for (int i = 0; i < inst->call_arg_count; i++) {
                if (i > 0) fputs(sep, out);
                if (inst->call_args && inst->call_args[i]) {
                    ir_value_print_ex(inst->call_args[i], out, use_arabic);
                }
            }
            fputs(")\n", out);
            return;

        case IR_OP_PHI: {
            fputc(' ', out);
            ir_type_print(inst->type, out, use_arabic);
            fputc(' ', out);

            IRPhiEntry* entry = inst->phi_entries;
            int first = 1;
            while (entry) {
                if (!first) fputs(sep, out);
                first = 0;

                fputc('[', out);
                ir_value_print_ex(entry->value, out, use_arabic);
                fputs(sep, out);
                // Avoid allocating a temporary IRValue for printing (prevents print-time leaks).
                IRValue tmp_block;
                tmp_block.kind = IR_VAL_BLOCK;
                tmp_block.type = NULL;
                tmp_block.data.block = entry->block;
                ir_value_print_ex(&tmp_block, out, use_arabic);
                fputc(']', out);

                entry = entry->next;
            }

            fputc('\n', out);
            return;
        }

        case IR_OP_CAST: {
            fputc(' ', out);
            IRType* from_t = (inst->operand_count >= 1 && inst->operands[0]) ? inst->operands[0]->type : NULL;
            ir_type_print(from_t, out, use_arabic);
            fputc(' ', out);
            if (inst->operand_count >= 1) ir_value_print_ex(inst->operands[0], out, use_arabic);
            fputs(use_arabic ? " إلى " : " to ", out);
            ir_type_print(inst->type, out, use_arabic);
            fputc('\n', out);
            return;
        }

        case IR_OP_CMP: {
            fputc(' ', out);
            fputs(use_arabic ? ir_cmp_pred_to_arabic(inst->cmp_pred) : ir_cmp_pred_to_english(inst->cmp_pred), out);
            fputc(' ', out);

            IRType* cmp_type = NULL;
            if (inst->operand_count >= 1 && inst->operands[0]) cmp_type = inst->operands[0]->type;
            if (!cmp_type) cmp_type = IR_TYPE_I64_T;

            ir_type_print(cmp_type, out, use_arabic);
            fputc(' ', out);

            if (inst->operand_count >= 1) ir_value_print_ex(inst->operands[0], out, use_arabic);
            if (inst->operand_count >= 2) { fputs(sep, out); ir_value_print_ex(inst->operands[1], out, use_arabic); }
            fputc('\n', out);
            return;
        }

        case IR_OP_ALLOCA: {
            fputc(' ', out);

            // In IR text format, `حجز` prints the allocated value type (not the pointer result type).
            IRType* alloc_t = inst->type;
            if (alloc_t && alloc_t->kind == IR_TYPE_PTR) alloc_t = alloc_t->data.pointee;
            ir_type_print(alloc_t, out, use_arabic);
            fputc('\n', out);
            return;
        }

        case IR_OP_LOAD:
            // `%r = حمل <type>، %ptr`
            fputc(' ', out);
            ir_type_print(inst->type, out, use_arabic);
            fputs(sep, out);
            if (inst->operand_count >= 1) ir_value_print_ex(inst->operands[0], out, use_arabic);
            fputc('\n', out);
            return;

        case IR_OP_STORE: {
            // `خزن <type> <val>، %ptr`
            fputc(' ', out);
            IRType* stored_t = NULL;
            if (inst->operand_count >= 1 && inst->operands[0]) stored_t = inst->operands[0]->type;
            if (!stored_t) stored_t = IR_TYPE_I64_T;

            ir_type_print(stored_t, out, use_arabic);
            fputc(' ', out);

            if (inst->operand_count >= 1) ir_value_print_ex(inst->operands[0], out, use_arabic);
            if (inst->operand_count >= 2) { fputs(sep, out); ir_value_print_ex(inst->operands[1], out, use_arabic); }
            fputc('\n', out);
            return;
        }

        default:
            break;
    }

    // Default formatting for most typed ops:
    // <op> <type> <op0>، <op1> ...
    fputc(' ', out);
    if (inst->type && inst->type->kind != IR_TYPE_VOID) {
        ir_type_print(inst->type, out, use_arabic);
        if (inst->operand_count > 0) fputc(' ', out);
    }

    for (int i = 0; i < inst->operand_count; i++) {
        if (i > 0) fputs(sep, out);
        ir_value_print_ex(inst->operands[i], out, use_arabic);
    }

    fputc('\n', out);
}

/**
 * Print a basic block
 */
void ir_block_print(IRBlock* block, FILE* out, int use_arabic) {
    if (!block) return;

    // Print label
    if (block->label) {
        ir_print_label(out, block->label, use_arabic);
        fputs(":\n", out);
    } else {
        if (use_arabic) {
            char num_buf[64];
            fprintf(out, "كتلة_%s:\n", int_to_arabic_numerals(block->id, num_buf));
        } else {
            fprintf(out, "block%d:\n", block->id);
        }
    }

    // Print instructions
    IRInst* inst = block->first;
    while (inst) {
        ir_inst_print(inst, out, use_arabic);
        inst = inst->next;
    }
}

/**
 * Print a function
 */
void ir_func_print(IRFunc* func, FILE* out, int use_arabic) {
    if (!func) return;

    char num_buf[64];
    const char* sep = ir_print_sep(use_arabic);

    // Print function header (match spec: دالة @name(...) -> type { )
    if (use_arabic) {
        fprintf(out, "دالة @%s(", func->name ? func->name : "???");
    } else {
        fprintf(out, "func @%s(", func->name ? func->name : "???");
    }

    // Print parameters as %معامل<n>
    for (int i = 0; i < func->param_count; i++) {
        if (i > 0) fputs(sep, out);

        ir_type_print(func->params[i].type, out, use_arabic);
        fputc(' ', out);

        fputc('%', out);
        if (use_arabic) {
            fputs("معامل", out);
            fputs(int_to_arabic_numerals(i, num_buf), out);
        } else {
            fprintf(out, "arg%d", i);
        }
    }

    if (func->is_variadic) {
        if (func->param_count > 0) fputs(sep, out);
        fputs("...", out);
    }

    fputs(") -> ", out);
    ir_type_print(func->ret_type, out, use_arabic);

    if (func->is_prototype) {
        fputs(";\n\n", out);
        return;
    }

    fputs(" {\n", out);

    // Print blocks (enable param printing inside the body)
    IRFunc* prev = g_ir_print_current_func;
    g_ir_print_current_func = func;

    IRBlock* block = func->blocks;
    while (block) {
        ir_block_print(block, out, use_arabic);
        block = block->next;
    }

    g_ir_print_current_func = prev;

    fputs("}\n\n", out);
}

/**
 * Print a global variable (match spec: عام @name = <type> <value>)
 */
void ir_global_print(IRGlobal* global, FILE* out, int use_arabic) {
    if (!global) return;

    if (use_arabic) {
        if (global->is_const) fputs("ثابت ", out);
        fputs("عام @", out);
    } else {
        if (global->is_const) fputs("const ", out);
        fputs("global @", out);
    }

    fputs(global->name ? global->name : "???", out);

    fputs(" = ", out);
    ir_type_print(global->type, out, use_arabic);
    fputc(' ', out);

    if (global->type && global->type->kind == IR_TYPE_ARRAY) {
        IRType* elem_t = global->type->data.array.element;
        int count = global->type->data.array.count;
        const char* sep = ir_print_sep(use_arabic);

        fputc('{', out);
        for (int i = 0; i < count; i++) {
            if (i > 0) fputs(sep, out);

            IRValue* v = NULL;
            if (global->init_elems && i < global->init_elem_count) {
                v = global->init_elems[i];
            }

            if (v) {
                ir_value_print_ex(v, out, use_arabic);
            } else {
                (void)elem_t;
                ir_print_int64(out, 0, use_arabic);
            }
        }
        fputc('}', out);
    } else {
        if (global->init) {
            ir_value_print_ex(global->init, out, use_arabic);
        } else {
            fputs(use_arabic ? "٠" : "0", out);
        }
    }

    fputc('\n', out);
}

const char* ir_module_get_baa_string(IRModule* module, int id) {
    if (!module) return NULL;

    IRBaaStringEntry* entry = module->baa_strings;
    while (entry) {
        if (entry->id == id) {
            return entry->content;
        }
        entry = entry->next;
    }
    return NULL;
}

int ir_module_add_baa_string(IRModule* module, const char* str) {
    if (!module || !str) return -1;

    ir_module_set_current(module);

    IRBaaStringEntry* entry = module->baa_strings;
    while (entry) {
        if (entry->content && strcmp(entry->content, str) == 0) {
            return entry->id;
        }
        entry = entry->next;
    }

    IRBaaStringEntry* new_entry = (IRBaaStringEntry*)ir_alloc(sizeof(IRBaaStringEntry), _Alignof(IRBaaStringEntry));
    if (!new_entry) return -1;

    new_entry->id = module->baa_string_count++;
    new_entry->content = ir_strdup(str);
    new_entry->next = module->baa_strings;
    module->baa_strings = new_entry;

    return new_entry->id;
}

/**
 * Print entire module
 */
void ir_module_print(IRModule* module, FILE* out, int use_arabic) {
    if (!module) return;

    if (use_arabic) {
        fprintf(out, ";; نواة باء - %s\n\n", module->name ? module->name : "وحدة");
    } else {
        fprintf(out, ";; Baa IR - %s\n\n", module->name ? module->name : "module");
    }

    // Print string table
    IRStringEntry* str = module->strings;
    if (str) {
        char num_buf[64];
        if (use_arabic) fprintf(out, ";; جدول النصوص\n");
        else fprintf(out, ";; String Table\n");

        while (str) {
            if (use_arabic) {
                fputs("@نص", out);
                fputs(int_to_arabic_numerals(str->id, num_buf), out);
            } else {
                fprintf(out, "@str%d", str->id);
            }
            fprintf(out, " = \"%s\"\n", str->content ? str->content : "");
            str = str->next;
        }
        fprintf(out, "\n");
    }

    // Print globals
    IRGlobal* global = module->globals;
    if (global) {
        if (use_arabic) fprintf(out, ";; المتغيرات العامة\n");
        else fprintf(out, ";; Global Variables\n");
        while (global) {
            ir_global_print(global, out, use_arabic);
            global = global->next;
        }
        fprintf(out, "\n");
    }

    // Print functions
    IRFunc* func = module->funcs;
    while (func) {
        ir_func_print(func, out, use_arabic);
        func = func->next;
    }
}

/**
 * Dump module to file (convenience wrapper)
 */
void ir_module_dump(IRModule* module, const char* filename, int use_arabic) {
    FILE* out = fopen(filename, "w");
    if (!out) {
        fprintf(stderr, "خطأ: لا يمكن فتح الملف %s\n", filename);
        return;
    }
    ir_module_print(module, out, use_arabic);
    fclose(out);
}

// --------------------------------------------------------------------------
// Compatibility wrappers (v0.3.0.6 task names)
// --------------------------------------------------------------------------

void ir_print_inst(IRInst* inst) {
    ir_inst_print(inst, stdout, 1);
}

void ir_print_block(IRBlock* block) {
    ir_block_print(block, stdout, 1);
}

void ir_print_func(IRFunc* func) {
    ir_func_print(func, stdout, 1);
}
