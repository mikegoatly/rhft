/* RISC OS environment emulation for RHFT: SWIs, modules, files, VDU, keys, sound */
#include "arm.h"

/* ------------------------------------------------------------------ logging */
static char logbuf[256];
static void hex8(char *d, u32 v) { static const char h[] = "0123456789ABCDEF"; for (int i = 7; i >= 0; i--) { d[i] = h[v & 15]; v >>= 4; } d[8] = 0; }
void host_logn(const char *s, u32 n) { int l = str_len(s); if (l > 200) l = 200; mem_cpy(logbuf, s, l); logbuf[l++] = '&'; hex8(logbuf + l, n); host_log(logbuf); }
int trace_swi = 0; u32 watch_addr = 0;

/* ------------------------------------------------------------------ RMA */
static u32 rma_ptr = RMA_BASE;
static u32 rma_claim(u32 size) {
    size = (size + 15) & ~15u;
    if (rma_ptr + size > RMA_END) { host_log("RMA exhausted"); halted = 1; return 0; }
    u32 a = rma_ptr; rma_ptr += size; mem_set(mem + a, 0, size); return a;
}

/* ------------------------------------------------------------------ errors */
static u32 err_block = SYS_SCRATCH + 0x200;
static int set_error(u32 swi_num, u32 code, const char *msg) {
    wr32(err_block, code); int i = 0; while (msg[i] && i < 200) { wr8(err_block + 4 + i, msg[i]); i++; } wr8(err_block + 4 + i, 0);
    if (swi_num & 0x20000) { R[0] = err_block; psr |= PSR_V; return 1; }
    host_logn(msg, swi_num); host_log("(fatal: non-X SWI error)"); halted = 1; return 1;
}

/* ------------------------------------------------------------------ files */
#define MAX_FILES 160
struct fsfile { const char *name; const u8 *data; u32 size; u32 ftype; };
static struct fsfile files[MAX_FILES]; static int nfiles;
void fs_add(const char *name, const u8 *data, u32 size, u32 ftype) { if (nfiles < MAX_FILES) { files[nfiles].name = name; files[nfiles].data = data; files[nfiles].size = size; files[nfiles].ftype = ftype; nfiles++; } }

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int name_eq_ci(const char *a, const char *b) { while (*a && lower(*a) == lower(*b)) { a++; b++; } return lower(*a) == lower(*b); }

/* translate RISC OS path "RHFT:Graphics.Main" -> "!RHFT/Graphics/Main" */
static void resolve_path(const char *in, char *out) {
    const char *rest = in; char pre[64];
    const char *colon = 0; for (const char *p = in; *p; p++) if (*p == ':') { colon = p; break; }
    if (colon) { int n = (int)(colon - in); if (n > 60) n = 60; mem_cpy(pre, in, n); pre[n] = 0; rest = colon + 1; }
    else pre[0] = 0;
    char *o = out;
    if (name_eq_ci(pre, "RHFT")) str_cpy(o, "!RHFT/");
    else if (name_eq_ci(pre, "RHFTSounds")) str_cpy(o, "!RHFT/Sounds/");
    else if (name_eq_ci(pre, "RHFTMap")) str_cpy(o, "!RHFT/Maps/");
    else o[0] = 0;
    o += str_len(o);
    while (*rest) { char c = *rest++; if (c == '.') c = '/'; *o++ = c; }
    *o = 0;
}
static struct fsfile *fs_find(const char *riscos_name) {
    char path[256]; resolve_path(riscos_name, path);
    for (int i = 0; i < nfiles; i++) if (name_eq_ci(files[i].name, path)) return &files[i];
    host_log("file not found:"); host_log(path);
    return 0;
}

#define MAX_HANDLES 16
struct fh { struct fsfile *f; u32 ptr; int open; };
static struct fh handles[MAX_HANDLES];
static int fs_open(struct fsfile *f) {
    for (int i = 1; i < MAX_HANDLES; i++) if (!handles[i].open) { handles[i].open = 1; handles[i].f = f; handles[i].ptr = 0; return 0xF0 + i; }
    return 0;
}
static struct fh *fs_handle(u32 h) { h -= 0xF0; if (h >= 1 && h < MAX_HANDLES && handles[h].open) return &handles[h]; return 0; }
static void fs_close_all(void) { for (int i = 1; i < MAX_HANDLES; i++) handles[i].open = 0; }

/* ------------------------------------------------------------------ modules */
#define MAX_MODULES 8
struct module { u32 base; u32 size; u32 privword; u32 swichunk; const char *name; };
static struct module modules[MAX_MODULES]; static int nmodules;

static void cpu_call_svc(u32 addr) { cpu_call(addr, MODE_SVC); }

