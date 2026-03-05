// ============================================================================
// إصدار قسم البيانات (Data Section Emission)
// ============================================================================

/**
 * @brief إصدار قسم البيانات الثابتة (صيغ الطباعة والقراءة).
 */
static void emit_rdata_section(FILE* out) {
    emit_rodata_section(out);
    fprintf(out, "fmt_int: .asciz \"%%lld\\n\"\n");
    fprintf(out, "fmt_str: .asciz \"%%s\\n\"\n");
    fprintf(out, "fmt_scan_int: .asciz \"%%lld\"\n");
}

/**
 * @brief إصدار قسم المتغيرات العامة.
 */
static const char* emit_data_dir_for_type(IRType* t, int* out_size_bytes) {
    if (out_size_bytes) *out_size_bytes = 8;
    if (!t) return ".quad";

    switch (t->kind) {
        case IR_TYPE_I1:
        case IR_TYPE_I8:
        case IR_TYPE_U8:
            if (out_size_bytes) *out_size_bytes = 1;
            return ".byte";
        case IR_TYPE_I16:
        case IR_TYPE_U16:
            if (out_size_bytes) *out_size_bytes = 2;
            return ".word";
        case IR_TYPE_I32:
        case IR_TYPE_U32:
            if (out_size_bytes) *out_size_bytes = 4;
            return ".long";
        case IR_TYPE_I64:
        case IR_TYPE_U64:
        case IR_TYPE_CHAR:
        case IR_TYPE_F64:
        case IR_TYPE_PTR:
        default:
            if (out_size_bytes) *out_size_bytes = 8;
            return ".quad";
    }
}

static void emit_data_section(MachineModule* module, FILE* out) {
    if (!module || module->global_count == 0) return;

    fprintf(out, "\n.data\n");

    for (IRGlobal* g = module->globals; g; g = g->next) {
        if (!g->name) continue;

        if (g->is_internal) {
            if (g_emit_target && g_emit_target->obj_format == BAA_OBJFORMAT_ELF) {
                fprintf(out, ".local %s\n", g->name);
            }
        } else {
            fprintf(out, ".globl %s\n", g->name);
        }

        // مصفوفة عامة
        if (g->type && g->type->kind == IR_TYPE_ARRAY) {
            IRType* elem_t = g->type->data.array.element;
            int count = g->type->data.array.count;
            int elem_size = 8;
            const char* dir = emit_data_dir_for_type(elem_t, &elem_size);
            if (count < 0) count = 0;

            // إن كانت التهيئة كلها أصفار، نستخدم .zero لتقليل حجم النص.
            int all_zero = 1;
            if (g->init_elems) {
                for (int i = 0; i < g->init_elem_count; i++) {
                    IRValue* v = g->init_elems[i];
                    if (!v) continue;
                    if (v->kind == IR_VAL_CONST_INT) {
                        if (v->data.const_int != 0) { all_zero = 0; break; }
                    } else if (v->kind == IR_VAL_CONST_STR || v->kind == IR_VAL_BAA_STR) {
                        all_zero = 0;
                        break;
                    } else if (v->kind == IR_VAL_FUNC) {
                        all_zero = 0;
                        break;
                    } else {
                        all_zero = 0;
                        break;
                    }
                }
            }

            if (all_zero) {
                int total = count * elem_size;
                if (total < 0) total = 0;
                fprintf(out, "%s: .zero %d\n", g->name, total);
            } else {
                fprintf(out, "%s:\n", g->name);
                for (int i = 0; i < count; i++) {
                    if (g->init_elems && i < g->init_elem_count && g->init_elems[i]) {
                        IRValue* iv = g->init_elems[i];
                        if (iv->kind == IR_VAL_CONST_INT) {
                            fprintf(out, "    %s %lld\n", dir, (long long)iv->data.const_int);
                            continue;
                        }
                        if (iv->kind == IR_VAL_CONST_STR && elem_t && elem_t->kind == IR_TYPE_PTR) {
                            fprintf(out, "    .quad .Lstr_%d\n", iv->data.const_str.id);
                            continue;
                        }
                        if (iv->kind == IR_VAL_BAA_STR && elem_t && elem_t->kind == IR_TYPE_PTR) {
                            fprintf(out, "    .quad .Lbs_%d\n", iv->data.const_str.id);
                            continue;
                        }
                        if (iv->kind == IR_VAL_FUNC && elem_t && elem_t->kind == IR_TYPE_FUNC) {
                            MachineOperand mop = {0};
                            mop.kind = MACH_OP_FUNC;
                            mop.data.name = iv->data.global_name;
                            fprintf(out, "    .quad ");
                            emit_operand(&mop, out);
                            fprintf(out, "\n");
                            continue;
                        }
                    }
                    fprintf(out, "    %s 0\n", dir);
                }
            }

            continue;
        }

        // إصدار القيمة الابتدائية
        if (g->init) {
            int sz = 8;
            const char* dir = emit_data_dir_for_type(g->type, &sz);

            if (g->init->kind == IR_VAL_CONST_INT) {
                fprintf(out, "%s: %s %lld\n", g->name, dir,
                        (long long)g->init->data.const_int);
            } else if (g->init->kind == IR_VAL_CONST_STR) {
                // C-String: تخزين مؤشر لسلسلة في جدول النصوص
                fprintf(out, "%s: .quad .Lstr_%d\n", g->name,
                        g->init->data.const_str.id);
            } else if (g->init->kind == IR_VAL_BAA_STR) {
                // نص باء: تخزين مؤشر لسلسلة حرف[] في جدول نصوص باء
                fprintf(out, "%s: .quad .Lbs_%d\n", g->name,
                        g->init->data.const_str.id);
            } else if (g->init->kind == IR_VAL_FUNC) {
                // مؤشر دالة: تخزين عنوان الدالة كـ .quad <symbol>
                MachineOperand mop = {0};
                mop.kind = MACH_OP_FUNC;
                mop.data.name = g->init->data.global_name;
                fprintf(out, "%s: .quad ", g->name);
                emit_operand(&mop, out);
                fprintf(out, "\n");
            } else {
                fprintf(out, "%s: %s 0\n", g->name, dir);
            }
        } else {
            int sz = 8;
            const char* dir = emit_data_dir_for_type(g->type, &sz);
            fprintf(out, "%s: %s 0\n", g->name, dir);
        }
    }
}

