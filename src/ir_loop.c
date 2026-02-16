/**
 * @file ir_loop.c
 * @brief تنفيذ تحليل الحلقات (Loop Detection) — اكتشاف الحلقات الطبيعية عبر حواف الرجوع.
 * @version 0.3.2.7.1
 */

#include "ir_loop.h"

#include "ir_analysis.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// هياكل البيانات (Data Structures)
// ============================================================================

struct IRLoop {
    IRBlock* header;
    IRBlock* preheader;

    // تمثيل مجموعة كتل الحلقة عبر bitset حسب block->id.
    uint64_t* bits;
    int bits_len;

    IRBlock** blocks;
    int block_count;
    int block_cap;

    IRBlock** latches;
    int latch_count;
    int latch_cap;
};

struct IRLoopInfo {
    IRFunc* func;
    int max_block_id;

    IRLoop** loops;
    int loop_count;
    int loop_cap;
};

// ============================================================================
// أدوات bitset (Bitset Helpers)
// ============================================================================

static int ir_loop_bits_len_for(int max_block_id) {
    if (max_block_id <= 0) return 0;
    return (max_block_id + 63) / 64;
}

static void ir_loop_bits_set(uint64_t* bits, int bits_len, int id) {
    if (!bits || bits_len <= 0 || id < 0) return;
    int w = id / 64;
    int b = id % 64;
    if (w < 0 || w >= bits_len) return;
    bits[w] |= (uint64_t)1u << (uint64_t)b;
}

static int ir_loop_bits_test(const uint64_t* bits, int bits_len, int id) {
    if (!bits || bits_len <= 0 || id < 0) return 0;
    int w = id / 64;
    int b = id % 64;
    if (w < 0 || w >= bits_len) return 0;
    return (bits[w] >> (uint64_t)b) & 1u;
}

static void ir_loop_bits_or(uint64_t* dst, const uint64_t* src, int bits_len) {
    if (!dst || !src || bits_len <= 0) return;
    for (int i = 0; i < bits_len; i++) {
        dst[i] |= src[i];
    }
}

// ============================================================================
// أدوات عامة (General Helpers)
// ============================================================================

static int ir_func_max_block_id_local(IRFunc* func) {
    int max_id = -1;
    if (!func) return 0;
    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (b->id > max_id) max_id = b->id;
    }
    return max_id + 1;
}

static int ir_block_dominates(IRBlock* dom, IRBlock* node) {
    if (!dom || !node) return 0;

    IRBlock* cur = node;
    int guard = 0;
    while (cur) {
        if (cur == dom) return 1;
        if (!cur->idom) return 0; // unreachable
        if (cur == cur->idom) break; // entry
        cur = cur->idom;
        if (++guard > 100000) break;
    }
    return 0;
}

static void ir_loop_push_block(IRLoop* loop, IRBlock* b) {
    if (!loop || !b) return;
    if (loop->block_count >= loop->block_cap) {
        int new_cap = loop->block_cap == 0 ? 16 : loop->block_cap * 2;
        IRBlock** nb = (IRBlock**)realloc(loop->blocks, (size_t)new_cap * sizeof(IRBlock*));
        if (!nb) return;
        loop->blocks = nb;
        loop->block_cap = new_cap;
    }
    loop->blocks[loop->block_count++] = b;
}

static void ir_loop_add_latch(IRLoop* loop, IRBlock* latch) {
    if (!loop || !latch) return;
    for (int i = 0; i < loop->latch_count; i++) {
        if (loop->latches[i] == latch) return;
    }

    if (loop->latch_count >= loop->latch_cap) {
        int new_cap = loop->latch_cap == 0 ? 4 : loop->latch_cap * 2;
        IRBlock** nb = (IRBlock**)realloc(loop->latches, (size_t)new_cap * sizeof(IRBlock*));
        if (!nb) return;
        loop->latches = nb;
        loop->latch_cap = new_cap;
    }
    loop->latches[loop->latch_count++] = latch;
}

static IRLoop* ir_loop_info_find_by_header(IRLoopInfo* info, IRBlock* header) {
    if (!info || !header) return NULL;
    for (int i = 0; i < info->loop_count; i++) {
        if (info->loops[i] && info->loops[i]->header == header) return info->loops[i];
    }
    return NULL;
}

static IRLoop* ir_loop_info_add_loop(IRLoopInfo* info, IRBlock* header) {
    if (!info || !header) return NULL;

    if (info->loop_count >= info->loop_cap) {
        int new_cap = info->loop_cap == 0 ? 8 : info->loop_cap * 2;
        IRLoop** nl = (IRLoop**)realloc(info->loops, (size_t)new_cap * sizeof(IRLoop*));
        if (!nl) return NULL;
        info->loops = nl;
        info->loop_cap = new_cap;
    }

    IRLoop* loop = (IRLoop*)calloc(1, sizeof(IRLoop));
    if (!loop) return NULL;

    loop->header = header;
    loop->preheader = NULL;
    loop->bits_len = ir_loop_bits_len_for(info->max_block_id);
    if (loop->bits_len > 0) {
        loop->bits = (uint64_t*)calloc((size_t)loop->bits_len, sizeof(uint64_t));
        if (!loop->bits) {
            free(loop);
            return NULL;
        }
    }

    info->loops[info->loop_count++] = loop;
    return loop;
}

// ============================================================================
// بناء الحلقة الطبيعية (Natural Loop Construction)
// ============================================================================