int module_load(const char *name, const u8 *img, u32 size) {
    u32 base = rma_claim(size + 4096);
    mem_cpy(mem + base, img, size);
    struct module *m = &modules[nmodules];
    m->base = base; m->size = size; m->name = name;
    m->privword = SYS_SCRATCH + 0x400 + (u32)nmodules * 4; wr32(m->privword, 0);
    m->swichunk = rd32(base + 0x1C); host_logn(name, base);
    u32 init = rd32(base + 4);
    if (init) {
        R[10] = SYS_SCRATCH + 0x300; wr8(R[10], 0); /* empty env string */
        R[11] = 0; R[12] = m->privword;
        cpu_call_svc(base + init);
        if (psr & PSR_V) { host_log("module init error:"); host_log(name); return -1; }
    }
    nmodules++;
    return nmodules - 1;
}

static struct module *module_for_swi(u32 num) {
    num &= ~0x20000u;
    for (int i = 0; i < nmodules; i++) if (modules[i].swichunk && num >= modules[i].swichunk && num < modules[i].swichunk + 64) return &modules[i];
    return 0;
}

/* call a *command provided by a module: name, tail */
int module_command(const char *cmd, const char *tail) {
    for (int mi = 0; mi < nmodules; mi++) {
        struct module *m = &modules[mi];
        u32 tab = rd32(m->base + 0x18); if (!tab) continue;
        u32 p = m->base + tab;
        while (rd8(p)) {
            char nm[64]; int n = 0; while (rd8(p + n) && n < 63) { nm[n] = rd8(p + n); n++; } nm[n] = 0;
            u32 q = (p + n + 1 + 3) & ~3u;
            u32 code = rd32(q);
            if (name_eq_ci(nm, cmd)) {
                if (!code) return -1;
                u32 ts = SYS_SCRATCH + 0x600; int i = 0; int args = 0, inword = 0;
                for (const char *t = tail; *t; t++) { wr8(ts + i++, (u8)*t); if (*t != ' ' && !inword) { inword = 1; args++; } if (*t == ' ') inword = 0; }
                wr8(ts + i, 13);
                R[0] = ts; R[1] = (u32)args; R[12] = m->privword;
                cpu_call_svc(m->base + code);
                if (psr & PSR_V) { host_log("command error:"); host_log(cmd); char eb[200]; rdstr((R[0] & 0x03FFFFFCu) + 4, eb, 200); host_log(eb); return -1; }
                return 0;
            }
            p = q + 16;
        }
    }
    host_log("command not found:"); host_log(cmd); return -1;
}

/* ------------------------------------------------------------------ VDU/screen */
static int vdu_bank = 1, disp_bank = 1;
int screen_mode = 0;
static int vdu_pending = 0; /* bytes pending after VDU 22 etc */
u32 screen_display_addr(void) { return SCREEN_BASE + (u32)(disp_bank - 1) * SCREEN_BANK; }
static u32 vdu_var(u32 n) {
    switch (n) {
    case 148: return SCREEN_BASE + (u32)(vdu_bank - 1) * SCREEN_BANK;   /* ScreenStart */
    case 149: return screen_display_addr();                              /* DisplayStart */
    case 150: return SCREEN_BANK * 2;                                    /* TotalScreenSize */
    case 0: return 0; case 1: return 0; case 2: return 319; case 3: return 255;
    case 4: return 2; case 5: return 2;                                   /* X/YEigFactor */
    case 6: return 320;                                                   /* LineLength */
    case 7: return SCREEN_BANK;                                           /* ScreenSize */
    case 8: return 0; case 9: return 3; case 10: return 3;                /* YShift, Log2BPP, Log2BPC */
    case 11: return 319; case 12: return 255;                             /* XWindLimit, YWindLimit */
    case 128: return 0; case 129: return 0; case 130: return 1279; case 131: return 1023;
    case 132: return 0; case 133: return 0; case 134: return 1279; case 135: return 1023;
    case 136: return 0; case 137: return 0;
    default: host_logn("unknown VDU var ", n); return 0;
    }
}
static void vdu_char(u32 c) {
    if (vdu_pending) { vdu_pending--; if (vdu_pending == 0 && screen_mode == -1) { screen_mode = (int)(c & 0x7F); host_logn("MODE ", c); } return; }
    if (c == 22) { vdu_pending = 1; screen_mode = -1; return; }
    if (c == 23) { vdu_pending = 9; return; }
    if (c == 19) { vdu_pending = 5; return; }
    if (c == 17 || c == 18) { vdu_pending = 1; return; }
    if (c == 31) { vdu_pending = 2; return; }
    if (c == 25) { vdu_pending = 5; return; }
    if (c >= 32 && c < 127) { char b[2] = { (char)c, 0 }; host_log(b); }
}

