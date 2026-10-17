/* Minimal ARM2/ARM3-style (26-bit) CPU + RISC OS environment for RHFT */
#ifndef ARM_H
#define ARM_H

typedef unsigned int u32;
typedef int s32;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long long u64;
typedef long long s64;

#define MEM_SIZE (32u << 20)
#define MEM_MASK (MEM_SIZE - 1)

/* memory map (emulated) */
#define APP_BASE      0x00008000u
#define RMA_BASE      0x01800000u
#define RMA_END       0x01C00000u
#define SVC_STACK_TOP 0x01C08000u
#define IRQ_STACK_TOP 0x01C10000u
#define SYS_SCRATCH   0x01C20000u   /* misc OS structures */
#define SCREEN_BASE   0x01FD8000u
#define SCREEN_BANK   0x00014000u   /* 80K per bank (mode 13) */
#define MAGIC_RET     0x03FFFF00u   /* "return to host" address */

#define PSR_N 0x80000000u
#define PSR_Z 0x40000000u
#define PSR_C 0x20000000u
#define PSR_V 0x10000000u
#define PSR_I 0x08000000u
#define PSR_F 0x04000000u
#define MODE_USR 0
#define MODE_FIQ 1
#define MODE_IRQ 2
#define MODE_SVC 3

extern u8 *mem;
extern u32 R[16];
extern u32 psr;
extern u32 cpu_pc;
extern u64 icount;
extern int halted;

void cpu_init(void);
void cpu_set_mode(u32 mode);
u32 cpu_mode(void);
int cpu_run(long max_instr); /* returns 0 normal, 1 yield, 2 halted */
void cpu_call(u32 addr, u32 mode); /* nested call until MAGIC_RET */
void cpu_call2(u32 addr, u32 mode, int push_ret);
int cpu_step_one(void);

static inline u32 rd32(u32 a) { a &= MEM_MASK & ~3u; return *(u32 *)(mem + a); }
static inline void wr32(u32 a, u32 v) { a &= MEM_MASK & ~3u; *(u32 *)(mem + a) = v; }
static inline u8 rd8(u32 a) { return mem[a & MEM_MASK]; }
static inline void wr8(u32 a, u8 v) { mem[a & MEM_MASK] = v; }

/* host hooks */
void host_log(const char *s);
void host_logn(const char *s, u32 n);
void os_swi(u32 num); /* dispatch */
extern int yield_flag;

/* string helpers */
int str_len(const char *s);
void str_cpy(char *d, const char *s);
int str_eq(const char *a, const char *b);
void mem_cpy(void *d, const void *s, u32 n);
void mem_set(void *d, int c, u32 n);
u32 rdstr(u32 a, char *buf, int max); /* read 0/ctrl terminated string */

#endif