static uint64_t* ir_build_natural_loop_bits(IRFunc* func, int max_id, IRBlock* head, IRBlock* tail) {
    (void)func;
    if (!head || !tail || max_id <= 0) return NULL;

    int bits_len = ir_loop_bits_len_for(max_id);
    if (bits_len <= 0) return NULL;

    uint64_t* bits = (uint64_t*)calloc((size_t)bits_len, sizeof(uint64_t));
    if (!bits) return NULL;

    // Worklist عبر stack من الكتل.
    IRBlock** stack = (IRBlock**)malloc((size_t)max_id * sizeof(IRBlock*));
    if (!stack) {
        free(bits);
        return NULL;
    }
    int sp = 0;

    ir_loop_bits_set(bits, bits_len, head->id);
    ir_loop_bits_set(bits, bits_len, tail->id);
    stack[sp++] = tail;

    while (sp > 0) {
        IRBlock* x = stack[--sp];
        if (!x) continue;

        for (int i = 0; i < x->pred_count; i++) {
            IRBlock* p = x->preds[i];
            if (!p) continue;
            if (p->id < 0 || p->id >= max_id) continue;

            if (!ir_loop_bits_test(bits, bits_len, p->id)) {
                ir_loop_bits_set(bits, bits_len, p->id);
                if (p != head) {
                    stack[sp++] = p;
                }
            }
        }
    }

    free(stack);
    return bits;
}

static void ir_loop_finalize(IRLoopInfo* info, IRLoop* loop) {
    if (!info || !loop || !info->func || !loop->header) return;

    // أعِد بناء قائمة blocks من الـ bitset.
    loop->block_count = 0;
    for (IRBlock* b = info->func->blocks; b; b = b->next) {
        if (!b) continue;
        if (b->id < 0 || b->id >= info->max_block_id) continue;
        if (ir_loop_bits_test(loop->bits, loop->bits_len, b->id)) {
            ir_loop_push_block(loop, b);
        }
    }

    // حساب preheader: predecessor وحيد لرأس الحلقة من خارج الحلقة.
    IRBlock* pre = NULL;
    int outside_count = 0;
    for (int i = 0; i < loop->header->pred_count; i++) {
        IRBlock* p = loop->header->preds[i];
        if (!p) continue;
        if (p->id < 0 || p->id >= info->max_block_id) continue;
        if (!ir_loop_bits_test(loop->bits, loop->bits_len, p->id)) {
            outside_count++;
            pre = p;
        }
    }
    loop->preheader = (outside_count == 1) ? pre : NULL;
}

// ============================================================================
// الواجهة العامة (Public API)
// ============================================================================

IRLoopInfo* ir_loop_analyze_func(IRFunc* func) {
    if (!func) return NULL;
    if (func->is_prototype) return NULL;
    if (!func->entry) return NULL;

    // ضمان وجود preds و idom.
    ir_func_compute_dominators(func);

    int max_id = ir_func_max_block_id_local(func);
    if (max_id <= 0) return NULL;

    IRLoopInfo* info = (IRLoopInfo*)calloc(1, sizeof(IRLoopInfo));
    if (!info) return NULL;
    info->func = func;
    info->max_block_id = max_id;

    // اكتشاف حواف الرجوع وبناء الحلقات.
    for (IRBlock* tail = func->blocks; tail; tail = tail->next) {
        if (!tail) continue;

        for (int s = 0; s < tail->succ_count; s++) {
            IRBlock* head = tail->succs[s];
            if (!head) continue;

            // back edge: head dominates tail
            if (!ir_block_dominates(head, tail)) continue;

            IRLoop* loop = ir_loop_info_find_by_header(info, head);
            if (!loop) {
                loop = ir_loop_info_add_loop(info, head);
                if (!loop) {
                    ir_loop_info_free(info);
                    return NULL;
                }
            }

            // natural loop blocks for (tail -> head)
            uint64_t* bits = ir_build_natural_loop_bits(func, max_id, head, tail);
            if (!bits) {
                ir_loop_info_free(info);
                return NULL;
            }

            ir_loop_bits_or(loop->bits, bits, loop->bits_len);
            free(bits);

            // latch
            ir_loop_add_latch(loop, tail);
        }
    }

    // finalize
    for (int i = 0; i < info->loop_count; i++) {
        ir_loop_finalize(info, info->loops[i]);
    }

    return info;
}

void ir_loop_info_free(IRLoopInfo* info) {
    if (!info) return;

    if (info->loops) {
        for (int i = 0; i < info->loop_count; i++) {
            IRLoop* loop = info->loops[i];
            if (!loop) continue;
            if (loop->bits) free(loop->bits);
            if (loop->blocks) free(loop->blocks);
            if (loop->latches) free(loop->latches);
            free(loop);
        }
        free(info->loops);
    }

    free(info);
}

int ir_loop_info_count(IRLoopInfo* info) {
    if (!info) return 0;
    return info->loop_count;
}

IRLoop* ir_loop_info_get(IRLoopInfo* info, int index) {
    if (!info) return NULL;
    if (index < 0 || index >= info->loop_count) return NULL;
    return info->loops[index];
}

bool ir_loop_contains(IRLoop* loop, IRBlock* block) {
    if (!loop || !block) return false;
    return ir_loop_bits_test(loop->bits, loop->bits_len, block->id) ? true : false;
}

IRBlock* ir_loop_header(IRLoop* loop) {
    return loop ? loop->header : NULL;
}

IRBlock* ir_loop_preheader(IRLoop* loop) {
    return loop ? loop->preheader : NULL;
}

int ir_loop_block_count(IRLoop* loop) {
    return loop ? loop->block_count : 0;
}

IRBlock* ir_loop_block_at(IRLoop* loop, int index) {
    if (!loop) return NULL;
    if (index < 0 || index >= loop->block_count) return NULL;
    return loop->blocks[index];
}
