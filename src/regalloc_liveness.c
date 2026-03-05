// ============================================================================
// حساب الحيوية (Liveness: live-in / live-out)
// ============================================================================

void regalloc_compute_liveness(RegAllocCtx *ctx)
{
    if (!ctx || !ctx->block_live)
        return;

    int words = ctx->bitset_words;
    int bc = ctx->block_count;

    // نحتاج مصفوفة مؤقتة للحسابات
    uint64_t *temp = bitset_alloc(words);
    if (!temp)
        return;

    // بناء خريطة block_id → فهرس في block_live
    // وخريطة الكتل الآلية
    MachineBlock **block_arr = malloc(bc * sizeof(MachineBlock *));
    if (!block_arr)
    {
        free(temp);
        return;
    }

    int bi = 0;
    for (MachineBlock *b = ctx->func->blocks; b; b = b->next)
    {
        if (bi < bc)
            block_arr[bi] = b;
        bi++;
    }

    // التكرار حتى الثبات (fixpoint)
    bool changed = true;
    int iterations = 0;
    while (changed && iterations < 100)
    {
        changed = false;
        iterations++;

        // المشي على الكتل بترتيب عكسي (أفضل للتقارب)
        for (int i = bc - 1; i >= 0; i--)
        {
            BlockLiveness *bl = &ctx->block_live[i];
            MachineBlock *mb = block_arr[i];

            // live_out = اتحاد live_in لكل خلف
            for (int s = 0; s < mb->succ_count; s++)
            {
                MachineBlock *succ = mb->succs[s];
                if (!succ)
                    continue;

                // إيجاد فهرس الخلف في المصفوفة
                for (int j = 0; j < bc; j++)
                {
                    if (block_arr[j] == succ)
                    {
                        changed |= bitset_union(bl->live_out,
                                                ctx->block_live[j].live_in, words);
                        break;
                    }
                }
            }

            // live_in = use ∪ (live_out - def)
            bitset_diff(temp, bl->live_out, bl->def, words);
            bitset_union(temp, bl->use, words);

            // التحقق من التغيير
            for (int w = 0; w < words; w++)
            {
                if (bl->live_in[w] != temp[w])
                {
                    changed = true;
                    bl->live_in[w] = temp[w];
                }
            }
        }
    }

    free(temp);
    free(block_arr);
}

// ============================================================================
// بناء فترات الحيوية (Build Live Intervals)
// ============================================================================

/**
 * @brief إضافة أو تحديث فترة حيوية لسجل افتراضي.
 */
static void update_interval(RegAllocCtx *ctx, int vreg, int pos)
{
    if (vreg < 0 || vreg >= ctx->max_vreg)
        return;

    // البحث عن فترة موجودة
    for (int i = 0; i < ctx->interval_count; i++)
    {
        if (ctx->intervals[i].vreg == vreg)
        {
            if (pos < ctx->intervals[i].start)
                ctx->intervals[i].start = pos;
            if (pos > ctx->intervals[i].end)
                ctx->intervals[i].end = pos;
            return;
        }
    }

    // إنشاء فترة جديدة
    if (ctx->interval_count >= ctx->interval_capacity)
    {
        int new_cap = ctx->interval_capacity ? ctx->interval_capacity * 2 : 32;
        LiveInterval *new_arr = realloc(ctx->intervals, new_cap * sizeof(LiveInterval));
        if (!new_arr)
            return;
        ctx->intervals = new_arr;
        ctx->interval_capacity = new_cap;
    }

    LiveInterval *li = &ctx->intervals[ctx->interval_count++];
    li->vreg = vreg;
    li->start = pos;
    li->end = pos;
    li->phys_reg = PHYS_NONE;
    li->spilled = false;
    li->spill_offset = 0;
}

void regalloc_build_intervals(RegAllocCtx *ctx)
{
    if (!ctx || !ctx->func)
        return;

    // المشي على كل تعليمة بترتيب تسلسلي
    int pos = 0;
    int block_idx = 0;
    for (MachineBlock *block = ctx->func->blocks; block; block = block->next)
    {
        // السجلات الحية عند دخول الكتلة تبدأ فتراتها هنا
        if (block_idx < ctx->block_count && ctx->block_live)
        {
            BlockLiveness *bl = &ctx->block_live[block_idx];
            for (int v = 0; v < ctx->max_vreg; v++)
            {
                if (bitset_test(bl->live_in, v))
                {
                    update_interval(ctx, v, pos);
                }
            }
        }

        for (MachineInst *inst = block->first; inst; inst = inst->next)
        {
            // تسجيل الاستخدامات
            if (is_normal_vreg(&inst->src1))
                update_interval(ctx, inst->src1.data.vreg, pos);
            if (is_normal_vreg(&inst->src2))
                update_interval(ctx, inst->src2.data.vreg, pos);
            if (inst->src1.kind == MACH_OP_MEM && mem_base_vreg(&inst->src1) >= 0)
                update_interval(ctx, mem_base_vreg(&inst->src1), pos);
            if (inst->src2.kind == MACH_OP_MEM && mem_base_vreg(&inst->src2) >= 0)
                update_interval(ctx, mem_base_vreg(&inst->src2), pos);
            if (inst->dst.kind == MACH_OP_MEM && mem_base_vreg(&inst->dst) >= 0)
                update_interval(ctx, mem_base_vreg(&inst->dst), pos);

            // تسجيل التعريف
            if (is_normal_vreg(&inst->dst))
                update_interval(ctx, inst->dst.data.vreg, pos);

            pos++;
        }

        // السجلات الحية عند خروج الكتلة تمتد فتراتها إلى هنا
        if (block_idx < ctx->block_count && ctx->block_live)
        {
            BlockLiveness *bl = &ctx->block_live[block_idx];
            for (int v = 0; v < ctx->max_vreg; v++)
            {
                if (bitset_test(bl->live_out, v))
                {
                    update_interval(ctx, v, pos - 1);
                }
            }
        }

        block_idx++;
    }
}

