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

void ir_arena_init(IRArena* arena, size_t default_chunk_size);
void ir_arena_destroy(IRArena* arena);

void* ir_arena_alloc(IRArena* arena, size_t size, size_t align);
void* ir_arena_calloc(IRArena* arena, size_t count, size_t size, size_t align);
char* ir_arena_strdup(IRArena* arena, const char* s);

#endif // BAA_IR_ARENA_H
