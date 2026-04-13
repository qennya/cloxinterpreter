#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "table.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

// ── Timer ────────────────────────────────────────────────────────────────────
#include <windows.h>
static double now_ms() {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / freq.QuadPart * 1000.0;
}

// ── String helpers ───────────────────────────────────────────────────────────

// Make a fresh ObjString without interning (so we can control exact keys)
static ObjString* makeString(const char* chars, int len) {
    return copyString(chars, len);
}

static ObjString* makeIndexedString(const char* prefix, int i) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%s%d", prefix, i);
    return makeString(buf, len);
}

// ── Benchmark 1: Pure insertions ─────────────────────────────────────────────
// Tests: how fast is tableSet when growing from empty?
// Why: the most common operation; also exercises all the resize/rehash paths.

static void bench_insertions(int n) {
    Table t;
    initTable(&t);

    double start = now_ms();
    for (int i = 0; i < n; i++) {
        ObjString* key = makeIndexedString("key", i);
        tableSet(&t, OBJ_VAL(key), NUMBER_VAL((double)i));
    }
    double elapsed = now_ms() - start;

    printf("  [1] Insert %d keys:           %.2f ms  (%.1f ns/op)\n",
           n, elapsed, elapsed * 1e6 / n);
    freeTable(&t);
}

// ── Benchmark 2: Lookup — all hits ───────────────────────────────────────────
// Tests: tableGet when the key is always present.
// Why: the hot path in real use (variable lookup, string interning).
// Separate from insertions so we don't conflate the two.

static void bench_lookup_hits(int n) {
    Table t;
    initTable(&t);

    // Pre-populate
    ObjString** keys = malloc(n * sizeof(ObjString*));
    for (int i = 0; i < n; i++) {
        keys[i] = makeIndexedString("key", i);
        tableSet(&t, OBJ_VAL(keys[i]), NUMBER_VAL((double)i));
    }

    Value out;
    double start = now_ms();
    for (int i = 0; i < n; i++) {
        tableGet(&t, OBJ_VAL(keys[i]), &out);
    }
    double elapsed = now_ms() - start;

    printf("  [2] Lookup %d keys (all hits): %.2f ms  (%.1f ns/op)\n",
           n, elapsed, elapsed * 1e6 / n);
    free(keys);
    freeTable(&t);
}

// ── Benchmark 3: Lookup — all misses ─────────────────────────────────────────
// Tests: tableGet when the key is never present.
// Why: misses must probe until they hit an EMPTY slot — worst case is a full
//      table. This also tests whether our load factor is conservative enough.

static void bench_lookup_misses(int n) {
    Table t;
    initTable(&t);

    for (int i = 0; i < n; i++) {
        ObjString* key = makeIndexedString("key", i);
        tableSet(&t, OBJ_VAL(key), NUMBER_VAL((double)i));
    }

    // Look up keys that were never inserted
    ObjString** miss_keys = malloc(n * sizeof(ObjString*));
    for (int i = 0; i < n; i++) {
        miss_keys[i] = makeIndexedString("miss", i);
    }

    Value out;
    double start = now_ms();
    for (int i = 0; i < n; i++) {
        tableGet(&t, OBJ_VAL(miss_keys[i]), &out);
    }
    double elapsed = now_ms() - start;

    printf("  [3] Lookup %d keys (all miss): %.2f ms  (%.1f ns/op)\n",
           n, elapsed, elapsed * 1e6 / n);
    free(miss_keys);
    freeTable(&t);
}

// ── Benchmark 4: Delete churn ─────────────────────────────────────────────────
// Tests: repeated insert + delete cycles.
// Why: tombstones accumulate with open addressing. If you delete a lot,
//      tombstones fill slots and make future probes longer — this reveals
//      whether that becomes a problem in practice.

static void bench_delete_churn(int n) {
    Table t;
    initTable(&t);

    // Fill half the table first
    int half = n / 2;
    for (int i = 0; i < half; i++) {
        ObjString* key = makeIndexedString("key", i);
        tableSet(&t, OBJ_VAL(key), NUMBER_VAL((double)i));
    }

    double start = now_ms();
    for (int i = 0; i < n; i++) {
        // Delete one, insert one — keeps size roughly constant
        ObjString* del_key = makeIndexedString("key", i % half);
        ObjString* ins_key = makeIndexedString("new", i);
        tableDelete(&t, OBJ_VAL(del_key));
        tableSet(&t, OBJ_VAL(ins_key), NUMBER_VAL((double)i));
    }
    double elapsed = now_ms() - start;

    printf("  [4] Delete churn %d ops:       %.2f ms  (%.1f ns/op)\n",
           n, elapsed, elapsed * 1e6 / n);
    freeTable(&t);
}