/* ------------------------------------------------------------------ keyboard */
u8 key_down[128];           /* indexed by RISC OS internal key number */
static u8 key_buf[64]; static int key_head, key_tail;
void key_push_char(u32 c) { int n = (key_tail + 1) & 63; if (n != key_head) { key_buf[key_tail] = (u8)c; key_tail = n; } }
void key_set(u32 code, u32 down) { if (code < 128) key_down[code] = (u8)down; }
static int any_key_from(int from) { for (int i = from; i < 128; i++) if (key_down[i]) return i; return 0xFF; }

/* ------------------------------------------------------------------ time */
u32 mono_time;              /* centiseconds, advanced by host */

/* ------------------------------------------------------------------ callbacks */
static u32 cb_addr[16], cb_r12[16]; static int ncb;
static void run_callbacks(void) {
    while (ncb > 0 && !halted) {
        u32 a = cb_addr[0], r12 = cb_r12[0];
        for (int i = 1; i < ncb; i++) { cb_addr[i - 1] = cb_addr[i]; cb_r12[i - 1] = cb_r12[i]; }
        ncb--;
        u32 save[16]; mem_cpy(save, R, sizeof(save));
        R[12] = r12; cpu_call_svc(a);
        mem_cpy(R, save, sizeof(save));
    }
}

/* ------------------------------------------------------------------ sound */
#define SND_CHANNELS 8
#define SND_BUFLEN 208          /* samples per channel per fill */
#define SND_PERIOD 48           /* microseconds -> 20833 Hz */
static u32 snd_handler_tab, snd_lin2log, snd_logscale, snd_sccb, snd_buf;
static u32 voices[32]; static int nvoices;
struct sndch { int voice; int active; int gate_pending; int update_pending; int flushing; };
static struct sndch sch[SND_CHANNELS];
int snd_enabled = 0;
static double exp2frac[4096];
double sound_pitch_base = 1024.0;  /* phase increment at pitch &4000 */
float snd_out[SND_BUFLEN];          /* mixed output of last generated buffer */

/* VIDC style 8-bit log sample: bit 0 sign, bits 4-1 mantissa, bits 7-5 chord */
static s32 log2lin(u8 b) {
    u32 chord = (b >> 5) & 7, mant = (b >> 1) & 15, sign = b & 1;
    s32 mag = (s32)((((mant << 1) + 33u) << chord) - 33u);
    return sign ? -mag : mag;   /* range +-8031 */
}
static u8 lin2log(s32 v) {
    u32 sign = v < 0; u32 mag = (u32)(v < 0 ? -v : v);
    mag = mag >> 18; /* 31-bit -> 13-bit: max 8191 */
    if (mag > 8031) mag = 8031;
    u32 m = mag + 33; u32 chord = 0; while ((m >> (chord + 1)) >= 32 && chord < 7) chord++;
    u32 mant = ((m >> (chord + 1)) - 16) & 15;
    return (u8)((chord << 5) | (mant << 1) | sign);
}

static void snd_init(void) {
    snd_handler_tab = SYS_SCRATCH + 0x800;
    snd_lin2log = SYS_SCRATCH + 0x4000;       /* 8K */
    snd_logscale = SYS_SCRATCH + 0x6000;      /* 256 */
    snd_sccb = SYS_SCRATCH + 0x7000;          /* 8 * 256 */
    snd_buf = SYS_SCRATCH + 0x8000;           /* 8 * 256 */
    wr32(snd_handler_tab, 0); wr32(snd_handler_tab + 4, 0); wr32(snd_handler_tab + 8, snd_lin2log); wr32(snd_handler_tab + 12, snd_logscale);
    for (u32 i = 0; i < 8192; i++) { s32 lin = (s32)(i << 19); wr8(snd_lin2log + i, lin2log(lin)); }
    for (u32 i = 0; i < 256; i++) wr8(snd_logscale + i, (u8)i);
    mem_set(mem + snd_sccb, 0, 8 * 256);
    double r = 1.0001692396774160; /* 2^(1/4096) */
    double v = 1.0; for (int i = 0; i < 4096; i++) { exp2frac[i] = v; v *= r; }
    for (int i = 0; i < SND_CHANNELS; i++) { sch[i].voice = -1; sch[i].active = 0; sch[i].gate_pending = 0; }
}

static u32 sound_pitch(u32 p) {
    if (p >= 0x8000) return p;
    u32 oct = (p >> 12) & 7, frac = p & 0xFFF;
    double n = sound_pitch_base * exp2frac[frac];
    int sh = (int)oct - 4;
    while (sh > 0) { n *= 2; sh--; }
    while (sh < 0) { n /= 2; sh++; }
    u32 fixed = (u32)(n * 65536.0 + 0.5);
    return fixed;
}

