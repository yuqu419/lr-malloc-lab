# Lingrui Malloc Lab

*Adapted from the CS:APP Malloc Lab (Carnegie Mellon University). You write your own `malloc`, `free` and `realloc`.*

**Note:** this lab is in English on purpose.

## 1. Introduction

Every call to `malloc` is really two questions: *where do I find free memory*, and *where do I keep the bookkeeping?*
In this lab you answer both of them yourself.

You will write a dynamic storage allocator for C programs: your own versions of `malloc`, `free` and
`realloc`. A driver program, `mdriver`, replays eleven recorded traces of allocation requests against your
allocator, checks that it is correct, then measures how much of the heap you wasted (**space utilization**)
and how fast you were (**throughput**). Those two numbers are combined into a single **performance index**
out of 100.

Your allocator must score **higher than the textbook allocator** in `mm-textbook.c` — the simple
implicit-free-list allocator from the textbook. That is the whole bar.

## 2. Logistics

### 2.1 What you are given

| File | Description |
| --- | --- |
| `mm.c` | **The only file you may modify.** This is your solution. |
| `mm.h` | The interfaces you must implement. Do not change them. |
| `mm-textbook.c` | The textbook allocator you have to beat. Read it, do not modify it. |
| `mm-naive.c` | A deliberately awful allocator that never reuses memory. For contrast. |
| `mdriver.c` | The driver that tests and scores your allocator. Do not modify it. |
| `traces/` | The eleven default traces used for scoring. Do not modify them. |
| `short1-bal.rep`, `short2-bal.rep` | Two tiny traces for debugging. |
| `Makefile` | Build and scoring targets. Do not modify it. |
| `config.h`, `memlib.{c,h}`, `fsecs.{c,h}`, `ftimer.{c,h}`, `clock.{c,h}`, `fcyc.{c,h}` | Driver support code. Do not modify it. |
| `malloclab.pdf` | The original CMU writeup. **You do not need to read it** — everything you need is in this README. |

### 2.2 How to hand in

1. **Fork** this repository.
2. Implement `mm.c`, commit, and push.
3. Only commits touching `mm.c` are acceptable. If any other tracked file changes, the submission is
   rejected — the tests below are only meaningful if the driver, the traces and the baseline are the ones
   I shipped.

I'll clone your fork and run `make score`, from the repository root.

`make score` does both and prints the verdict, so run it before you push.

## 3. Getting Started

You need **Linux on x86-64** — native, a virtual machine, or **WSL2** on Windows. `gcc` and GNU `make` are
the only tools required.

```bash
make            # build the driver against your mm.c
make test       # run all eleven traces and print your score
make textbook   # run the same traces against the textbook allocator
make score      # both, then PASS or FAIL
```

To work on one trace at a time while you develop:

```bash
make test TRACE=short1-bal.rep          # a tiny trace, fast to debug
make test TRACE=traces/realloc-bal.rep  # the trace that hurts most allocators
```

**The file you are given is intentionally terrible.** Run `make test` before you change anything and you
will see:

```text
Terminated with 5 errors
```

That is not a broken lab. The starting `mm.c` hands out fresh memory from the top of the heap on every
`malloc`, never reuses a freed block, and never coalesces; it runs out of heap on three traces and its
`realloc` is a stub. Its performance index is **0**. Your job is to replace it with a real allocator.

## 4. What you must implement

Four functions, declared in `mm.h` and defined in `mm.c`:

```c
int   mm_init(void);
void *mm_malloc(size_t size);
void  mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);
```

Their semantics match the C library:

- **`mm_init`** is called once before any other call, and must return `0` on success and `-1` if
  initialization fails. The heap starts empty; grow it with `mem_sbrk`.
- **`mm_malloc`** returns a pointer to an allocated block of **at least** `size` bytes that lies entirely
  inside the heap and does not overlap any other allocated block. It returns `NULL` on failure.
- **`mm_free`** frees the block `ptr`, which must have come from an earlier `mm_malloc` or `mm_realloc`
  and must not already be free.
- **`mm_realloc`** returns a block of at least `size` bytes:
  - `ptr == NULL` is equivalent to `mm_malloc(size)`;
  - `size == 0` is equivalent to `mm_free(ptr)`;
  - otherwise the contents are preserved up to the minimum of the old and new sizes, and the rest is
    uninitialized. The returned pointer may be the same block or a different one.

The driver starts from an empty heap for every trace, and it **enforces 8-byte alignment**: every pointer
your allocator returns must be a multiple of 8, exactly like the C library's `malloc`. It reports an error
the moment you return something misaligned.

You may use the memory-system routines from `memlib.c` instead of the real `sbrk`:

- `void *mem_sbrk(int incr)` grows the simulated heap by `incr` positive bytes and returns a pointer to
  the first byte of the new area, or `(void *)-1` if the heap would exceed 20 MB.
- `void *mem_heap_lo(void)`, `void *mem_heap_hi(void)` return the first and last byte of the heap.
- `size_t mem_heapsize(void)`, `size_t mem_pagesize(void)` return the current heap size and the page size.

## 5. How you are scored

The driver computes a single performance index `P`:

```text
P = 0.6 * U + 0.4 * min(1, T / 600000)
```

where `U` is the average space utilization over the traces and `T` is the average throughput in
operations per second. Utilization is the peak ratio of bytes you handed out to bytes of heap you used, so
1 is perfect; `600000` is the throughput at which the fast half of the score is capped.

Then:

- **Correctness.** All eleven traces must be valid. If any trace produces an error, the driver prints
  `Terminated with N errors` and your performance index is **0**, whatever else happened.
- **The bar.** Your index must be **strictly higher** than the index of `mm-textbook.c` measured on the
  same machine in the same session, which is what `make score` checks. On a normal x86-64 Linux machine
  the textbook scores around **70-71**, and it is worth knowing why: its utilization is 74%, and it loses
  throughput to a linear first-fit scan over a 2 MB implicit list.

Two things are worth understanding before you optimise:

- **Throughput is nearly free.** Any allocator that keeps real free lists rather than scanning the whole
  heap reaches the cap and takes the full 40 points. The textbook's 26-27 points are an artefact of its
  linear scan. So the fight is almost entirely about **space utilization**.
- **Two traces are capped.** `binary-bal.rep` tops out at **55%** and `binary2-bal.rep` at **51%** for
  *any* allocator: the interleaving of sizes leaves holes that the later, larger requests simply cannot
  use (456-byte holes cannot hold 520-byte requests). A good allocator therefore lands around 89%
  utilization on this trace set, not 100%. Do not burn a week chasing the last few percent on those two
  files.

Your final grade is my judgement of your `mm.c` plus that number, so write code you would be willing to
defend.

## 6. Rules

- **Only `mm.c` may change.** Do not touch the driver, the traces, `config.h`, the `Makefile`, or the
  textbook baseline.
- Do not call any memory-management routine: no `malloc`, `calloc`, `realloc`, `free`, `sbrk`, `brk`,
  or any variant.
- Scalar globals (integers, floats, pointers) are always allowed. **Arrays and lists** are the only
  compound data structures you may define globally. Structs, trees and any other compound structure are not.
- Every pointer you return must be 8-byte aligned.
- Do not change the interfaces in `mm.h`.

A submission that breaks these rules, or that crashes the driver, scores zero.

## 7. Recommended approach

The design that works well here, and that I recommend you start from, is a **segregated free list**.

This is the idea, not the design — the choice of size classes, of placement policy, and of everything
else is deliberately left to you to explore.

Work in stages. Get `mm_init`, `mm_malloc` and `mm_free` correct and fast on the **first nine traces**
first; only then turn to `mm_realloc`, which is what the last two traces exercise. Encapsulate your
pointer arithmetic in preprocessor macros — `HDRP`, `FTRP`, `NEXT_BLKP`, `PREV_BLKP` and friends are the
standard vocabulary and they will save you from a lot of casting mistakes.

## 8. Debugging

- `make test TRACE=short1-bal.rep` — eleven traces take a few seconds; a tiny trace takes none.
- `make debug` — same driver, built with `-g` and AddressSanitizer/UndefinedBehaviorSanitizer. Slower,
  but it turns a wild pointer into a precise report instead of a crash in the wrong place.
- `make naive` — shows what the no-reuse allocator scores, if you want a floor to compare against.
- `make clean` — delete `build/`.
- The driver itself is worth learning: `mdriver -h` lists its flags, `-v` prints the per-trace table you
  already see from `make test`, `-V` adds a line per trace as it runs (useful for finding *which* trace
  fails), and `-l` also measures the C library's `malloc` for comparison.
- Compile with `make debug` and run under `gdb` or using vscode GUI.

## 9. References

- *Computer Systems: A Programmer's Perspective* (CS:APP), Chapter 9.9 — the implicit free list, explicit
  free lists, segregated fits and the boundary-tag machinery this lab is built on.
- The CS:APP Malloc Lab writeup, Carnegie Mellon University (`malloclab.pdf` in this repository).
- The original handout files: `mdriver.c`, `memlib.c`, `config.h` and the trace set are copyright (c)
  2002 R. Bryant and D. O'Hallaron, and are redistributed here unmodified for teaching use.
