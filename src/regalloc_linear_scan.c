// ============================================================================
// مقارنة فترات الحيوية للترتيب
// ============================================================================

/**
 * @brief مقارنة فترتي حيوية حسب نقطة البداية.
 */
static int interval_cmp_start(const void *a, const void *b)
{
    const LiveInterval *ia = (const LiveInterval *)a;
    const LiveInterval *ib = (const LiveInterval *)b;
    if (ia->start != ib->start)
        return ia->start - ib->start;
    return ia->vreg - ib->vreg;
}

/**
 * @brief هل الفترة تعبر موقع CALL؟
 * @param li مؤشر إلى فترة الحيوية
 * @param call_pos مصفوفة مواقع CALL
 * @param call_count عدد مواقع CALL
 * @return true إذا كانت الفترة تعبر CALL
 */
static bool interval_crosses_call(const LiveInterval *li, const int *call_pos, int call_count)
{
    if (!li || !call_pos || call_count == 0)
        return false;
    for (int c = 0; c < call_count; c++)
    {
        int p = call_pos[c];
        if (li->start < p && li->end > p)
            return true;
    }
    return false;
}

// ============================================================================
// خوارزمية المسح الخطي (Linear Scan Algorithm)
// ============================================================================

/**
 * @struct ActiveInterval
 * @brief عنصر في قائمة الفترات النشطة مرتبة حسب نقطة النهاية.
 */
typedef struct
{
    int interval_idx; // فهرس في مصفوفة الفترات
    PhysReg reg;      // السجل الفيزيائي المخصص
} ActiveEntry;

