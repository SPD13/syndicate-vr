/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file difftest.c
 *     Differential testing of remade C functions against original asm.
 * @par Purpose:
 *     Shadow execution: snapshot writable state, run asm, save result,
 *     restore snapshot, run C, compare both results byte for byte.
 * @par Comment:
 *     See difftest.h for usage. Only active with --enable-difftest;
 *     --enable-test-harness has the turn limit and rand() only.
 * @author   SyndicateVR decomp agent
 * @date     06 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "difftest.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bfconfig.h"
#include "bftypes.h"
#if defined(WIN32) || defined(_WIN32)
# include <windows.h>
#endif
/******************************************************************************/

#if DIFFTEST_HARNESS
/** The C library rand() state, here instead of in msvcrt.dll.
 *
 * msvcrt keeps the rand() state in per-thread data that is not part of our
 * image, so the snapshots missed it: the C run continued the sequence after
 * the asm run's draws. In difftest builds rand() and srand() are defined
 * here, with the msvcrt algorithm and initial state, so the state lives in
 * our .data and is saved, restored and compared like other game state.
 * The asm reaches rand() through ac_rand, which calls this one too.
 * Harness builds without difftest use it as well, so that runs give the
 * same draws on every target and C library.
 */
static u32 dt_holdrand = 1;

void srand(unsigned int seed) __attribute__((no_instrument_function));
int rand(void) __attribute__((no_instrument_function));

void srand(unsigned int seed)
{
    dt_holdrand = seed;
}

int rand(void)
{
    dt_holdrand = dt_holdrand * 214013u + 2531011u;
    return (int)((dt_holdrand >> 16) & 0x7FFF);
}

/** Xorshift on 32 bits, also where ulong is longer; the bot uses it too. */
ulong difftest_fuzz_rand(ulong *p_seed)
{
    u32 x = (u32)*p_seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *p_seed = x;
    return x;
}
#endif

#if defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST

#define DT_MAX_REGIONS 32
#define DT_MAX_IGNORES 64
#define DT_MAX_REPORTED_DIFFS 24
#define DT_STACK_WINDOW_MAX 0x4000

struct DtRegion {
    const char *name;
    ubyte *ptr;
    size_t len;
};

struct DtIgnore {
    ulong addr;
    ulong len;
};

/** All difftest state, kept in one struct so that it can be excluded
 * from snapshots.
 */
struct DiffTestState {
    TbBool initialized;
    TbBool enabled;
    TbBool in_shadow;
    TbBool fast;
    TbBool fuzzing;
    char fn_name[128];
    ulong max_compares;
    ulong min_compares;
    ulong turn_limit;
    ulong turns;
    ulong calls;
    ulong compares;
    FILE *log;
    char log_fname[260];

    struct DtRegion regions[DT_MAX_REGIONS];
    int regions_num;
    struct DtRegion extra[DT_MAX_REGIONS];
    int extra_num;
    struct DtIgnore ignores[DT_MAX_IGNORES];
    int ignores_num;

    /* Snapshot buffers: pre-state and post-asm state, per region */
    ubyte *pre_buf;
    ubyte *post_buf;
    size_t buf_size;

    const char *cur_name;
    ubyte asm_ret[16];
    size_t asm_ret_len;
    ubyte c_ret[16];
    size_t c_ret_len;
    TbBool c_ret_set;
};

static struct DiffTestState dt;

#if LB_MEMORY_ARENAS
/* bflibrary arenas; the struct is private there, so mirror its layout */
struct DtMemBlock {
    void *Pointer;
    ulong Selector;
    ulong Size;
};
extern struct DtMemBlock memory_blocks[256];
#endif

#if !(defined(WIN32) || defined(_WIN32))
extern char __data_start[], _end[];
#endif

static void dt_log(const char *fmt, ...) __attribute__((format(printf,1,2)));

/** Returns address relative to our image base, which stays the same
 * between runs even when the image is relocated.
 */
static ulong dt_rva(const void *addr) __attribute__((no_instrument_function));
static ulong dt_rva(const void *addr)
{
#if defined(WIN32) || defined(_WIN32)
    return (ulong)addr - (ulong)GetModuleHandle(NULL);
#else
    return (ulong)addr;
#endif
}

