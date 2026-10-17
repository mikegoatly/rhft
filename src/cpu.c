#include "arm.h"

u8 *mem;
u32 R[16];
u32 psr;        /* NZCVIF + mode bits (26-bit layout) */
u32 cpu_pc;     /* address of the instruction being executed */
u64 icount;
int halted;
int yield_flag;

/* banked registers */
static u32 bank_usr[2], bank_svc[2], bank_irq[2];
static u32 bank_fiq[7], bank_usr_fiq[5];

int str_len(const char *s) { int n = 0; while (s[n]) n++; return n; }
void str_cpy(char *d, const char *s) { while ((*d++ = *s++)); }
int str_eq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
void mem_cpy(void *d, const void *s, u32 n) { u8 *dd = d; const u8 *ss = s; while (n--) *dd++ = *ss++; }
void mem_set(void *d, int c, u32 n) { u8 *dd = d; while (n--) *dd++ = (u8)c; }
u32 rdstr(u32 a, char *buf, int max) {
    int i = 0;
    for (;;) { u8 c = rd8(a + i); if (c < 32 || i >= max - 1) break; buf[i++] = c; }
    buf[i] = 0; return (u32)i;
}

u32 cpu_mode(void) { return psr & 3; }

static u32 *bank_for(u32 mode) {
    switch (mode) { case MODE_SVC: return bank_svc; case MODE_IRQ: return bank_irq; default: return bank_usr; }
}

void cpu_set_mode(u32 mode) {
    u32 old = psr & 3;
    if (old == mode) { return; }
    /* save current r13/r14 */
    if (old == MODE_FIQ) { for (int i = 0; i < 7; i++) bank_fiq[i] = R[8 + i]; for (int i = 0; i < 5; i++) R[8 + i] = bank_usr_fiq[i]; }
    else { u32 *b = bank_for(old); b[0] = R[13]; b[1] = R[14]; }
    if (mode == MODE_FIQ) { for (int i = 0; i < 5; i++) bank_usr_fiq[i] = R[8 + i]; for (int i = 0; i < 7; i++) R[8 + i] = bank_fiq[i]; }
    else { u32 *b = bank_for(mode); R[13] = b[0]; R[14] = b[1]; }
    psr = (psr & ~3u) | mode;
}

static inline void set_psr_full(u32 v) {
    if ((psr & 3) == MODE_USR) { psr = (psr & 0x0FFFFFFFu) | (v & 0xF0000000u); }
    else { u32 m = v & 3; psr = (psr & 3) | (v & 0xFC000000u); cpu_set_mode(m); }
}

void cpu_init(void) {
    for (int i = 0; i < 16; i++) R[i] = 0;
    psr = MODE_USR; cpu_pc = 0; halted = 0; icount = 0;
    bank_usr[0] = bank_usr[1] = 0;
    bank_svc[0] = SVC_STACK_TOP; bank_svc[1] = 0;
    bank_irq[0] = IRQ_STACK_TOP; bank_irq[1] = 0;
}

static inline u32 reg_get(int r, u32 pc) {
    if (r == 15) return ((pc + 8) & 0x03FFFFFCu) | (psr & 0xFC000003u);
    return R[r];
}

#define FLAG_N ((psr >> 31) & 1)
#define FLAG_Z ((psr >> 30) & 1)
#define FLAG_C ((psr >> 29) & 1)
#define FLAG_V ((psr >> 28) & 1)

static inline int cond_ok(u32 cc) {
    switch (cc) {
    case 0: return FLAG_Z; case 1: return !FLAG_Z;
    case 2: return FLAG_C; case 3: return !FLAG_C;
    case 4: return FLAG_N; case 5: return !FLAG_N;
    case 6: return FLAG_V; case 7: return !FLAG_V;
    case 8: return FLAG_C && !FLAG_Z; case 9: return !FLAG_C || FLAG_Z;
    case 10: return FLAG_N == FLAG_V; case 11: return FLAG_N != FLAG_V;
    case 12: return !FLAG_Z && (FLAG_N == FLAG_V); case 13: return FLAG_Z || (FLAG_N != FLAG_V);
    case 14: return 1; default: return 0;
    }
}

static inline void set_nz(u32 r) { psr = (psr & 0x3FFFFFFFu) | (r & 0x80000000u) | (r == 0 ? PSR_Z : 0); }
static inline void set_nzc(u32 r, u32 c) { psr = (psr & 0x1FFFFFFFu) | (r & 0x80000000u) | (r == 0 ? PSR_Z : 0) | (c ? PSR_C : 0); }
static inline void set_nzcv(u32 r, u32 c, u32 v) { psr = (psr & 0x0FFFFFFFu) | (r & 0x80000000u) | (r == 0 ? PSR_Z : 0) | (c ? PSR_C : 0) | (v ? PSR_V : 0); }

