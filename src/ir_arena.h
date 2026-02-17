/**
 * @file ir_arena.h
 * @brief مُخصِّص ساحة (Arena) لذاكرة IR.
 *
 * الهدف: تسريع التخصيص داخل IR عبر تخصيص كتل كبيرة ثم توزيع أجزاء صغيرة منها.
 * يتم تحرير كل الذاكرة دفعة واحدة عند تدمير الساحة.
 */

#ifndef BAA_IR_ARENA_H
#define BAA_IR_ARENA_H

#include <stddef.h>

// ============================================================================
// ساحة تخصيص IR (IR Arena)
// ============================================================================

typedef struct IRArenaChunk {
    struct IRArenaChunk* next;
    size_t used;
    size_t cap;
    unsigned char data[];
} IRArenaChunk;

typedef struct IRArena {
    IRArenaChunk* head;
    size_t default_chunk_size;
} IRArena;

typedef struct IRArenaStats {
    size_t chunks;
    size_t used_bytes;
    size_t cap_bytes;
} IRArenaStats;

void ir_arena_init(IRArena* arena, size_t default_chunk_size);
void ir_arena_destroy(IRArena* arena);

void* ir_arena_alloc(IRArena* arena, size_t size, size_t align);
void* ir_arena_calloc(IRArena* arena, size_t count, size_t size, size_t align);
char* ir_arena_strdup(IRArena* arena, const char* s);

void ir_arena_get_stats(const IRArena* arena, IRArenaStats* out_stats);

#endif // BAA_IR_ARENA_H