static void dt_log(const char *fmt, ...)
{
    va_list ap;
    if (dt.log == NULL) {
        dt.log = fopen(dt.log_fname, "a");
        if (dt.log == NULL)
            dt.log = stderr;
    }
    va_start(ap, fmt);
    vfprintf(dt.log, fmt, ap);
    va_end(ap);
    fflush(dt.log);
}

static void dt_parse_ignores(const char *str)
{
    while ((str != NULL) && (*str != '\0') && (dt.ignores_num < DT_MAX_IGNORES))
    {
        char *endp;
        ulong addr, len;
        addr = strtoul(str, &endp, 16);
        if (*endp != ':')
            break;
        len = strtoul(endp + 1, &endp, 0);
        dt.ignores[dt.ignores_num].addr = addr;
        dt.ignores[dt.ignores_num].len = len;
        dt.ignores_num++;
        str = (*endp == ',') ? endp + 1 : NULL;
    }
}

/** Checks that rand() gives the same sequence as msvcrt's own. */
static void dt_check_rand(void)
{
#if defined(WIN32) || defined(_WIN32)
    HMODULE crt = GetModuleHandle("msvcrt.dll");
    void (*crt_srand)(unsigned int);
    int (*crt_rand)(void);
    u32 saved;
    int i, bad;

    if (crt == NULL)
        return;
    crt_srand = (void (*)(unsigned int))GetProcAddress(crt, "srand");
    crt_rand = (int (*)(void))GetProcAddress(crt, "rand");
    if ((crt_srand == NULL) || (crt_rand == NULL) || (crt_rand == rand))
        return;
    saved = dt_holdrand;
    crt_srand(12345);
    srand(12345);
    bad = 0;
    for (i = 0; i < 64; i++) {
        if (crt_rand() != rand())
            bad++;
    }
    dt_holdrand = saved;
    if (bad != 0)
        dt_log("difftest: WARNING rand() differs from msvcrt in %d of 64 draws\n", bad);
#endif
}

static void dt_init(void)
{
    const char *s;

    if (dt.initialized)
        return;
    dt.initialized = true;

    s = getenv("SWARS_DIFFTEST_FN");
    if ((s != NULL) && (*s != '\0')) {
        dt.enabled = true;
        snprintf(dt.fn_name, sizeof(dt.fn_name), "%s", s);
    }
    s = getenv("SWARS_DIFFTEST_MAX");
    dt.max_compares = (s != NULL) ? strtoul(s, NULL, 0) : 300;
    s = getenv("SWARS_DIFFTEST_MIN");
    dt.min_compares = (s != NULL) ? strtoul(s, NULL, 0) : 1;
    s = getenv("SWARS_DIFFTEST_TURNS");
    dt.turn_limit = (s != NULL) ? strtoul(s, NULL, 0) : 0;
    s = getenv("SWARS_DIFFTEST_FAST");
    dt.fast = (s != NULL) && (*s != '\0') && (*s != '0');
    s = getenv("SWARS_DIFFTEST_LOG");
    snprintf(dt.log_fname, sizeof(dt.log_fname), "%s",
      ((s != NULL) && (*s != '\0')) ? s : "difftest.log");
    dt_parse_ignores(getenv("SWARS_DIFFTEST_IGNORE"));

    if (dt.enabled)
        dt_log("difftest: start fn=%s max=%lu min=%lu turns=%lu ignores=%d\n",
          dt.fn_name, dt.max_compares, dt.min_compares, dt.turn_limit,
          dt.ignores_num);
    if (dt.enabled)
        dt_check_rand();
}

static TbBool dt_name_matches(const char *name)
{
    if (!dt.enabled)
        return false;
    if (strcmp(dt.fn_name, "*") == 0)
        return true;
    return (strcmp(dt.fn_name, name) == 0);
}

static void dt_add_region(const char *name, void *ptr, size_t len)
{
    if ((ptr == NULL) || (len == 0) || (dt.regions_num >= DT_MAX_REGIONS))
        return;
    dt.regions[dt.regions_num].name = name;
    dt.regions[dt.regions_num].ptr = ptr;
    dt.regions[dt.regions_num].len = len;
    dt.regions_num++;
}

