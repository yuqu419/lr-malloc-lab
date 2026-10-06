#include "mm.h"
#include "memlib.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// it occupys 24 bytes
typedef struct mm_chunk {
    size_t size; // The low 3 bits work as flag
    struct mm_chunk *pre;
    struct mm_chunk *next;
} mm_chunk;
/* double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define REAL_SIZE(chunk) ((chunk->size) & ~(size_t)0x7)
#define NOT_FREE(chunk) (chunk->size & 0x1)
// the head works as sentinel
static mm_chunk *head;
// the tail works as sentinel
static mm_chunk *tail;
// initialize the head to ensure the head is not null
// 0 stands for free,1 stands for not free
int mm_init(void) {
    head = mem_sbrk(sizeof(mm_chunk));
    if (head == (void *)-1)
        return -1;
    tail = mem_sbrk(sizeof(mm_chunk));
    if (tail == (void *)-1)
        return -1;
    *tail = (mm_chunk){.next = NULL, .pre = head, .size = 0 | 1};
    *head = (mm_chunk){.next = tail, .pre = NULL, .size = 0 | 1};
    return 0;
}
// it receive the size aligned
// extend the heap and return the pointer of the applied memory
static void *extend_memory(size_t size) {
    size_t new_size = size + sizeof(mm_chunk);
    // When I call mem_sbrk(incr),it will return the first byte of chunk
    void *res = mem_sbrk(new_size);
    if (res == (void *)-1)
        return NULL;
    mm_chunk *new_tail = (mm_chunk *)((char *)res + size);
    *new_tail = (mm_chunk){.next = NULL, .pre = tail, .size = 0 | 1};
    *tail = (mm_chunk){.next = new_tail, .pre = tail->pre, .size = size | 1};
    tail = new_tail;
    return tail->pre + 1;
}
// it receive the size aligned
//  you should ensure that chunk is free and it can contain the size and a new
//  chunk
static void *split(mm_chunk *chunk, size_t size) {
    mm_chunk *new_chunk = (mm_chunk *)((char *)(chunk + 1) + size);
    mm_chunk *next_chunk = chunk->next;
    if (next_chunk)
        next_chunk->pre = new_chunk;
    chunk->next = new_chunk;
    size_t remaining_size = REAL_SIZE(chunk) - sizeof(mm_chunk) - size;
    *new_chunk =
        (mm_chunk){.pre = chunk, .next = next_chunk, .size = remaining_size};
    chunk->size = size | 1;
    return chunk + 1;
}
// return the merged free chunk
static inline void *merge_front(mm_chunk *chunk) {
    while (!NOT_FREE(chunk)) {
        mm_chunk *pre_chunk = chunk->pre;
        mm_chunk *next_chunk = chunk->next;

        if (pre_chunk && !NOT_FREE(pre_chunk)) {
            *pre_chunk = (mm_chunk){.size = REAL_SIZE(pre_chunk) +
                                            REAL_SIZE(chunk) + sizeof(mm_chunk),
                                    .next = next_chunk,
                                    .pre = pre_chunk->pre};
            next_chunk->pre = pre_chunk;
        }
        chunk = pre_chunk;
    }
    return chunk->pre;
}
// return the merged free chunk
static inline void *merge_back(mm_chunk *chunk) {
    mm_chunk *next_chunk = chunk->next;
    while (next_chunk && !NOT_FREE(next_chunk)) {
        mm_chunk *grand_next_chunk = next_chunk->next;
        *chunk = (mm_chunk){.next = grand_next_chunk,
                            .pre = chunk->pre,
                            .size = REAL_SIZE(chunk) + REAL_SIZE(next_chunk) +
                                    sizeof(mm_chunk)};
        if (grand_next_chunk)
            grand_next_chunk->pre = chunk;
        next_chunk = grand_next_chunk;
    }
    return chunk;
}
void *mm_malloc(size_t size) {
    size = ALIGN(size);
    // we need to check there is any available playload
    // eaily,iterate the chunk list,and jugde the size of all free chunk
    // if it satisfys our requirements,we should take into consideration that
    // whether we need to split it to small pieces
    // the size we discuss is ALIGN(size)
    // situation 1:the size is smaller than we need,we are supposed
    // to handle next one
    // situation 2:the size is larger than we need but it
    // smaller than size + sizeof(chunk)
    //  ,we can place it into the playload of this chunk
    // situation 3:the size is larger than size + sizeof(chunk),without doubt,we
    // should split
    //  it to smaller pieces.
    mm_chunk *cur = head;
    while (cur) {
        if (REAL_SIZE(cur) >= size &&
            REAL_SIZE(cur) <= size + sizeof(mm_chunk) && !NOT_FREE(cur)) {
            cur->size = REAL_SIZE(cur) | 1;
            return cur + 1;
        } else if (REAL_SIZE(cur) > size + sizeof(mm_chunk) * 3 &&
                   !NOT_FREE(cur)) {
            return split(cur, size);
        }
        cur = cur->next;
    }
    // extend the memory directly
    return extend_memory(size);
}

void mm_free(void *ptr) {
    if (!ptr)
        return;
    mm_chunk *chunk = ((mm_chunk *)ptr) - 1;
    chunk->size = REAL_SIZE(chunk);
    // merge the back chunks
    merge_back(chunk);
    // merge the front chunks
    merge_front(chunk);
}

void *mm_realloc(void *ptr, size_t size) {
    size = ALIGN(size);
    mm_chunk *chunk = ((mm_chunk *)ptr) - 1;

    mm_chunk *next_chunk = chunk->next;
    if (REAL_SIZE(chunk) >= size) {
        return ptr;
    }
    if (next_chunk && !NOT_FREE(next_chunk)) {
        *chunk = (mm_chunk){.size = (REAL_SIZE(chunk) + REAL_SIZE(next_chunk) +
                                     sizeof(mm_chunk)) |
                                    1,
                            .next = next_chunk->next,
                            .pre = chunk->pre};
        if (next_chunk->next)
            next_chunk->next->pre = chunk;
    }
    if (REAL_SIZE(chunk) >= size) {
        // if (REAL_SIZE(chunk) > size + sizeof(mm_chunk)) {
        //     ptr = split(chunk, size);
        // }
        return ptr;
    }
    // mm_chunk *pre_chunk = chunk->pre;
    // if (pre_chunk && !NOT_FREE(pre_chunk) && REAL_SIZE(pre_chunk) + sizeof(mm_chunk) + REAL_SIZE(chunk) >= size) {
    //     mm_chunk *next_chunk = chunk->next;
    //     size_t chunk_size = REAL_SIZE(chunk);
    //     memmove(pre_chunk + 1, chunk + 1, chunk_size);
    //     *pre_chunk = (mm_chunk){
    //         .size = (REAL_SIZE(pre_chunk) + chunk_size + sizeof(mm_chunk)) | 1,
    //         .next = next_chunk,
    //         .pre = pre_chunk->pre};
    //     if (next_chunk)
    //         next_chunk->pre = pre_chunk;
    //     chunk = pre_chunk;
    //     ptr = chunk + 1;
    // }
    // if (REAL_SIZE(chunk) >= size) {
    //     if (REAL_SIZE(chunk) > size + sizeof(mm_chunk)) {
    //         ptr = split(chunk, size);
    //     }
    //     return ptr;
    // }
    if (chunk->next == tail) {
        size_t new_size = size - REAL_SIZE(chunk);
        // When I call mem_sbrk(incr),it will return the first byte of chunk
        void *res = mem_sbrk(new_size);
        if (res == (void *)-1)
            return NULL;
        tail = (mm_chunk *)((char *)mem_heap_hi() + 1) - 1;
        *tail = (mm_chunk){.next = NULL, .pre = chunk, .size = 0 | 1};
        *chunk = (mm_chunk){.next = tail, .pre = chunk->pre, .size = size | 1};
        return chunk + 1;
    }
    size_t copy_size = REAL_SIZE(chunk) > size ? size : REAL_SIZE(chunk);
    void *new_ptr = mm_malloc(size);
    if (!new_ptr)
        return NULL;
    memcpy(new_ptr, ptr, copy_size);
    mm_free(ptr);
    return new_ptr;
}
