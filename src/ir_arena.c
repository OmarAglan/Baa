/**
 * @file ir_arena.c
 * @brief تنفيذ مُخصِّص ساحة (Arena) لذاكرة IR.
 */

#include "ir_arena.h"

#include <stdlib.h>
#include <string.h>

#define IR_ARENA_MIN_CHUNK_SIZE (64u * 1024u)

static size_t ir_align_up(size_t x, size_t align) {
    if (align == 0) return x;
    size_t m = align - 1;
    return (x + m) & ~m;
}

static size_t ir_max_size(size_t a, size_t b) {
    return (a > b) ? a : b;
}

void ir_arena_init(IRArena* arena, size_t default_chunk_size) {
    if (!arena) return;
    arena->head = NULL;
    arena->default_chunk_size = default_chunk_size;
    if (arena->default_chunk_size < IR_ARENA_MIN_CHUNK_SIZE) {
        arena->default_chunk_size = IR_ARENA_MIN_CHUNK_SIZE;
    }
}

void ir_arena_destroy(IRArena* arena) {
    if (!arena) return;
    IRArenaChunk* c = arena->head;
    while (c) {
        IRArenaChunk* next = c->next;
        free(c);
        c = next;
    }
    arena->head = NULL;
}

void* ir_arena_alloc(IRArena* arena, size_t size, size_t align) {
    if (!arena) return NULL;
    if (size == 0) size = 1;
    if (align == 0) align = sizeof(void*);

    // ضمان أن align قوة 2 لتفادي سلوك غير متوقع.
    if ((align & (align - 1)) != 0) {
        align = sizeof(void*);
    }

    IRArenaChunk* c = arena->head;
    if (c) {
        size_t off = ir_align_up(c->used, align);
        if (off + size <= c->cap) {
            void* p = (void*)(c->data + off);
            c->used = off + size;
            return p;
        }
    }

    // تخصيص كتلة جديدة.
    size_t need = ir_align_up(size, align);
    size_t cap = ir_max_size(arena->default_chunk_size, need);
    IRArenaChunk* n = (IRArenaChunk*)malloc(sizeof(IRArenaChunk) + cap);
    if (!n) return NULL;
    n->next = arena->head;
    n->used = 0;
    n->cap = cap;
    arena->head = n;

    size_t off = ir_align_up(n->used, align);
    void* p = (void*)(n->data + off);
    n->used = off + size;
    return p;
}

void* ir_arena_calloc(IRArena* arena, size_t count, size_t size, size_t align) {
    if (!arena) return NULL;
    if (count == 0 || size == 0) {
        return ir_arena_alloc(arena, 1, align);
    }

    // فحص overflow.
    size_t total = count * size;
    if (size != 0 && total / size != count) return NULL;

    void* p = ir_arena_alloc(arena, total, align);
    if (!p) return NULL;
    memset(p, 0, total);
    return p;
}

char* ir_arena_strdup(IRArena* arena, const char* s) {
    if (!arena) return NULL;
    if (!s) return NULL;
    size_t len = strlen(s);
    char* out = (char*)ir_arena_alloc(arena, len + 1, 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}