/**
 * @brief تهريب سلسلة نصية لتكون صالحة داخل ".asciz" في GAS.
 *
 * GAS يدعم تهريب C داخل السلاسل النصية (مثل \n و \t).
 * نحن نحتاج تهريب المحارف التي قد تكسر ملف التجميع:
 * - علامة الاقتباس "
 * - الشرطة العكسية \
 * - أسطر جديدة/تبويب/عودة عربة
 * - المحارف غير القابلة للطباعة (نستخدم \xHH)
 */
static void emit_gas_escaped_string(FILE* out, const char* s) {
    if (!out || !s) return;

    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '\"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\t': fputs("\\t", out); break;
            case '\r': fputs("\\r", out); break;
            default:
                if (c < 0x20 || c == 0x7F) {
                    fprintf(out, "\\x%02X", (unsigned)c);
                } else {
                    fputc((int)c, out);
                }
                break;
        }
    }
}

/**
 * @brief إصدار جدول النصوص (String Table).
 */
static void emit_string_table(MachineModule* module, FILE* out) {
    if (!module || module->string_count == 0) return;

    fprintf(out, "\n");
    emit_rodata_section(out);

    for (IRStringEntry* s = module->strings; s; s = s->next) {
        if (!s->content) continue;

        fprintf(out, ".Lstr_%d: .asciz \"", s->id);
        emit_gas_escaped_string(out, s->content);
        fprintf(out, "\"\n");
    }
}

static void emit_baa_string_table(MachineModule* module, FILE* out) {
    if (!module || module->baa_string_count == 0) return;

    fprintf(out, "\n");
    emit_rodata_section(out);

    for (IRBaaStringEntry* s = module->baa_strings; s; s = s->next) {
        if (!s->content) continue;

        // محاذاة 8 بايت لأن العناصر .quad
        fprintf(out, "    .p2align 3\n");
        fprintf(out, ".Lbs_%d:\n", s->id);

        const unsigned char* p = (const unsigned char*)s->content;
        while (*p) {
            unsigned char b0 = p[0];
            int len = 0;
            if ((b0 & 0x80u) == 0x00u) len = 1;
            else if ((b0 & 0xE0u) == 0xC0u) len = 2;
            else if ((b0 & 0xF0u) == 0xE0u) len = 3;
            else if ((b0 & 0xF8u) == 0xF0u) len = 4;
            else len = 0;

            unsigned char bytes[4] = {0, 0, 0, 0};

            bool ok = true;
            if (len <= 0) {
                ok = false;
            } else {
                for (int i = 0; i < len; i++) {
                    unsigned char bi = p[i];
                    if (bi == 0) { ok = false; break; }
                    if (i > 0 && ((bi & 0xC0u) != 0x80u)) { ok = false; break; }
                    bytes[i] = bi;
                }
            }

            if (!ok) {
                // U+FFFD (EF BF BD)
                bytes[0] = 0xEF;
                bytes[1] = 0xBF;
                bytes[2] = 0xBD;
                bytes[3] = 0x00;
                len = 3;
                // تقدم بايتاً واحداً لتجنب تعليق الحلقة
                p++;
            } else {
                p += (size_t)len;
            }

            uint64_t bytes_field = (uint64_t)bytes[0] |
                                   ((uint64_t)bytes[1] << 8) |
                                   ((uint64_t)bytes[2] << 16) |
                                   ((uint64_t)bytes[3] << 24);
            uint64_t packed = bytes_field | ((uint64_t)(unsigned)len << 32);

            fprintf(out, "    .quad %llu\n", (unsigned long long)packed);
        }

        // النهاية: حرف بطول 0
        fprintf(out, "    .quad 0\n");
    }
}

// ============================================================================
// إصدار دالة كاملة (Function Emission)
// ============================================================================

/**
 * @brief جمع السجلات المحفوظة (callee-saved) المستخدمة في الدالة.
 *
 * يمشي على جميع تعليمات الدالة ويكشف أي سجلات callee-saved
 * استُخدمت كوجهة (dst) حتى نحفظها في المقدمة ونستعيدها في الخاتمة.
 */