/** Adds a region, splitting it around our own state struct.
 */
static void dt_add_region_excl(const char *name, ubyte *ptr, size_t len)
{
    ubyte *ex_beg = (ubyte *)&dt;
    ubyte *ex_end = ex_beg + sizeof(dt);
    if ((ex_end <= ptr) || (ex_beg >= ptr + len)) {
        dt_add_region(name, ptr, len);
        return;
    }
    if (ex_beg > ptr)
        dt_add_region(name, ptr, ex_beg - ptr);
    if (ex_end < ptr + len)
        dt_add_region(name, ex_end, (ptr + len) - ex_end);
}

static void dt_add_static_regions(void)
{
#if defined(WIN32) || defined(_WIN32)
    /* Enumerate writable sections of our own PE image */
    ubyte *base = (ubyte *)GetModuleHandle(NULL);
    IMAGE_DOS_HEADER *p_dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *p_nt = (IMAGE_NT_HEADERS *)(base + p_dos->e_lfanew);
    IMAGE_SECTION_HEADER *p_sec = IMAGE_FIRST_SECTION(p_nt);
    int i;

    for (i = 0; i < p_nt->FileHeader.NumberOfSections; i++, p_sec++)
    {
        static char sec_names[16][9];
        if ((p_sec->Characteristics & IMAGE_SCN_MEM_WRITE) == 0)
            continue;
        if (i >= 16)
            break;
        memcpy(sec_names[i], p_sec->Name, 8);
        sec_names[i][8] = '\0';
        dt_add_region_excl(sec_names[i], base + p_sec->VirtualAddress,
          p_sec->Misc.VirtualSize);
    }
#else
    dt_add_region_excl(".data+.bss", (ubyte *)__data_start,
      (ubyte *)_end - (ubyte *)__data_start);
#endif
}

static void *dt_stack_base(void)
{
#if defined(WIN32) || defined(_WIN32)
    NT_TIB *p_tib = (NT_TIB *)NtCurrentTeb();
    return p_tib->StackBase;
#else
    return NULL;
#endif
}

static void dt_collect_regions(void *frame)
{
    int i;
    ubyte *stk_beg, *stk_end;

    dt.regions_num = 0;
    dt_add_static_regions();
#if LB_MEMORY_ARENAS
    dt_add_region("arena0", memory_blocks[0].Pointer, memory_blocks[0].Size);
    dt_add_region("arena1", memory_blocks[1].Pointer, memory_blocks[1].Size);
#endif
    for (i = 0; i < dt.extra_num; i++)
        dt_add_region(dt.extra[i].name, dt.extra[i].ptr, dt.extra[i].len);
    /* Stack above the frame of the shadowed function - caller locals */
    stk_beg = frame;
    stk_end = dt_stack_base();
    if ((stk_end == NULL) || (stk_end <= stk_beg) ||
      (stk_end - stk_beg > DT_STACK_WINDOW_MAX))
        stk_end = stk_beg + DT_STACK_WINDOW_MAX;
    /* skip saved frame pointer and return address */
    stk_beg += 2 * sizeof(void *);
    dt_add_region("stack", stk_beg, stk_end - stk_beg);
}

static TbBool dt_ensure_buffers(void)
{
    size_t total = 0;
    int i;

    for (i = 0; i < dt.regions_num; i++)
        total += dt.regions[i].len;
    if (total <= dt.buf_size)
        return true;
    free(dt.pre_buf);
    free(dt.post_buf);
    dt.pre_buf = malloc(total);
    dt.post_buf = malloc(total);
    if ((dt.pre_buf == NULL) || (dt.post_buf == NULL)) {
        dt_log("difftest: cannot allocate %lu bytes for snapshots\n", (ulong)total);
        dt.buf_size = 0;
        dt.enabled = false;
        return false;
    }
    dt.buf_size = total;
    return true;
}

static void dt_save(ubyte *buf)
{
    int i;
    for (i = 0; i < dt.regions_num; i++) {
        memcpy(buf, dt.regions[i].ptr, dt.regions[i].len);
        buf += dt.regions[i].len;
    }
}

