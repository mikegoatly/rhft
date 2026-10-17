#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arm.h"
extern u8 *rhft_pack_buffer(void); extern int rhft_boot(u32); extern int rhft_frame(void); extern const u8 *rhft_screen(void);
extern void rhft_key(u32,u32); extern void rhft_char(u32); extern void rhft_audio(float*,int); extern void rhft_set_trace(int); extern u32 rhft_pc(void); extern u32 rhft_reg(int);
extern u32 trace_ring[4096]; extern u32 trace_pos;
void dump_trace(int n) { for (int i = n; i > 0; i--) fprintf(stderr, "%08X ", trace_ring[(trace_pos - i) & 4095]); fprintf(stderr, "\n"); }
void host_log(const char *s) { fprintf(stderr, "[log] %s\n", s); if (strstr(s, "Undefined") || strstr(s, "runaway") || strstr(s, "misaligned")) dump_trace(40); }
static void save_ppm(const char *fn, const u8 *scr) {
    FILE *f = fopen(fn, "wb"); fprintf(f, "P6 320 256 255\n");
    for (int i = 0; i < 320*256; i++) { u8 p = scr[i];
        int t = p & 3; int r = ((p & 0x10) ? 8 : 0) | ((p & 4) ? 4 : 0) | t; int g = ((p & 0x40) ? 8 : 0) | ((p & 0x20) ? 4 : 0) | t; int b = ((p & 0x80) ? 8 : 0) | ((p & 8) ? 4 : 0) | t;
        fputc(r * 17, f); fputc(g * 17, f); fputc(b * 17, f); }
    fclose(f);
}
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); u8 *pb = rhft_pack_buffer(); u32 n = fread(pb, 1, 1 << 20, f); fclose(f);
    int frames = argc > 2 ? atoi(argv[2]) : 100;
    const char *script = argc > 3 ? argv[3] : "";
    if (getenv("TRACE")) rhft_set_trace(1);
    { extern u32 watch_addr; if (getenv("WATCH")) watch_addr = strtoul(getenv("WATCH"), 0, 16); }
    if (rhft_boot(n)) { fprintf(stderr, "boot failed\n"); return 1; }
    if (getenv("SLOTS")) { void slot_test(void); slot_test(); return 0; }
    if (getenv("HAMMER")) { void hammer_test(const char*, const char*); hammer_test(getenv("HAMMER"), "/tmp/hammer_out.bin"); return 0; }
    /* script: comma separated frame:keycode:downflag or frame:c:char */
    char *sc = strdup(script); char *tok = strtok(sc, ",");
    struct ev { int fr; int key; int down; } evs[256]; int nev = 0;
    while (tok) { int a, b, c; if (sscanf(tok, "%d:%d:%d", &a, &b, &c) == 3) { evs[nev].fr = a; evs[nev].key = b; evs[nev].down = c; nev++; } tok = strtok(0, ","); }
    static float audio[416]; int sndframes = 0; FILE *af = getenv("AUDIO") ? fopen(getenv("AUDIO"), "wb") : 0;
    for (int fr = 0; fr < frames; fr++) {
        for (int i = 0; i < nev; i++) if (evs[i].fr == fr) { if (evs[i].down == 2) rhft_char((u32)evs[i].key); else rhft_key((u32)evs[i].key, (u32)evs[i].down); }
        if (getenv("TRACEFROM") && fr == atoi(getenv("TRACEFROM"))) rhft_set_trace(1);
        if (getenv("TRACEFROM") && fr == atoi(getenv("TRACEFROM")) + 2) rhft_set_trace(0);
        int r = rhft_frame();
        if (getenv("PCLOG")) fprintf(stderr, "frame %d end pc=%08X\n", fr, rhft_pc());
        rhft_audio(audio, 2); if (af) fwrite(audio, sizeof(float), 416, af);
        if (r == 2) { fprintf(stderr, "halted at frame %d pc=%08X\n", fr, rhft_pc()); for (int i = 0; i < 16; i++) fprintf(stderr, "r%d=%08X ", i, rhft_reg(i)); fprintf(stderr, "\n"); break; }
        if (r == 1) fprintf(stderr, "frame %d: budget exhausted pc=%08X\n", fr, rhft_pc());
        if (getenv("SHOTS") && (fr % atoi(getenv("SHOTS")) == 0)) { char fn[64]; sprintf(fn, "%s/f%04d.ppm", getenv("SHOTDIR") ? getenv("SHOTDIR") : "shots", fr); save_ppm(fn, rhft_screen()); }
        (void)sndframes;
    }
    save_ppm("shots/last.ppm", rhft_screen());
    if (af) fclose(af);
    return 0;
}
/* test hook: decompress a file via Blitz into buffer and dump */
void hammer_test(const char *name, const char *outfile) {
    u32 nm = 0x300000; for (int i = 0; name[i]; i++) wr8(nm + i, name[i]); wr8(nm + str_len(name), 0);
    u32 buf = 0x310000;
    R[0] = 0; R[1] = nm; R[2] = 0; R[3] = 0; os_swi(0x69645);
    fprintf(stderr, "HammerOp(0): r0=%08X r3=%08X psr=%08X\n", R[0], R[3], psr);
    u32 size = R[3];
    R[0] = 1; R[1] = nm; R[2] = 0; R[3] = buf; os_swi(0x69645);
    fprintf(stderr, "HammerOp(1): r0=%08X psr=%08X\n", R[0], psr);
    if (psr & PSR_V) { char eb[200]; rdstr((R[0] & 0x3FFFFFC) + 4, eb, 200); fprintf(stderr, "err: %s\n", eb); }
    FILE *f = fopen(outfile, "wb"); fwrite(mem + buf, 1, size, f); fclose(f);
}
void slot_test(void) {
    for (u32 slot = 1; slot < 25; slot++) { R[0] = slot; os_swi(0x67D4C); fprintf(stderr, "slot %u: r1=%08X r2=%08X r3=%08X r4=%08X r5=%08X r6=%08X V=%d\n", slot, R[1], R[2], R[3], R[4], R[5], R[6], !!(psr & PSR_V)); if (psr & PSR_V) { char eb[200]; rdstr((R[0] & 0x3FFFFFC) + 4, eb, 200); fprintf(stderr, "   err: %s\n", eb); } }
}