static int voice_find(const char *name) {
    for (int i = 1; i <= nvoices; i++) {
        u32 v = voices[i]; if (!v) continue;
        u32 nm = v + rd32(v + 28);
        char b[64]; rdstr(nm, b, 64);
        if (name_eq_ci(b, name)) return i;
    }
    return 0;
}

/* call a voice entry (offset 0 fill, 4 update, 8 gate on, 12 gate off) for channel ch */
static u32 voice_call(int ch, u32 entry) {
    u32 v = voices[sch[ch].voice];
    u32 sccb = snd_sccb + (u32)ch * 256;
    u32 buf = snd_buf + (u32)ch * 256;
    u32 save[16]; mem_cpy(save, R, sizeof(save));
    u32 save_psr = psr;
    for (int i = 0; i < 9; i++) R[i] = rd32(sccb + (u32)i * 4);
    R[7] = (u32)ch; R[8] = SND_PERIOD; R[9] = sccb; R[10] = buf + SND_BUFLEN; R[11] = 1; R[12] = buf;
    cpu_call2(v + entry, MODE_IRQ, 1);
    u32 flags = R[0];
    for (int i = 1; i < 9; i++) wr32(sccb + (u32)i * 4, R[i]);
    wr8(sccb + 3, (u8)flags);
    psr = save_psr;
    mem_cpy(R, save, sizeof(save));
    return flags;
}

/* generate one buffer (SND_BUFLEN samples) of mixed audio into snd_out */
void snd_generate(void) {
    s32 mix[SND_BUFLEN]; for (int i = 0; i < SND_BUFLEN; i++) mix[i] = 0;
    for (int ch = 0; ch < SND_CHANNELS; ch++) {
        struct sndch *c = &sch[ch];
        if (c->voice < 0 || !snd_enabled) continue;
        if (!c->active && !c->gate_pending) continue;
        u32 entry = c->gate_pending ? 8 : (c->update_pending ? 4 : 0);
        c->gate_pending = 0; c->update_pending = 0;
        u32 flags = voice_call(ch, entry);
        if (trace_swi) { host_logn("voice ch ", (u32)ch); host_logn("  entry ", entry); host_logn("  flags ", flags); host_logn("  stasis flags ", rd32(rd32(modules[2].privword) + ((u32)ch << 7) + 0x4c + 0x1c)); }
        c->active = (flags & 8) ? 1 : 0;
        if (!c->active) { /* zero the rest? the voice should have filled silence */ }
        u32 buf = snd_buf + (u32)ch * 256;
        { u32 mx = 0; for (int i = 0; i < SND_BUFLEN; i++) { u8 b = rd8(buf + (u32)i); if (b > mx) mx = b; mix[i] += log2lin(b); } if (trace_swi) { host_logn("  maxbyte ", mx); host_logn("  pos ", rd32(rd32(modules[2].privword) + ((u32)ch << 7) + 0x4c + 0x10)); host_logn("  step ", rd32(rd32(modules[2].privword) + ((u32)ch << 7) + 0x4c + 0x18)); host_logn("  start ", rd32(rd32(modules[2].privword) + ((u32)ch << 7) + 0x4c + 8)); host_logn("  len ", rd32(rd32(modules[2].privword) + ((u32)ch << 7) + 0x4c + 0xc)); host_logn("  vol ", rd32(rd32(modules[2].privword) + ((u32)ch << 7) + 0x4c + 0x28)); } }
        /* fill counter (duration) */
        u32 cnt = rd32(snd_sccb + (u32)ch * 256 + 12);
        if (cnt > 0 && cnt < 0x10000000u) { cnt--; wr32(snd_sccb + (u32)ch * 256 + 12, cnt); if (cnt == 0 && c->active) { voice_call(ch, 12); c->active = 0; } }
    }
    for (int i = 0; i < SND_BUFLEN; i++) { float f = (float)mix[i] / 16384.0f; if (f > 1) f = 1; if (f < -1) f = -1; snd_out[i] = f; }
}

static void sound_control(u32 chn, u32 amp, u32 pitch, u32 dur) {
    int ch = (int)(chn - 1) & 7;
    u32 sccb = snd_sccb + (u32)ch * 256;
    u32 a7;
    if (amp >= 0x100 && amp <= 0x1FF) a7 = amp & 0x7F; else if (amp == 0 || amp >= 0xFFF1) a7 = (u32)(0x7F - ((0x10000 - amp) & 0xF) * 8); else a7 = 0x7F;
    u32 gate_update = (amp >= 0x100 && amp <= 0x1FF) ? ((amp >> 7) & 1) : 0;
    wr8(sccb + 0, (u8)a7);
    wr8(sccb + 1, (u8)sch[ch].voice);
    if (pitch < 0x100) pitch = 0x4000 + ((s32)pitch - 53) * 0x1000 / 48;
    u32 ph = sound_pitch(pitch);
    wr32(sccb + 4, ph >> 16);
    wr32(sccb + 8, 0);
    u32 fills;
    if (dur == 0xFF) fills = 0xF0000000u; else fills = dur * 5; /* 5cs = 50ms = 5 buffers of ~10ms */
    if (fills == 0) fills = 1;
    wr32(sccb + 12, fills);
    if (sch[ch].voice >= 0) { if (gate_update && sch[ch].active) sch[ch].update_pending = 1; else sch[ch].gate_pending = 1; }
}