void regalloc_linear_scan(RegAllocCtx *ctx)
{
    if (!ctx || ctx->interval_count == 0)
        return;

    // ترتيب الفترات حسب نقطة البداية
    qsort(ctx->intervals, ctx->interval_count,
          sizeof(LiveInterval), interval_cmp_start);

    // قائمة الفترات النشطة
    ActiveEntry *active = malloc(ctx->interval_count * sizeof(ActiveEntry));
    if (!active)
        return;
    int active_count = 0;

    // بناء قائمة مواقع CALL (حسب ترتيب inst_map)
    int *call_pos = NULL;
    int call_count = 0;
    if (ctx->inst_map && ctx->total_insts > 0)
    {
        call_pos = malloc((size_t)ctx->total_insts * sizeof(int));
        if (call_pos)
        {
            for (int p = 0; p < ctx->total_insts; p++)
            {
                MachineInst *inst = ctx->inst_map[p];
                if (inst && (inst->op == MACH_CALL || inst->op == MACH_INLINE_ASM))
                {
                    call_pos[call_count++] = p;
                }
            }
        }
    }

    // مصفوفة تتبع توفر السجلات الفيزيائية
    bool reg_free[PHYS_REG_COUNT];
    for (int i = 0; i < PHYS_REG_COUNT; i++)
        reg_free[i] = true;

    // RSP و RBP محجوزان دائماً
    reg_free[PHYS_RSP] = false;
    reg_free[PHYS_RBP] = false;

    // سجلات ABI الثابتة لا نخصصها لفترات عادية:
    // - سجل الإرجاع
    // - سجلات معاملات الاستدعاء
    // لأننا نستخدم vregs سالبة لفرض هذه السجلات.
    const BaaCallingConv* cc = ctx && ctx->cc ? ctx->cc : baa_target_builtin_windows_x86_64()->cc;
    if (cc)
    {
        if (cc->ret_phys_reg >= 0 && cc->ret_phys_reg < PHYS_REG_COUNT)
            reg_free[cc->ret_phys_reg] = false;
        for (int i = 0; i < cc->int_arg_reg_count; i++)
        {
            int r = cc->int_arg_phys_regs[i];
            if (r >= 0 && r < PHYS_REG_COUNT)
                reg_free[r] = false;
        }
    }

    // المسح الخطي
    for (int i = 0; i < ctx->interval_count; i++)
    {
        LiveInterval *cur = &ctx->intervals[i];
        bool cur_crosses_call = interval_crosses_call(cur, call_pos, call_count);

        // 1. انتهاء الفترات النشطة التي انتهت قبل نقطة بداية الفترة الحالية
        int new_active_count = 0;
        for (int j = 0; j < active_count; j++)
        {
            LiveInterval *act = &ctx->intervals[active[j].interval_idx];
            if (act->end < cur->start)
            {
                // الفترة انتهت: تحرير السجل
                reg_free[active[j].reg] = true;
            }
            else
            {
                // الفترة لا تزال نشطة
                active[new_active_count++] = active[j];
            }
        }
        active_count = new_active_count;

        // 2. محاولة تخصيص سجل فيزيائي
        PhysReg assigned = PHYS_NONE;

        // البحث عن سجل متاح بترتيب الأفضلية
        for (int k = 0; k < alloc_order_count; k++)
        {
            PhysReg r = alloc_order[k];

            // إذا كانت الفترة تعبر CALL، يجب أن تكون في سجل callee-saved
            // حتى لا تُدمّر قيمتها أثناء الاستدعاء.
            if (cur_crosses_call && reg_is_caller_saved_cc(cc, r))
            {
                continue;
            }

            if (reg_free[r])
            {
                assigned = r;
                reg_free[r] = false;
                break;
            }
        }

        if (assigned != PHYS_NONE)
        {
            // نجح التخصيص
            cur->phys_reg = assigned;
            cur->spilled = false;

            // تسجيل استخدام سجل callee-saved
            if (reg_is_callee_saved_cc(cc, assigned))
            {
                ctx->callee_saved_used[assigned] = true;
            }

            // إضافة إلى القائمة النشطة (مرتبة حسب نقطة النهاية)
            int insert_pos = active_count;
            for (int j = 0; j < active_count; j++)
            {
                if (ctx->intervals[active[j].interval_idx].end > cur->end)
                {
                    insert_pos = j;
                    break;
                }
            }
            // إزاحة العناصر
            for (int j = active_count; j > insert_pos; j--)
            {
                active[j] = active[j - 1];
            }
            active[insert_pos].interval_idx = i;
            active[insert_pos].reg = assigned;
            active_count++;
        }
        else
        {
            // 3. لا سجل متاح: تسريب
            // نسرّب الفترة الأطول (بما في ذلك الحالية)
            int spill_idx = i; // افتراضياً نسرّب الحالية
            int longest_end = cur->end;

            for (int j = 0; j < active_count; j++)
            {
                LiveInterval *act = &ctx->intervals[active[j].interval_idx];

                // إذا كانت الفترة الحالية تعبر CALL، نحتاج تحرير سجل callee-saved فقط.
                // تحرير سجل caller-saved (مثل r10/r11) لن يحل المشكلة.
                if (cur_crosses_call && reg_is_caller_saved_cc(cc, active[j].reg))
                {
                    continue;
                }

                if (act->end > longest_end)
                {
                    longest_end = act->end;
                    spill_idx = active[j].interval_idx;
                }
            }

            if (spill_idx != i)
            {
                // تسريب فترة نشطة بدلاً من الحالية
                LiveInterval *to_spill = &ctx->intervals[spill_idx];
                PhysReg freed_reg = to_spill->phys_reg;

                // تسريب الفترة القديمة
                to_spill->spilled = true;
                to_spill->phys_reg = PHYS_NONE;
                ctx->next_spill_offset += 8;
                to_spill->spill_offset = -(int)ctx->next_spill_offset;
                ctx->spill_count++;

                // تحديث خرائط النتائج
                if (to_spill->vreg >= 0 && to_spill->vreg < ctx->max_vreg)
                {
                    ctx->vreg_spilled[to_spill->vreg] = true;
                    ctx->vreg_spill_offset[to_spill->vreg] = to_spill->spill_offset;
                    ctx->vreg_to_phys[to_spill->vreg] = PHYS_NONE;
                }

                // إزالته من القائمة النشطة
                int remove_pos = -1;
                for (int j = 0; j < active_count; j++)
                {
                    if (active[j].interval_idx == spill_idx)
                    {
                        remove_pos = j;
                        break;
                    }
                }
                if (remove_pos >= 0)
                {
                    for (int j = remove_pos; j < active_count - 1; j++)
                    {
                        active[j] = active[j + 1];
                    }
                    active_count--;
                }

                // تخصيص السجل المحرر للفترة الحالية
                cur->phys_reg = freed_reg;
                cur->spilled = false;

                if (reg_is_callee_saved_cc(cc, freed_reg))
                {
                    ctx->callee_saved_used[freed_reg] = true;
                }

                // إضافة الحالية إلى القائمة النشطة
                int insert_pos = active_count;
                for (int j = 0; j < active_count; j++)
                {
                    if (ctx->intervals[active[j].interval_idx].end > cur->end)
                    {
                        insert_pos = j;
                        break;
                    }
                }
                for (int j = active_count; j > insert_pos; j--)
                {
                    active[j] = active[j - 1];
                }
                active[insert_pos].interval_idx = i;
                active[insert_pos].reg = freed_reg;
                active_count++;
            }
            else
            {
                // تسريب الفترة الحالية
                cur->spilled = true;
                cur->phys_reg = PHYS_NONE;
                ctx->next_spill_offset += 8;
                cur->spill_offset = -(int)ctx->next_spill_offset;
                ctx->spill_count++;
            }
        }

        // تحديث خرائط النتائج
        if (cur->vreg >= 0 && cur->vreg < ctx->max_vreg)
        {
            ctx->vreg_to_phys[cur->vreg] = cur->phys_reg;
            ctx->vreg_spilled[cur->vreg] = cur->spilled;
            if (cur->spilled)
            {
                ctx->vreg_spill_offset[cur->vreg] = cur->spill_offset;
            }
        }
    }

    // تحديث حجم المكدس
    ctx->func->stack_size = ctx->next_spill_offset;

    free(call_pos);
    free(active);
}

