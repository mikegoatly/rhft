/* Machine setup and host-facing API for the RHFT emulator */
#include "arm.h"

extern void fs_add(const char *name, const u8 *data, u32 size, u32 ftype);
extern int module_load(const char *name, const u8 *img, u32 size);
extern int module_command(const char *cmd, const char *tail);
extern void os_init(void);
extern void os_frame_tick(void);
extern u32 screen_display_addr(void);
extern void snd_generate(void);
extern float snd_out[];
extern int trace_swi;
extern u32 mono_time;
extern void key_set(u32 code, u32 down);
extern void key_push_char(u32 c);
extern int snd_enabled;
extern double sound_pitch_base;

static u8 memory_pool[MEM_SIZE + 64];
#define PACK_MAX (1024 * 1024)
static u8 pack[PACK_MAX];
static u32 pack_len;

/* exported for the host: where to put the data pack */
u8 *rhft_pack_buffer(void) { return pack; }
u32 rhft_pack_capacity(void) { return PACK_MAX; }

struct pent { u32 name_off, data_off, size, ftype; };

static const u8 *find_file(const char *name, u32 *size) {
    u32 n = *(u32 *)pack;
    const struct pent *e = (const struct pent *)(pack + 4);
    for (u32 i = 0; i < n; i++) if (str_eq((const char *)pack + e[i].name_off, name)) { *size = e[i].size; return pack + e[i].data_off; }
    return 0;
}

static int booted;
static const char *sound_slots[][2] = {
    {"1", "RHFTSounds:aieee!"}, {"2", "RHFTSounds:clomping"}, {"3", "RHFTSounds:explosion"}, {"4", "RHFTSounds:hup"},
    {"5", "RHFTSounds:crack"}, {"6", "RHFTSounds:ooff"}, {"8", "RHFTSounds:oooohhh"}, {"9", "RHFTSounds:pop"},
    {"10", "RHFTSounds:pusheduh"}, {"11", "RHFTSounds:ready?"}, {"12", "RHFTSounds:switch"}, {"13", "RHFTSounds:teleport"},
    {"14", "RHFTSounds:telerev"}, {"15", "RHFTSounds:extralife"}, {"16", "RHFTSounds:uh-oh"}, {"17", "RHFTSounds:uhh"},
    {"18", "RHFTSounds:creak"}, {"21", "RHFTSounds:yeah"}, {"22", "RHFTSounds:collect"}, {"23", "RHFTSounds:bounce"}, {"24", "RHFTSounds:burn"},
    {0, 0}
};

int rhft_boot(u32 packlen) {
    pack_len = packlen;
    mem = memory_pool;
    mem_set(mem, 0, MEM_SIZE);
    cpu_init();
    os_init();
    u32 n = *(u32 *)pack;
    const struct pent *e = (const struct pent *)(pack + 4);
    for (u32 i = 0; i < n; i++) fs_add((const char *)pack + e[i].name_off, pack + e[i].data_off, e[i].size, e[i].ftype);
    static const char *mods[] = { "FastSpr", "Amnesia", "Stasis", "StasisRaw", "Blitz" };
    for (int i = 0; i < 5; i++) {
        char path[64]; str_cpy(path, "!RHFT/Resources/"); str_cpy(path + str_len(path), mods[i]);
        u32 sz; const u8 *img = find_file(path, &sz);
        if (!img) { host_log("missing module"); host_log(path); return -1; }
        if (module_load(mods[i], img, sz) < 0) return -1;
        if (halted) return -1;
    }
    /* !Run2 */
    char tail[16];
    for (int i = 1; i <= 8; i++) { tail[0] = (char)('0' + i); tail[1] = ' '; tail[2] = (char)('0' + i); tail[3] = 0; if (module_command("StasisLink", tail) < 0) return -1; }
    for (int i = 0; sound_slots[i][0]; i++) {
        char t[80]; str_cpy(t, sound_slots[i][0]); int l = str_len(t); t[l] = ' '; str_cpy(t + l + 1, sound_slots[i][1]);
        if (module_command("StasisLoad", t) < 0) return -1;
        if (halted) return -1;
    }
    /* load the game */
    u32 gsz; const u8 *g = find_file("!RHFT/Code/Game", &gsz);
    if (!g) { host_log("missing Game"); return -1; }
    mem_cpy(mem + APP_BASE, g, gsz);
    cpu_set_mode(MODE_USR);
    for (int i = 0; i < 15; i++) R[i] = 0;
    R[13] = 0x400000; /* overwritten by the game immediately */
    cpu_pc = APP_BASE;
    psr = (psr & 3);
    booted = 1;
    host_log("boot complete");
    return 0;
}

/* run one video frame (until the game waits for vsync). returns 0 ok, 1 budget exhausted, 2 halted */
int rhft_frame(void) {
    if (!booted || halted) return 2;
    int r = cpu_run(20000000);
    os_frame_tick();
    return r == 2 ? 2 : (r == 0 ? 1 : 0);
}

/* audio: generate n buffers of 208 samples into out (float) */
void rhft_audio(float *out, int nbuf) {
    for (int b = 0; b < nbuf; b++) { snd_generate(); for (int i = 0; i < 208; i++) out[b * 208 + i] = snd_out[i]; }
}

/* host adds/overrides a file after boot (e.g. a custom map pack); name and data must stay valid (inside the pack buffer) */
void rhft_fs_add(const char *name, const u8 *data, u32 size) { fs_add(name, data, size, 0xFFD); }

const u8 *rhft_screen(void) { return mem + (screen_display_addr() & MEM_MASK); }
u8 *rhft_mem(void) { return mem; }
void rhft_key(u32 code, u32 down) { key_set(code, down); }
void rhft_char(u32 c) { key_push_char(c); }
int rhft_halted(void) { return halted; }
u32 rhft_icount_lo(void) { return (u32)icount; }
void rhft_set_trace(int t) { trace_swi = t; }
void rhft_set_pitch_base(double b) { sound_pitch_base = b; }
int rhft_sound_on(void) { return snd_enabled; }
u32 rhft_pc(void) { return cpu_pc; }
u32 rhft_reg(int i) { return R[i & 15]; }