/* ------------------------------------------------------------------ SWI dispatch */
static u32 poll_count;
static void swi_os_byte(u32 num) {
    u32 a = R[0] & 0xFF;
    if (a == 0x7A || a == 0x79 || a == 0x81) { if (++poll_count > 400) { poll_count = 0; yield_flag = 1; } }
    switch (a) {
    case 0x13: yield_flag = 1; poll_count = 0; break;  /* wait vsync */
    case 0x70: vdu_bank = (int)(R[1] ? R[1] : 1); R[1] = 1; break;
    case 0x71: disp_bank = (int)(R[1] ? R[1] : 1); R[1] = 1; break;
    case 0x7A: R[1] = (u32)any_key_from(0x10); break; /* keyboard scan from &10 */
    case 0x79: R[1] = (u32)any_key_from((int)(R[1] & 0x7F)); break;
    case 0x7C: break;                                  /* clear escape */
    case 0x7D: break; case 0x7E: break;
    case 0x81: {
        if (R[2] == 0xFF) { u32 k = (0xFF - R[1]) & 0x7F; R[1] = R[2] = key_down[k] ? 0xFF : 0; }
        else { /* read key with timeout */
            if (key_head != key_tail) { R[1] = key_buf[key_head]; key_head = (key_head + 1) & 63; R[2] = 0; psr &= ~PSR_C; }
            else { R[2] = 0xFF; psr |= PSR_C; }
        }
        break;
    }
    case 0x0F: key_head = key_tail; break;            /* flush buffers */
    case 0x15: key_head = key_tail; break;
    case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xC4: case 0xC5: case 0xC6: case 0x04: case 0xDC: case 0x0B: case 0x0C: break;
    case 0xA1: R[1] = 0; R[2] = 0; break;              /* read CMOS */
    default: if (trace_swi) host_logn("OS_Byte ", a); break;
    }
    (void)num;
}

static void swi_os_file(u32 num) {
    char name[256]; rdstr(R[1], name, 256);
    u32 r0 = R[0];
    if (r0 == 5 || r0 == 17 || r0 == 20 || r0 == 15 || r0 == 13 || r0 == 23) { /* read catalogue info */
        struct fsfile *f = fs_find(name);
        if (!f) { R[0] = 0; return; }
        R[0] = 1; R[2] = 0xFFF00000u | (f->ftype << 8); R[3] = 0; R[4] = f->size; R[5] = 3; R[6] = f->ftype;
        return;
    }
    if (r0 == 255 || r0 == 16 || r0 == 12 || r0 == 14) { /* load */
        struct fsfile *f = fs_find(name);
        if (!f) { set_error(num, 0xD6, "File not found"); return; }
        u32 addr = R[2]; if ((r0 == 255 || r0 == 16) && R[3] != 0) addr = R[2]; /* R3=0 -> use R2 */
        mem_cpy(mem + (addr & MEM_MASK), f->data, f->size);
        R[0] = 1; R[2] = 0xFFF00000u | (f->ftype << 8); R[3] = 0; R[4] = f->size; R[5] = 3; R[6] = f->ftype;
        return;
    }
    if (r0 == 18 || r0 == 0 || r0 == 10 || r0 == 1 || r0 == 2 || r0 == 3 || r0 == 4 || r0 == 9 || r0 == 19) { host_logn("OS_File write ignored, reason ", r0); return; }
    host_logn("OS_File unsupported reason ", r0);
}

static void swi_os_find(u32 num) {
    u32 r0 = R[0];
    if (r0 == 0) { struct fh *h = fs_handle(R[1]); if (h) h->open = 0; else fs_close_all(); return; }
    u32 reason = r0 & 0xC0;
    if (reason == 0x40 || (reason == 0 && (r0 & 0xFF) != 0) || reason == 0xC0) {
        char name[256]; rdstr(R[1], name, 256);
        struct fsfile *f = fs_find(name);
        if (!f) { if (r0 & 8) set_error(num, 0xD6, "File not found"); else R[0] = 0; return; }
        R[0] = (u32)fs_open(f); return;
    }
    set_error(num, 0xD6, "Cannot open for output");
}

