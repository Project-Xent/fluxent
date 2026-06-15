#include "fx_internal.h"

#include <stdlib.h>
#include <string.h>

#define FX_ARENA_MIN_CHUNK (64u * 1024u)

static FxArenaChunk *fx_arena_chunk_create(size_t capacity) {
	if (capacity < FX_ARENA_MIN_CHUNK) capacity = FX_ARENA_MIN_CHUNK;
	FxArenaChunk *chunk = ( FxArenaChunk * ) malloc(sizeof(FxArenaChunk) + capacity);
	if (!chunk) return NULL;
	chunk->next     = NULL;
	chunk->capacity = capacity;
	chunk->used     = 0;
	return chunk;
}

bool fx_arena_init(FxArena *arena, size_t first_chunk_size) {
	arena->first = fx_arena_chunk_create(first_chunk_size);
	arena->head  = arena->first;
	return arena->first != NULL;
}

void fx_arena_dispose(FxArena *arena) {
	FxArenaChunk *chunk = arena->first;
	while (chunk) {
		FxArenaChunk *next = chunk->next;
		free(chunk);
		chunk = next;
	}
	arena->first = NULL;
	arena->head  = NULL;
}

/* Overflow chunks are released on reset; a frame that ballooned once does not
 * pin its high-water mark forever. */
void fx_arena_reset(FxArena *arena) {
	if (!arena->first) return;
	FxArenaChunk *chunk = arena->first->next;
	while (chunk) {
		FxArenaChunk *next = chunk->next;
		free(chunk);
		chunk = next;
	}
	arena->first->next = NULL;
	arena->first->used = 0;
	arena->head        = arena->first;
}

static size_t fx_align_up(size_t n) {
	size_t const a = _Alignof(max_align_t);
	return (n + a - 1) & ~(a - 1);
}

void *fx_arena_alloc(FxArena *arena, size_t size) {
	if (!arena->head) return NULL;
	size = fx_align_up(size ? size : 1);

	if (arena->head->used + size > arena->head->capacity) {
		FxArenaChunk *chunk = fx_arena_chunk_create(size);
		if (!chunk) return NULL;
		chunk->next       = arena->head->next;
		arena->head->next = chunk;
		arena->head       = chunk;
	}

	void *p            = arena->head->data + arena->head->used;
	arena->head->used += size;
	memset(p, 0, size);
	return p;
}