static void dt_restore(const ubyte *buf)
{
    int i;
    for (i = 0; i < dt.regions_num; i++) {
        memcpy(dt.regions[i].ptr, buf, dt.regions[i].len);
        buf += dt.regions[i].len;
    }
}

static TbBool dt_is_ignored(ulong addr)
{
    int i;
    for (i = 0; i < dt.ignores_num; i++) {
        if ((addr >= dt.ignores[i].addr) &&
          (addr < dt.ignores[i].addr + dt.ignores[i].len))
            return true;
    }
    return false;
}

static void dt_log_bytes(const char *label, const ubyte *p, size_t n)
{
    size_t k;
    dt_log("      %s:", label);
    for (k = 0; k < n; k++)
        dt_log(" %02x", (uint)p[k]);
    dt_log("\n");
}

/** Compares current state against post-asm buffer; returns amount of
 * differing bytes (not counting ignored ranges).
 */
static ulong dt_compare(void)
{
    const ubyte *buf = dt.post_buf;
    ulong diff_bytes = 0;
    int reported = 0;
    int i;

    for (i = 0; i < dt.regions_num; i++)
    {
        const ubyte *cur = dt.regions[i].ptr;
        size_t len = dt.regions[i].len;
        size_t pos = 0;

        if (memcmp(cur, buf, len) != 0)
        {
            while (pos < len)
            {
                size_t beg, end;
                if (cur[pos] == buf[pos] || dt_is_ignored((ulong)(cur + pos))) {
                    pos++;
                    continue;
                }
                beg = pos;
                end = pos + 1;
                /* merge differences closer than 8 bytes */
                while (end < len) {
                    size_t k, lim = end + 8;
                    TbBool more = false;
                    for (k = end; k < lim && k < len; k++) {
                        if (cur[k] != buf[k] && !dt_is_ignored((ulong)(cur + k))) {
                            more = true;
                            end = k + 1;
                            break;
                        }
                    }
                    if (!more)
                        break;
                }
                for (pos = beg; pos < end; pos++) {
                    if (cur[pos] != buf[pos])
                        diff_bytes++;
                }
                if (reported < DT_MAX_REPORTED_DIFFS) {
                    size_t n = end - beg;
                    if (n > 32) n = 32;
                    dt_log("  DIFF region=%s addr=0x%08lx rva=0x%08lx off=0x%lx len=%lu\n",
                      dt.regions[i].name, (ulong)(cur + beg), dt_rva(cur + beg),
                      (ulong)beg, (ulong)(end - beg));
                    dt_log_bytes("asm", buf + beg, n);
                    dt_log_bytes("  c", cur + beg, n);
                }
                reported++;
                pos = end;
            }
        }
        buf += len;
    }
    if (reported > DT_MAX_REPORTED_DIFFS)
        dt_log("  ... %d more differing ranges\n", reported - DT_MAX_REPORTED_DIFFS);
    return diff_bytes;
}

static void dt_finish(void)
{
    int code;

    if (dt.compares < dt.min_compares)
        code = DtExit_UNVERIFIED;
    else
        code = DtExit_PASS;
    dt_log("difftest: %s fn=%s calls=%lu compared=%lu turns=%lu\n",
      (code == DtExit_PASS) ? "PASS" : "UNVERIFIED",
      dt.fn_name, dt.calls, dt.compares, dt.turns);
    exit(code);
}

int difftest_shadow_begin(const char *name, void *frame)
{
    dt_init();
    if (dt.in_shadow || !dt_name_matches(name))
        return 0;
    dt.calls++;
    if (dt.compares >= dt.max_compares)
        return 0;
    dt_collect_regions(frame);
    if (!dt_ensure_buffers())
        return 0;
    if (dt.compares == 0) {
        int i;
        for (i = 0; i < dt.regions_num; i++)
            dt_log("  region %-8s 0x%08lx len=0x%lx\n", dt.regions[i].name,
              (ulong)dt.regions[i].ptr, (ulong)dt.regions[i].len);
    }
    dt.in_shadow = true;
    dt.cur_name = name;
    dt.asm_ret_len = 0;
    dt.c_ret_len = 0;
    dt.c_ret_set = false;
    dt_save(dt.pre_buf);
    return 1;
}