static void swi_os_gbpb(u32 num) {
    u32 r0 = R[0];
    if (r0 == 3 || r0 == 4) {
        struct fh *h = fs_handle(R[1]); if (!h) { set_error(num, 0xDE, "Channel"); return; }
        if (r0 == 3) h->ptr = R[4];
        u32 n = R[3], p = R[2]; u32 done = 0;
        while (n > 0 && h->ptr < h->f->size) { wr8(p++, h->f->data[h->ptr++]); n--; done++; }
        R[2] = p; R[3] = n; R[4] = h->ptr;
        if (n) psr |= PSR_C; else psr &= ~PSR_C;
        return;
    }
    set_error(num, 0xDE, "OS_GBPB unsupported");
}

static void swi_os_args(u32 num) {
    struct fh *h = fs_handle(R[1]);
    switch (R[0]) {
    case 0: if (h) R[2] = h->ptr; break;
    case 1: if (h) h->ptr = R[2]; break;
    case 2: if (h) R[2] = h->f->size; break;
    case 5: if (h) R[2] = (h->ptr >= h->f->size); break;
    default: (void)num; break;
    }
}

static void swi_os_module(u32 num) {
    switch (R[0]) {
    case 6: { u32 a = rma_claim(R[3]); R[2] = a; break; }
    case 7: break; /* free: ignored */
    case 13: { /* extend block: allocate new and copy */
        u32 old = R[2]; u32 oldsize = 0; (void)oldsize; u32 a = rma_claim((u32)((s32)R[3] + 0x10000)); mem_cpy(mem + a, mem + (old & MEM_MASK), 0x10000); R[2] = a; break; }
    default: host_logn("OS_Module reason ", R[0]); (void)num; break;
    }
}

static void swi_convert_int(u32 num, int signed_) {
    u32 v = R[0]; u32 buf = R[1]; u32 len = R[2]; char tmp[16]; int n = 0;
    int neg = signed_ && (s32)v < 0; u32 m = neg ? (u32)(-(s32)v) : v;
    if (m == 0) tmp[n++] = '0';
    while (m) { tmp[n++] = (char)('0' + m % 10); m /= 10; }
    if (neg) tmp[n++] = '-';
    u32 i = 0; while (n > 0 && i + 1 < len) { wr8(buf + i, (u8)tmp[--n]); i++; }
    wr8(buf + i, 0);
    R[0] = buf; R[1] = buf + i; R[2] = len - i;
    (void)num;
}

static void swi_read_unsigned(u32 num) {
    u32 base = R[0] & 0xFF; if (base == 0) base = 10;
    u32 p = R[1]; u32 v = 0; int any = 0;
    while (rd8(p) == ' ') p++;
    u32 b2 = base;
    if (rd8(p) == '&') { b2 = 16; p++; }
    else if (rd8(p + 1) == '_' && rd8(p) >= '2' && rd8(p) <= '9') { b2 = rd8(p) - '0'; p += 2; }
    for (;;) {
        u32 c = rd8(p); u32 d;
        if (c >= '0' && c <= '9') d = c - '0'; else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10; else if (c >= 'a' && c <= 'z') d = c - 'a' + 10; else break;
        if (d >= b2) break;
        v = v * b2 + d; p++; any = 1;
    }
    if (!any) { set_error(num, 0x11C, "Bad number"); return; }
    R[1] = p; R[2] = v;
}