/* compute shifter operand; sets *cout */
static inline u32 shifter(u32 instr, u32 pc, u32 *cout) {
    u32 c = FLAG_C;
    if (instr & (1u << 25)) {
        u32 imm = instr & 0xFF, rot = ((instr >> 8) & 0xF) * 2;
        u32 v = rot ? ((imm >> rot) | (imm << (32 - rot))) : imm;
        if (rot) c = v >> 31;
        *cout = c; return v;
    }
    u32 rm = instr & 0xF, type = (instr >> 5) & 3, amt;
    u32 val;
    if (instr & 0x10) { /* register shift */
        amt = R[(instr >> 8) & 0xF] & 0xFF;
        val = (rm == 15) ? (((pc + 12) & 0x03FFFFFCu) | (psr & 0xFC000003u)) : R[rm];
        if (amt == 0) { *cout = c; return val; }
    } else {
        amt = (instr >> 7) & 0x1F;
        val = reg_get(rm, pc);
        if (amt == 0) {
            switch (type) {
            case 0: *cout = c; return val;
            case 1: *cout = val >> 31; return 0;
            case 2: *cout = val >> 31; return (val >> 31) ? 0xFFFFFFFFu : 0;
            case 3: *cout = val & 1; return (val >> 1) | (c << 31); /* RRX */
            }
        }
    }
    switch (type) {
    case 0: if (amt < 32) { *cout = (val >> (32 - amt)) & 1; return val << amt; }
            if (amt == 32) { *cout = val & 1; return 0; } *cout = 0; return 0;
    case 1: if (amt < 32) { *cout = (val >> (amt - 1)) & 1; return val >> amt; }
            if (amt == 32) { *cout = val >> 31; return 0; } *cout = 0; return 0;
    case 2: if (amt < 32) { *cout = (val >> (amt - 1)) & 1; return (u32)((s32)val >> amt); }
            *cout = val >> 31; return (val >> 31) ? 0xFFFFFFFFu : 0;
    default: amt &= 31; if (amt == 0) { *cout = val >> 31; return val; }
            *cout = (val >> (amt - 1)) & 1; return (val >> amt) | (val << (32 - amt));
    }
}

static inline void write_pc(u32 v) { cpu_pc = v & 0x03FFFFFCu; }

static void undefined(u32 instr, u32 pc) {
    char b[64]; str_cpy(b, "Undefined instruction at ");
    host_logn(b, pc); host_logn("  instr=", instr); halted = 1;
}