void difftest_shadow_mid(const void *asm_ret, size_t ret_len)
{
    if (ret_len > sizeof(dt.asm_ret))
        ret_len = sizeof(dt.asm_ret);
    if (asm_ret != NULL)
        memcpy(dt.asm_ret, asm_ret, ret_len);
    dt.asm_ret_len = ret_len;
    dt_save(dt.post_buf);
    dt_restore(dt.pre_buf);
}

void difftest_shadow_ret(const void *c_ret, size_t ret_len)
{
    if (ret_len > sizeof(dt.c_ret))
        ret_len = sizeof(dt.c_ret);
    memcpy(dt.c_ret, c_ret, ret_len);
    dt.c_ret_len = ret_len;
    dt.c_ret_set = true;
}

void difftest_shadow_end(int *p_active)
{
    ulong diff_bytes;
    TbBool ret_mismatch;

    if (!*p_active)
        return;
    dt.in_shadow = false;
    dt.compares++;

    ret_mismatch = false;
    if (dt.asm_ret_len > 0) {
        if (!dt.c_ret_set) {
            dt_log("difftest: %s call %lu: C code returned without DIFFTEST_RETURN()\n",
              dt.cur_name, dt.calls);
            ret_mismatch = true;
        } else if ((dt.asm_ret_len != dt.c_ret_len) ||
          (memcmp(dt.asm_ret, dt.c_ret, dt.asm_ret_len) != 0)) {
            dt_log("difftest: %s call %lu: RETURN VALUE MISMATCH\n",
              dt.cur_name, dt.calls);
            dt_log_bytes("asm", dt.asm_ret, dt.asm_ret_len);
            dt_log_bytes("  c", dt.c_ret, dt.c_ret_len);
            ret_mismatch = true;
        }
    }
    diff_bytes = dt_compare();
    if (diff_bytes > 0 || ret_mismatch)
    {
        dt_log("difftest: MISMATCH fn=%s call=%lu compared=%lu turn=%lu diff_bytes=%lu\n",
          dt.cur_name, dt.calls, dt.compares, dt.turns, diff_bytes);
        exit(DtExit_MISMATCH);
    }
}

void difftest_add_region(const char *name, void *ptr, size_t len)
{
    int i;
    for (i = 0; i < dt.extra_num; i++) {
        if (strcmp(dt.extra[i].name, name) == 0) {
            dt.extra[i].ptr = ptr;
            dt.extra[i].len = len;
            return;
        }
    }
    if (dt.extra_num >= DT_MAX_REGIONS)
        return;
    dt.extra[dt.extra_num].name = name;
    dt.extra[dt.extra_num].ptr = ptr;
    dt.extra[dt.extra_num].len = len;
    dt.extra_num++;
}

void difftest_game_turn(void)
{
    dt_init();
    if (!dt.enabled)
        return;
    dt.turns++;
    if ((dt.turn_limit > 0) && (dt.turns >= dt.turn_limit))
        dt_finish();
}

TbBool difftest_skip_turn_wait(void)
{
    dt_init();
    return dt.enabled && dt.fast;
}

/******************************************************************************/
/* Random-input tests of pure functions */


void difftest_run_fuzz(void)
{
    const char *s;
    ulong iters, seed, k;
    int i, ran;

    s = getenv("SWARS_SMK_COMPARE");
    if ((s != NULL) && (*s != '\0')) {
        extern int smack_compare_files(const char *list);
        exit(smack_compare_files(s) ? DtExit_MISMATCH : DtExit_PASS);
    }
    dt_init();
    s = getenv("SWARS_DIFFTEST_ITERS");
    iters = (s != NULL) ? strtoul(s, NULL, 0) : 2000;
    if (!dt.enabled) {
        dt.enabled = true;
        snprintf(dt.fn_name, sizeof(dt.fn_name), "*");
    }
    dt.fuzzing = true;
    dt.max_compares = (ulong)-1;
    ran = 0;
    for (i = 0; difftest_fuzz_cases[i].name != NULL; i++)
    {
        ulong compares_before;
        if (!dt_name_matches(difftest_fuzz_cases[i].name))
            continue;
        seed = 0x2545F491;
        compares_before = dt.compares;
        for (k = 0; k < iters; k++)
            difftest_fuzz_cases[i].func(&seed);
        dt_log("difftest: fuzz %s PASS iterations=%lu compared=%lu\n",
          difftest_fuzz_cases[i].name, iters, dt.compares - compares_before);
        ran++;
    }
    if (ran == 0) {
        dt_log("difftest: fuzz: no case matches fn=%s\n", dt.fn_name);
        exit(DtExit_UNVERIFIED);
    }
    exit((dt.compares > 0) ? DtExit_PASS : DtExit_UNVERIFIED);
}