static void swi_sound(u32 num) {
    u32 n = num & ~0x20000u;
    switch (n) {
    case 0x40140: { /* Sound_Configure */
        u32 oc = SND_CHANNELS, ol = SND_BUFLEN, op = SND_PERIOD;
        if (R[0]) host_logn("Sound_Configure channels ", R[0]);
        R[0] = oc; R[1] = ol; R[2] = op; R[3] = snd_handler_tab; R[4] = 0; break; }
    case 0x40141: { u32 old = (u32)snd_enabled; if (R[0] == 1) snd_enabled = 0; else if (R[0] == 2) snd_enabled = 1; R[0] = old ? 2 : 1; break; }
    case 0x40142: case 0x40143: break;
    case 0x40180: R[0] = 127; break;                 /* Sound_Volume */
    case 0x40181: R[0] = lin2log((s32)R[0]); break;  /* Sound_SoundLog */
    case 0x40182: R[0] = R[0] & 0xFF; break;         /* Sound_LogScale */
    case 0x40183: { /* InstallVoice: R0 = voice table, R1 = slot (0 = allocate) */
        u32 slot = R[1]; if (slot == 0) { slot = (u32)(++nvoices); } else if ((int)slot > nvoices) nvoices = (int)slot;
        voices[slot] = R[0] & 0x03FFFFFCu; R[1] = slot;
        /* call Install entry (offset 24)? Many voices have no install; PRM says install called with R0 = voice number */
        break; }
    case 0x40184: break; /* RemoveVoice */
    case 0x40185: { /* AttachVoice: R0 = channel, R1 = voice number */
        int ch = (int)(R[0] - 1) & 7; int old = sch[ch].voice; sch[ch].voice = (int)R[1] ? (int)R[1] : -1; sch[ch].active = 0;
        if (sch[ch].voice > 0) { u32 v = voices[sch[ch].voice]; u32 s[16]; mem_cpy(s, R, 64); R[0] = (u32)ch; cpu_call2(v + 16, MODE_SVC, 1); mem_cpy(R, s, 64); }
        R[1] = old > 0 ? (u32)old : 0; break; }
    case 0x40186: break; /* ControlPacked */
    case 0x40187: R[0] = 0x4000; break; /* Tuning */
    case 0x40188: R[0] = sound_pitch(R[0]); break;
    case 0x40189: sound_control(R[0], R[1], R[2], R[3]); break;
    case 0x4018A: { /* AttachNamedVoice: R0 = channel, R1 = name */
        char nm[64]; rdstr(R[1], nm, 64); int vi = voice_find(nm);
        if (!vi) { set_error(num, 0x400, "No such voice"); return; }
        int ch = (int)(R[0] - 1) & 7; sch[ch].voice = vi; sch[ch].active = 0;
        u32 v = voices[vi]; u32 s[16]; mem_cpy(s, R, 64); R[0] = (u32)ch; cpu_call2(v + 16, MODE_SVC, 1); mem_cpy(R, s, 64);
        break; }
    case 0x4018B: { int ch = (int)(R[0] - 1) & 7; R[2] = rd32(snd_sccb + (u32)ch * 256 + (R[1] & 0xFC)); break; }
    case 0x4018C: { int ch = (int)(R[0] - 1) & 7; wr32(snd_sccb + (u32)ch * 256 + (R[1] & 0xFC), R[2]); break; }
    case 0x401C0: case 0x401C1: case 0x401C2: case 0x401C3: case 0x401C4: case 0x401C5: case 0x401C6: case 0x401C7: break; /* scheduler */
    default: host_logn("Sound SWI ", n); break;
    }
}

static void swi_write0(u32 a) { char b[256]; u32 n = rdstr(a, b, 256); (void)n; host_log(b); }