/* execute one instruction; returns nonzero if pc already updated */
#ifdef DEBUGTRACE
u32 trace_ring[4096]; u32 trace_pos;
#endif
static inline void step(void) {
    u32 pc = cpu_pc;
    u32 instr = rd32(pc);
#ifdef DEBUGTRACE
    trace_ring[trace_pos++ & 4095] = pc;
    if (pc & 0xFC000003u) { host_logn("misaligned pc ", pc); halted = 1; return; }
#endif
    u32 next = pc + 4;
    icount++;
    u32 cc = instr >> 28;
    if (instr == 0) { undefined(instr, pc); return; }
    if (cc != 14 && !cond_ok(cc)) { cpu_pc = next; return; }
    u32 top = (instr >> 25) & 7;
    switch (top) {
    case 0: case 1: {
        if (top == 0 && (instr & 0x0F0) == 0x090) { /* multiply / swap */
            if ((instr & 0x0FC00000u) == 0) { /* MUL/MLA */
                u32 rd = (instr >> 16) & 0xF, rn = (instr >> 12) & 0xF, rs = (instr >> 8) & 0xF, rm = instr & 0xF;
                u32 res = R[rm] * R[rs];
                if (instr & (1u << 21)) res += R[rn];
                R[rd] = res;
                if (instr & (1u << 20)) set_nz(res);
                cpu_pc = next; return;
            }
            if ((instr & 0x0F800000u) == 0x00800000u) { /* long multiply */
                u32 rdhi = (instr >> 16) & 0xF, rdlo = (instr >> 12) & 0xF, rs = (instr >> 8) & 0xF, rm = instr & 0xF;
                u64 res;
                if (instr & (1u << 22)) res = (u64)((s64)(s32)R[rm] * (s64)(s32)R[rs]); else res = (u64)R[rm] * (u64)R[rs];
                if (instr & (1u << 21)) res += ((u64)R[rdhi] << 32) | R[rdlo];
                R[rdlo] = (u32)res; R[rdhi] = (u32)(res >> 32);
                if (instr & (1u << 20)) { psr = (psr & 0x3FFFFFFFu) | ((u32)(res >> 32) & 0x80000000u) | (res == 0 ? PSR_Z : 0); }
                cpu_pc = next; return;
            }
            if ((instr & 0x0FB00FF0u) == 0x01000090u) { /* SWP */
                u32 rn = (instr >> 16) & 0xF, rd = (instr >> 12) & 0xF, rm = instr & 0xF;
                u32 a = R[rn];
                if (instr & (1u << 22)) { u32 t = rd8(a); wr8(a, (u8)R[rm]); R[rd] = t; }
                else { u32 t = rd32(a); u32 rot = (a & 3) * 8; if (rot) t = (t >> rot) | (t << (32 - rot)); wr32(a, R[rm]); R[rd] = t; }
                cpu_pc = next; return;
            }
            undefined(instr, pc); return;
        }
        if (top == 0 && (instr & 0x0E000090u) == 0x00000090u) { undefined(instr, pc); return; } /* halfword ops */
        /* data processing */
        u32 op = (instr >> 21) & 0xF, S = (instr >> 20) & 1;
        u32 rn = (instr >> 16) & 0xF, rd = (instr >> 12) & 0xF;
        u32 cout;
        u32 op2 = shifter(instr, pc, &cout);
        u32 a = (rn == 15) ? ((pc + 8) & 0x03FFFFFCu) : R[rn];
        u32 res; u32 c, v;
        int logical = 0, write = 1;
        switch (op) {
        case 0: res = a & op2; logical = 1; break;
        case 1: res = a ^ op2; logical = 1; break;
        case 2: res = a - op2; c = a >= op2; v = ((a ^ op2) & (a ^ res)) >> 31; break;
        case 3: res = op2 - a; c = op2 >= a; v = ((op2 ^ a) & (op2 ^ res)) >> 31; break;
        case 4: res = a + op2; c = res < a; v = (~(a ^ op2) & (a ^ res)) >> 31; break;
        case 5: { u64 t = (u64)a + op2 + FLAG_C; res = (u32)t; c = t >> 32; v = (~(a ^ op2) & (a ^ res)) >> 31; break; }
        case 6: { u64 t = (u64)a - op2 - (FLAG_C ? 0 : 1); res = (u32)t; c = !(t >> 32); v = ((a ^ op2) & (a ^ res)) >> 31; break; }
        case 7: { u64 t = (u64)op2 - a - (FLAG_C ? 0 : 1); res = (u32)t; c = !(t >> 32); v = ((op2 ^ a) & (op2 ^ res)) >> 31; break; }
        case 8: res = a & op2; logical = 1; write = 0; break;
        case 9: res = a ^ op2; logical = 1; write = 0; break;
        case 10: res = a - op2; c = a >= op2; v = ((a ^ op2) & (a ^ res)) >> 31; write = 0; break;
        case 11: res = a + op2; c = res < a; v = (~(a ^ op2) & (a ^ res)) >> 31; write = 0; break;
        case 12: res = a | op2; logical = 1; break;
        case 13: res = op2; logical = 1; break;
        case 14: res = a & ~op2; logical = 1; break;
        default: res = ~op2; logical = 1; break;
        }
        if (!write) {
            if (rd == 15) { set_psr_full(res); }  /* TSTP/TEQP/CMPP/CMNP */
            else if (logical) set_nzc(res, cout); else set_nzcv(res, c, v);
            cpu_pc = next; return;
        }
        if (rd == 15) {
            if (S) set_psr_full(res);
            write_pc(res); return;
        }
        R[rd] = res;
        if (S) { if (logical) set_nzc(res, cout); else set_nzcv(res, c, v); }
        cpu_pc = next; return;
    }
    case 2: case 3: { /* single data transfer */
        if (top == 3 && (instr & 0x10)) { undefined(instr, pc); return; }
        u32 rn = (instr >> 16) & 0xF, rd = (instr >> 12) & 0xF;
        u32 P = (instr >> 24) & 1, U = (instr >> 23) & 1, B = (instr >> 22) & 1, W = (instr >> 21) & 1, L = (instr >> 20) & 1;
        u32 off;
        if (top == 3) { u32 cdum; u32 save = psr; off = shifter(instr & ~0x02000010u, pc, &cdum); psr = save; }
        else off = instr & 0xFFF;
        u32 base = (rn == 15) ? ((pc + 8) & 0x03FFFFFCu) : R[rn];
        u32 addr = P ? (U ? base + off : base - off) : base;
        if (L) {
            u32 val;
            if (B) val = rd8(addr);
            else { val = rd32(addr); u32 rot = (addr & 3) * 8; if (rot) val = (val >> rot) | (val << (32 - rot)); }
            if (!P || W) { u32 wb = U ? base + off : base - off; if (!P) R[rn] = wb; else R[rn] = wb; }
            if (rd == 15) { write_pc(val); return; }
            R[rd] = val;
        } else {
            u32 val = reg_get(rd, pc);
            if (rd == 15) val += 4; /* STR PC stores PC+12 */
            if (B) wr8(addr, (u8)val); else wr32(addr, val);
            if (!P || W) { u32 wb = U ? base + off : base - off; R[rn] = wb; }
        }
        cpu_pc = next; return;
    }
    case 4: { /* block data transfer */
        u32 rn = (instr >> 16) & 0xF;
        u32 P = (instr >> 24) & 1, U = (instr >> 23) & 1, S = (instr >> 22) & 1, W = (instr >> 21) & 1, L = (instr >> 20) & 1;
        u32 list = instr & 0xFFFF;
        int n = 0; for (u32 i = 0; i < 16; i++) if (list & (1u << i)) n++;
        u32 base = R[rn];
        u32 start, wb;
        if (U) { start = P ? base + 4 : base; wb = base + n * 4; }
        else { start = P ? base - n * 4 : base - n * 4 + 4; wb = base - n * 4; }
        start &= ~3u;
        int user_bank = S && !(L && (list & 0x8000)) && (psr & 3) != MODE_USR;
        u32 mode_save = psr & 3;
        if (user_bank) { /* swap in user r13/r14 */ cpu_set_mode(MODE_USR); psr = (psr & ~3u) | 0; }
        u32 a = start;
        if (L) {
            int pc_loaded = 0; u32 pcval = 0;
            for (u32 i = 0; i < 16; i++) if (list & (1u << i)) {
                u32 v = rd32(a); a += 4;
                if (i == 15) { pc_loaded = 1; pcval = v; } else R[i] = v;
            }
            if (W && !(list & (1u << rn))) R[rn] = wb;
            if (user_bank) { cpu_set_mode(mode_save); }
            if (pc_loaded) {
                if (S) set_psr_full(pcval);
                write_pc(pcval); return;
            }
        } else {
            int first = 1;
            for (u32 i = 0; i < 16; i++) if (list & (1u << i)) {
                u32 v;
                if (i == 15) v = ((pc + 12) & 0x03FFFFFCu) | (psr & 0xFC000003u);
                else if (i == rn && !first) v = wb;
                else v = R[i];
                wr32(a, v); a += 4; first = 0;
            }
            if (W) R[rn] = wb;
            if (user_bank) { cpu_set_mode(mode_save); }
        }
        cpu_pc = next; return;
    }
    case 5: { /* branch */
        s32 off = (s32)(instr << 8) >> 6;
        if (instr & (1u << 24)) R[14] = (next & 0x03FFFFFCu) | (psr & 0xFC000003u);
        cpu_pc = (pc + 8 + (u32)off) & 0x03FFFFFCu; return;
    }
    case 7: {
        if (instr & (1u << 24)) { /* SWI */
            cpu_pc = next;
            os_swi(instr & 0x00FFFFFFu);
            return;
        }
        undefined(instr, pc); return;
    }
    default: undefined(instr, pc); return;
    }
}