/******************************************************************************/
/* Call coverage, for builds with -finstrument-functions */

#define DT_COV_SLOTS 65536

struct DtCovSlot {
    void *fn;
    ulong count;
};

static struct DtCovSlot *dt_cov;
static TbBool dt_cov_checked;

void __cyg_profile_func_enter(void *this_fn, void *call_site)
  __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void *this_fn, void *call_site)
  __attribute__((no_instrument_function));
static void dt_cov_dump(void) __attribute__((no_instrument_function));

static void dt_cov_dump(void)
{
    const char *fname = getenv("SWARS_COVERAGE");
    FILE *fh;
    ulong i;

    if ((dt_cov == NULL) || (fname == NULL))
        return;
    fh = fopen(fname, "w");
    if (fh == NULL)
        return;
    for (i = 0; i < DT_COV_SLOTS; i++) {
        if (dt_cov[i].fn != NULL)
            fprintf(fh, "%08lx %lu\n", dt_rva(dt_cov[i].fn), dt_cov[i].count);
    }
    fclose(fh);
}

void __cyg_profile_func_enter(void *this_fn, void *call_site)
{
    ulong h;

    if (!dt_cov_checked) {
        dt_cov_checked = true;
        if (getenv("SWARS_COVERAGE") != NULL) {
            dt_cov = calloc(DT_COV_SLOTS, sizeof(struct DtCovSlot));
            atexit(dt_cov_dump);
        }
    }
    if (dt_cov == NULL)
        return;
    h = ((ulong)this_fn >> 2) & (DT_COV_SLOTS - 1);
    while ((dt_cov[h].fn != NULL) && (dt_cov[h].fn != this_fn))
        h = (h + 1) & (DT_COV_SLOTS - 1);
    dt_cov[h].fn = this_fn;
    dt_cov[h].count++;
}

void __cyg_profile_func_exit(void *this_fn, void *call_site)
{
}

#else

void difftest_add_region(const char *name, void *ptr, size_t len)
{
}

#if DIFFTEST_HARNESS
/** Turn limit and fast turns of harness builds without difftest; same
 * variables and exit code as a difftest run of a function never called.
 */
static struct {
    TbBool initialized;
    TbBool enabled;
    TbBool fast;
    ulong turn_limit;
    ulong turns;
} ht;

static void ht_init(void)
{
    const char *s;

    if (ht.initialized)
        return;
    ht.initialized = true;
    s = getenv("SWARS_DIFFTEST_FN");
    ht.enabled = (s != NULL) && (*s != ' ');
    s = getenv("SWARS_DIFFTEST_TURNS");
    ht.turn_limit = (s != NULL) ? strtoul(s, NULL, 0) : 0;
    s = getenv("SWARS_DIFFTEST_FAST");
    ht.fast = (s != NULL) && (*s != ' ') && (*s != '0');
}

void difftest_game_turn(void)
{
    ht_init();
    if (!ht.enabled)
        return;
    ht.turns++;
    if ((ht.turn_limit > 0) && (ht.turns >= ht.turn_limit))
        exit(DtExit_UNVERIFIED);
}

TbBool difftest_skip_turn_wait(void)
{
    ht_init();
    return ht.enabled && ht.fast;
}
#else
void difftest_game_turn(void)
{
}

TbBool difftest_skip_turn_wait(void)
{
    return false;
}
#endif

void difftest_run_fuzz(void)
{
    fprintf(stderr, "difftest: not enabled in this build (--enable-difftest)\n");
    exit(2);
}

#endif
/******************************************************************************/