void os_swi(u32 num) {
    u32 n = num & ~0x20000u;
    if (watch_addr) { for (int i = 0; i < 8; i++) host_logn("   watch ", rd32(watch_addr + (u32)i * 4)); }
    if (trace_swi) { host_logn("SWI ", num); host_logn("  r0 ", R[0]); host_logn("  r1 ", R[1]); host_logn("  r2 ", R[2]); host_logn("  r3 ", R[3]); }
    if (n >= 0x100 && n < 0x200) { vdu_char(n - 0x100); return; }
    struct module *m = module_for_swi(n);
    if (m) {
        u32 handler = rd32(m->base + 0x20);
        if (!handler) { set_error(num, 0x1E6, "SWI not known"); return; }
        u32 s10 = R[10], s11 = R[11], s12 = R[12];
        R[11] = n - m->swichunk; R[12] = m->privword;
        /* module SWI handler returns via MOVS PC,R14; V set => error */
        cpu_call_svc(m->base + handler);
        R[10] = s10; R[11] = s11; R[12] = s12;
        if (trace_swi) { host_logn("  => r0 ", R[0]); host_logn("  => r1 ", R[1]); host_logn("  => r2 ", R[2]); host_logn("  => psr ", psr); }
        if ((psr & PSR_V) && !(num & 0x20000)) {
            char eb[200]; rdstr((R[0] & 0x03FFFFFCu) + 4, eb, 200); host_log("module SWI error:"); host_log(eb); host_logn("SWI ", num); halted = 1;
        }
        return;
    }
    switch (n) {
    case 0x00: vdu_char(R[0]); break;
    case 0x01: { /* OS_WriteS: inline string after SWI */
        u32 p = cpu_pc; char b[256]; u32 len = rdstr(p, b, 256); host_log(b);
        /* skip string + terminator, word align */
        u32 e = p; while (rd8(e)) e++; e++; cpu_pc = (e + 3) & ~3u; break; }
    case 0x02: swi_write0(R[0]); { u32 e = R[0]; while (rd8(e)) e++; R[0] = e + 1; } break;
    case 0x03: break;
    case 0x05: { char b[256]; rdstr(R[0], b, 256); host_log("OS_CLI:"); host_log(b); break; }
    case 0x06: swi_os_byte(num); break;
    case 0x07: break; /* OS_Word */
    case 0x08: swi_os_file(num); break;
    case 0x09: swi_os_args(num); break;
    case 0x0A: { struct fh *h = fs_handle(R[1]); if (h && h->ptr < h->f->size) { R[0] = h->f->data[h->ptr++]; psr &= ~PSR_C; } else { R[0] = 0xFE; psr |= PSR_C; } break; }
    case 0x0C: swi_os_gbpb(num); break;
    case 0x0D: swi_os_find(num); break;
    case 0x10: R[0] = SYS_SCRATCH + 0x300; R[1] = 0x400000; R[2] = SYS_SCRATCH + 0x300; break;
    case 0x11: host_log("OS_Exit called"); halted = 1; break;
    case 0x13: case 0x14: break;
    case 0x1E: swi_os_module(num); break;
    case 0x1F: case 0x20: break; /* OS_Claim / OS_Release vectors: ignored */
    case 0x21: swi_read_unsigned(num); break;
    case 0x22: break;
    case 0x23: set_error(num, 0x124, "System variable not found"); break;
    case 0x24: break;
    case 0x29: if (R[0] == 22) fs_close_all(); break;
    case 0x2B: { char eb[200]; rdstr((R[0] & 0x03FFFFFCu) + 4, eb, 200); host_log("OS_GenerateError:"); host_log(eb); halted = 1; break; }
    case 0x2C: psr &= ~PSR_C; break; /* no escape */
    case 0x30: { /* OS_ServiceCall: offer to every module */
        for (int i = 0; i < nmodules && R[1] != 0; i++) {
            u32 svc = rd32(modules[i].base + 0xC); if (!svc) continue;
            u32 s10 = R[10], s11 = R[11], s12 = R[12];
            R[12] = modules[i].privword; cpu_call_svc(modules[i].base + svc);
            R[10] = s10; R[11] = s11; R[12] = s12;
        }
        break; }
    case 0x31: { u32 in = R[0] & 0x03FFFFFCu, out = R[1] & 0x03FFFFFCu; for (;;) { u32 v = rd32(in); if (v == 0xFFFFFFFFu) break; wr32(out, vdu_var(v)); in += 4; out += 4; } break; }
    case 0x35: R[2] = vdu_var(R[1]); break;
    case 0x36: case 0x37: break;
    case 0x3A: psr &= ~PSR_C; break; /* OS_ValidateAddress: valid */
    case 0x42: R[0] = mono_time; break;
    case 0x44: swi_write0(R[0]); break;
    case 0x46: { char b[256]; u32 l = R[1]; if (l > 255) l = 255; for (u32 i = 0; i < l; i++) b[i] = (char)rd8(R[0] + i); b[l] = 0; host_log(b); break; }
    case 0x54: if (ncb < 16) { cb_addr[ncb] = R[0]; cb_r12[ncb] = R[1]; ncb++; } break;
    case 0x3B: case 0x3C: case 0x3D: break; /* CallAfter/CallEvery/RemoveTicker */
    case 0xD0: case 0xD1: case 0xD2: case 0xD3: case 0xD4: { u32 v = R[0], buf = R[1], len = R[2]; int digits = (n == 0xD0) ? 1 : (n == 0xD1) ? 2 : (n == 0xD2) ? 4 : (n == 0xD3) ? 6 : 8; char t[9]; hex8(t, v); u32 i = 0; for (int k = 8 - digits; k < 8 && i + 1 < len; k++) wr8(buf + i++, (u8)t[k]); wr8(buf + i, 0); R[0] = buf; R[1] = buf + i; R[2] = len - i; break; }
    case 0xD5: case 0xD6: case 0xD7: case 0xD8: swi_convert_int(num, 0); break;
    case 0xD9: case 0xDA: case 0xDB: case 0xDC: swi_convert_int(num, 1); break;
    default:
        if (n >= 0x40140 && n < 0x40200) { swi_sound(num); break; }
        host_logn("Unimplemented SWI ", num);
        set_error(num, 0x1E6, "SWI not known");
        break;
    }
    if ((psr & 3) == MODE_USR) run_callbacks();
}

/* ------------------------------------------------------------------ public API */
void os_init(void) {
    rma_ptr = RMA_BASE; nfiles = 0; nmodules = 0; ncb = 0; nvoices = 0;
    for (int i = 0; i < MAX_HANDLES; i++) handles[i].open = 0;
    for (int i = 0; i < 32; i++) voices[i] = 0;
    vdu_bank = disp_bank = 1; screen_mode = 0; vdu_pending = 0;
    key_head = key_tail = 0; for (int i = 0; i < 128; i++) key_down[i] = 0;
    mono_time = 0; snd_enabled = 0; poll_count = 0;
    snd_init();
}
void os_frame_tick(void) { mono_time += 2; }
int os_screen_mode(void) { return screen_mode; }
