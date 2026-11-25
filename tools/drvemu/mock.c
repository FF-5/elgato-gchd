#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
typedef unsigned char u8; typedef unsigned short u16;
struct gchd;
static int rd[3][256]; /* 0 FE, 1 BE, 2 DEC */
/* Recorded values per register in order; the last one repeats. */
static unsigned char seq[3][256][4096]; static int nseq[3][256], pos[3][256];
static int pend = -1, pend_reg;
unsigned long jiffies;
char __this_module[4096];
char param_ops_uint[64];
static const char *pn(u8 port){ return port==0x4e?"9C":port==0x4c?"98":port==0x44?"88":"BR"; }
int gchd_mail_write(struct gchd *d, u8 port, const u8 *b, u8 len)
{
	if (port == 0x33 && len == 3 && ((b[0]==0x9d&&b[1]==0xcd)||(b[0]==0x99&&b[1]==0x89)||(b[0]==0x89&&b[1]==0x89))) {
		pend = b[0]==0x9d?0:b[0]==0x99?1:2; pend_reg = b[2]; return 0; }
	if (port == 0x33 && len == 3 && b[0] == 0x94) { printf("WRITE  94:%02X = %02X\n", b[1], b[2]); return 0; }
	if (port != 0x33 && len == 2) { printf("WRITE  %s:%02X = %02X\n", pn(port), b[0], b[1]); return 0; }
	printf("MAILW  %02x:", port); for (int i=0;i<len;i++) printf(" %02x", b[i]); printf("\n"); return 0;
}
static long nreads;
int gchd_mail_read(struct gchd *d, u8 port, u8 *b, u8 len)
{
	jiffies += 1;
	if (++nreads > 20000) { printf("# stuck in a read loop\n"); exit(2); }
	if (pend >= 0 && len == 1) {
		int n = nseq[pend][pend_reg];
		/* Detection registers: the value of the settled source. */
		if (pend == 0 && pend_reg >= 0x65 && pend_reg <= 0x68 && n) pos[pend][pend_reg] = n - 1;
		int v = n ? seq[pend][pend_reg][pos[pend][pend_reg] < n ? pos[pend][pend_reg]++ : n - 1] : 0xff;
		b[0] = v;
		printf("READ   %s:%02X -> %02X\n", pend==0?"9C":pend==1?"98":"88", pend_reg, b[0]);
		pend = -1; return 0; }
	memset(b, 0, len); return 0;
}
int gchd_do_enable(struct gchd *d, u16 m, u16 v){ printf("ENABLE mask=%04x values=%04x\n", m, v); return 0; }
int gchd_enable_analog(struct gchd *d){ printf("ENABLE_ANALOG\n"); return 0; }
int gchd_raw_read16(struct gchd *d, u16 val, u16 idx, u16 *v){ jiffies += 1; *v = 1; return 0; }
int gchd_slsi(struct gchd *d, u16 a, u16 v){ printf("SLSI   %04x = %04x\n", a, v); return 0; }
int gchd_sparam(struct gchd *d, u16 a, u8 lsb, u8 bits, u16 v){ printf("SOCF   %04x/%d/%d = %u\n", a, lsb, bits, v); return 0; }
int gchd_scmd(struct gchd *d, u8 c, u8 m, u16 v){ printf("SCMD   %d mode=%02X data=%04X\n", c, m, v); return 0; }
int gchd_state_cmd(struct gchd *d, u8 c, u8 m, u16 v, ...){ printf("STATE  %d mode=%02X data=%04X\n", c, m, v); return 0; }
int gchd_return_to_stop(struct gchd *d){ printf("RETURN_TO_STOP\n"); return 0; }
void gchd_remember_std(struct gchd *d){}
int ___ratelimit(void *a, const char *f){ return 0; }
void __dynamic_dev_dbg(void *a, ...){}
unsigned long __msecs_to_jiffies(unsigned int m){ return m; }
void _dev_err(void *d, const char *f, ...){}
void _dev_info(void *d, const char *f, ...){}
void _dev_warn(void *d, const char *f, ...){}
void msleep(unsigned int m){ jiffies += m; }
void usleep_range_state(unsigned long a, unsigned long b, unsigned int s){ jiffies += 1; }
struct gchd *emu_dev(void);
int emu_component(struct gchd *d, int prog);
int main(int argc, char **argv)
{
	char line[256]; FILE *f;
	memset(rd, 0xff, sizeof(rd));
	f = fopen(argv[1], "r");
	while (f && fgets(line, sizeof line, f)) {
		char dev[8]; unsigned reg, val;
		if (sscanf(line, "%*f READ %2s:%x -> %x", dev, &reg, &val) == 3) {
			int i = !strcmp(dev,"9C")?0:!strcmp(dev,"98")?1:!strcmp(dev,"88")?2:-1;
			if (i >= 0 && nseq[i][reg] < 4096) seq[i][reg][nseq[i][reg]++] = val;
		}
	}
	printf("# result %d\n", emu_component(emu_dev(), argc > 2));
	return 0;
}
