/* SPDX-License-Identifier: MIT */
/* zombu4 OF-28: stock 2.05 has two smoothing filters, Reading_StabilityFilter (0x0800B754, state 0x20007DF8 /
 * 0x20008085 / 0x20007238) and Reading_StabilityFilterAC (0x08008344, state 0x20007CD8 / 0x20008084 / 0x200070A8),
 * each with ONE state shared by every caller. Different quantities pass through the same state (AC+DC: DC phase and
 * combined value; VA: voltage and current; burden display), so when two quantities are within the threshold of each
 * other the steady reading is averaged with the other one (AC+DC 20009 shown as 20002).
 * Fix: every call site gets its own state. The algorithm is the stock one, quirks included (the newest value is
 * stored at [0] before the shift, so it is counted twice and the previous newest drops out; the DC variant returns
 * the input unchanged for threshold 0 or n 0; the AC variant has no such test), so a site that was never shared
 * reads exactly as before. State lives in .bss, zeroed on first use behind a magic word (no startup code zeroes
 * patch RAM). */
#include "stock.h"

typedef struct { int32_t mean; uint8_t count; int32_t hist[20]; } sf_state;

enum { S_DCV, S_DCMV, S_OHM, S_DIODE, S_DCI, S_ACDC, S_CAP, S_ACI, S_ACI_CLAMP, S_ACVA_V, S_ACV_6BB0, S_N };
static sf_state st[S_N];
static uint32_t st_magic;
#define SF_MAGIC 0x5346A28Bu

static sf_state *state(int i)
{
    if (st_magic != SF_MAGIC) {
        uint8_t *p = (uint8_t *)st;
        for (unsigned k = 0; k < sizeof st; k++) p[k] = 0;
        st_magic = SF_MAGIC;
    }
    return &st[i];
}

static int32_t run(sf_state *s, int32_t v, int32_t thr, unsigned n)
{
    int32_t d = (int32_t)((uint32_t)v - (uint32_t)s->mean);   /* wraps as in stock */
    if (d > thr || (int64_t)d + thr < 0) s->count = 0;
    s->hist[0] = v;
    if (s->count == 0) { s->count = 1; s->mean = v; return v; }
    s->count++;
    if (s->count > n) s->count = (uint8_t)n;
    if (s->count > 20) s->count = 20;              /* stock callers pass n <= 20 (Ohm range 0 uses 20) */
    for (int i = 1; i < s->count; i++) s->hist[s->count - i] = s->hist[s->count - 1 - i];
    int64_t sum = 0;
    for (int i = 0; i < s->count; i++) sum += s->hist[i];
    s->mean = (int32_t)(sum / s->count);
    return s->mean;
}

static int32_t dc(int i, int32_t v, int32_t thr, unsigned n)
{
    if (thr == 0 || (n & 0xFFFF) == 0) return v;
    return run(state(i), v, thr, n & 0xFFFF);
}
static int32_t ac(int i, int32_t v, int32_t thr, unsigned n) { return run(state(i), v, thr, n & 0xFFFF); }

int32_t sf_dcv(int32_t v, int32_t t, unsigned n)      { return dc(S_DCV, v, t, n); }      /* 0800772e DCV, AC+DC DC phase, DC VA V */
int32_t sf_dcmv(int32_t v, int32_t t, unsigned n)     { return dc(S_DCMV, v, t, n); }     /* 0800780a */
int32_t sf_ohm(int32_t v, int32_t t, unsigned n)      { return dc(S_OHM, v, t, n); }      /* 08007a88 */
int32_t sf_diode(int32_t v, int32_t t, unsigned n)    { return dc(S_DIODE, v, t, n); }    /* 08007eb2 */
int32_t sf_dci(int32_t v, int32_t t, unsigned n)      { return dc(S_DCI, v, t, n); }      /* 08008154 DCI, DC VA I */
int32_t sf_acdc(int32_t v, int32_t t, unsigned n)     { return dc(S_ACDC, v, t, n); }     /* 0800a65e AC+DC combined */
int32_t sf_cap(int32_t v, int32_t t, unsigned n)      { return dc(S_CAP, v, t, n); }      /* 0800b4b2 */
int32_t sf_aci(int32_t v, int32_t t, unsigned n)      { return ac(S_ACI, v, t, n); }      /* 080068d2 AC I, AC VA I */
int32_t sf_aci_clamp(int32_t v, int32_t t, unsigned n){ return ac(S_ACI_CLAMP, v, t, n); }/* 08006a70 */
int32_t sf_acva_v(int32_t v, int32_t t, unsigned n)   { return ac(S_ACVA_V, v, t, n); }   /* 08006aba AC VA V */
int32_t sf_acv_6bb0(int32_t v, int32_t t, unsigned n) { return ac(S_ACV_6BB0, v, t, n); } /* 08006bb0 */