int cpu_run(long max_instr) {
    yield_flag = 0;
    while (max_instr-- > 0) {
        step();
        if (halted) return 2;
        if (yield_flag) return 1;
    }
    return 0;
}

/* nested call: run from addr in given mode until pc reaches MAGIC_RET */
int cpu_step_one(void) { step(); return halted; }

void cpu_call2(u32 addr, u32 mode, int push_ret) {
    u32 save_psr = psr;
    u32 save_pc = cpu_pc;
    u32 save_r13, save_r14;
    cpu_set_mode(mode);
    save_r13 = R[13]; save_r14 = R[14];
    R[14] = MAGIC_RET | (psr & 0xF0000000u) | mode;
    if (push_ret) { R[13] -= 4; wr32(R[13], R[14]); }
    cpu_pc = addr & 0x03FFFFFCu;
    long guard = 200000000;
    while (cpu_pc != MAGIC_RET && !halted && guard-- > 0) step();
    if (guard <= 0) { host_log("cpu_call: runaway"); halted = 1; }
    /* flags from callee (psr now has whatever MOVS PC,R14 restored) */
    u32 flags = psr & 0xF0000000u;
    cpu_set_mode(mode);
    R[13] = save_r13; R[14] = save_r14;
    cpu_set_mode(save_psr & 3);
    psr = (save_psr & 0x0FFFFFFFu) | flags;
    cpu_pc = save_pc;
}
void cpu_call(u32 addr, u32 mode) { cpu_call2(addr, mode, 0); }
