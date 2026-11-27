
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
int emu_hdmi(struct gchd *d, int prog, int range, int rx, int allow)
{
	int r;
	d->input = 0; d->family = GCHD_FAMILY_HDNEW;
	d->h264_profile = 4; d->out_active = 0; d->hdmi_range = range;
	d->allow_5060 = allow; d->hdmi_rx_ready = rx;
	d->bitrate_mode = 1; /* CBR, bitrate automatic */
	r = gchd_configure_hdmi_exact(d);
	if (r) return r;
	return prog ? gchd_encoder_program(d) : 0;
}