// ── Benchmark 5: Small table, repeated access ─────────────────────────────────
// Tests: a tiny table (8–16 entries) hammered with millions of lookups.
// Why: real Lox programs have small local scopes. If the table is tiny but
//      lookup is slow, that dominates runtime. Also tests cache behaviour —
//      a tiny table fits entirely in L1 cache.

static void bench_small_table_hot(int reps) {
    Table t;
    initTable(&t);

    // Insert just 8 keys
    ObjString* keys[8];
    for (int i = 0; i < 8; i++) {
        keys[i] = makeIndexedString("var", i);
        tableSet(&t, OBJ_VAL(keys[i]), NUMBER_VAL((double)i));
    }

    Value out;
    double start = now_ms();
    for (int i = 0; i < reps; i++) {
        tableGet(&t, OBJ_VAL(keys[i % 8]), &out);
    }
    double elapsed = now_ms() - start;

    printf("  [5] Small table %d lookups:    %.2f ms  (%.1f ns/op)\n",
           reps, elapsed, elapsed * 1e6 / reps);
    freeTable(&t);
}

// ── Benchmark 6: Non-string keys (numbers) ────────────────────────────────────
// Tests: our new hashValue() path for VAL_NUMBER keys.
// Why: we added number key support in challenge 1 — make sure it's not slower
//      than string keys due to the bit-reinterpretation in hashValue().

static void bench_number_keys(int n) {
    Table t;
    initTable(&t);

    double start = now_ms();
    for (int i = 0; i < n; i++) {
        tableSet(&t, NUMBER_VAL((double)i), NUMBER_VAL((double)i * 2));
    }
    double elapsed_insert = now_ms() - start;

    Value out;
    start = now_ms();
    for (int i = 0; i < n; i++) {
        tableGet(&t, NUMBER_VAL((double)i), &out);
    }
    double elapsed_lookup = now_ms() - start;

    printf("  [6] Number keys insert %d:     %.2f ms  (%.1f ns/op)\n",
           n, elapsed_insert, elapsed_insert * 1e6 / n);
    printf("  [6] Number keys lookup %d:     %.2f ms  (%.1f ns/op)\n",
           n, elapsed_lookup, elapsed_lookup * 1e6 / n);
    freeTable(&t);
}

// ── Benchmark 7: Collision stress — sequential integers ───────────────────────
// Tests: what happens when keys hash to nearby buckets (common with sequential
//        numbers since their low bits are similar)?
// Why: linear probing degrades badly when entries cluster together. This checks
//      whether our hash function spreads number keys well enough.

static void bench_collision_stress(int n) {
    Table t;
    initTable(&t);

    // Insert numbers 0..n — these have very similar hashes if the function
    // is naive (low bits all differ by 1, upper bits all zero)
    double start = now_ms();
    for (int i = 0; i < n; i++) {
        tableSet(&t, NUMBER_VAL((double)i), BOOL_VAL(true));
    }

    Value out;
    for (int i = 0; i < n; i++) {
        tableGet(&t, NUMBER_VAL((double)i), &out);
    }
    double elapsed = now_ms() - start;

    printf("  [7] Sequential int stress %d:  %.2f ms  (%.1f ns/op)\n",
           n * 2, elapsed, elapsed * 1e6 / (n * 2));
    freeTable(&t);
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
    // VM must be initialized because ObjString allocation touches vm.objects
    initVM();

    int N = 100000;
    int REPS = 1000000;

    printf("\n=== clox Hash Table Benchmarks (N=%d) ===\n\n", N);

    bench_insertions(N);
    bench_lookup_hits(N);
    bench_lookup_misses(N);
    bench_delete_churn(N);
    bench_small_table_hot(REPS);
    bench_number_keys(N);
    bench_collision_stress(N);

    printf("\n");
    freeVM();
    return 0;
}