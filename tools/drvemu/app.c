
/* ---- emulator entry (appended) ---- */
struct gchd *emu_dev(void) { static struct gchd g; return &g; }
int emu_component(struct gchd *d, int prog)
{
	int r;
	d->input = 1; d->family = GCHD_FAMILY_HDNEW;
	d->h264_profile = 4; d->out_active = 0;
	r = gchd_configure_component_exact(d);
	if (r) return r;
	return prog ? gchd_encoder_program(d) : 0;
}
