// SPDX-License-Identifier: GPL-2.0
/*
 * OBS source "Elgato Game Capture HD".
 *
 * Reads the MPEG transport stream of the elgato_gchd driver's video device,
 * decodes video and audio with FFmpeg and passes them to OBS. The device is
 * found by its driver in sysfs; the input, output size and encoder settings are
 * driver's settings, shared with every other program: the properties show
 * the device's current values; a change is applied to the device 0.5 s
 * after the last change (also while capturing) and remembered by the
 * driver; changes made elsewhere are taken over every 1.5 s. All device
 * settings calls run in a worker thread, never in OBS's user interface.
 * The capture device runs while the source is shown. Deinterlacing is OBS's
 * own (Yadif 2x, top field first, set once when the source is created).
 */

#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <util/bmem.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/videodev2.h>

OBS_DECLARE_MODULE()

/* Private control of the driver: "Stretch SD Input". */
#define GCHD_CID_STRETCH (V4L2_CID_USER_BASE + 0x10f1)
#define GCHD_IO_SIZE 65424 /* 348 transport-stream packets */

/*
 * Bitrate of composite and S-video, shown in Mbit/s as the device's capture
 * application shows it: VBR as the average (half the bitrate): 3.0-7.23
 * (bitrate 6000-14460 kbit/s) for the standard and 640x480 sizes, 2.0-7.5
 * (4000-15000) for 480x360, which is always VBR; CBR as the bitrate:
 * 7.0-11.25 (the application uses 11250 only).
 */
#define GCHD_STEP_KBPS 20
static const double gchd_vbr_min[3] = { 3.0, 3.0, 2.0 };
static const double gchd_vbr_max[3] = { 7.23, 7.23, 7.5 };
static const double gchd_vbr_def[3] = { 7.0, 7.0, 7.5 };
#define GCHD_CBR_MIN 7.0
#define GCHD_CBR_MAX 11.25
#define GCHD_CBR_DEF 11.25

/* Device settings as the properties show them. */
struct gchd_vals {
	int input;		/* 0 HDMI, 1 component, 2 composite, 3 S-video */
	int output;		/* 0 720x480/576, 1 640x480, 2 480x360 */
	int mode;		/* V4L2_MPEG_VIDEO_BITRATE_MODE_* */
	double rate;		/* Mbit/s as shown (VBR: average) */
	int profile;		/* V4L2_MPEG_VIDEO_H264_PROFILE_* */
	bool stretch;
};

/* The driver's values of a setting group as last read (index: mode). */
struct gchd_mem {
	bool known;
	int mode;
	double rate[2];		/* [V4L2_MPEG_VIDEO_BITRATE_MODE_VBR/_CBR] */
	int profile;
};

#define GCHD_APPLY_DELAY_NS 500000000ULL /* after the last change */
#define GCHD_SYNC_NS 1500000000ULL	 /* device read interval */

struct gchd_src {
	obs_source_t *source;

	/* settings */
	char *device;		/* /dev/videoN, empty: first found */
	struct gchd_vals cur;	/* the source's settings */
	struct gchd_vals base;	/* the device's values at the last sync or apply */
	struct gchd_vals dlg;	/* mode, rate, profile the properties set
				   themselves (not the user's changes) */
	bool have_base;
	struct gchd_mem mem[2];	/* [0] HDMI/component, [1] composite/S-video */
	pthread_mutex_t lock;	/* device, cur, base, dlg, mem, apply_pending */
	pthread_t worker;	/* applies and synchronises the settings */
	bool worker_running;
	volatile bool worker_stop;
	bool apply_pending;
	uint64_t apply_at;	/* os_gettime_ns() of the delayed apply */

	/* capture */
	pthread_t thread;
	bool thread_running;
	volatile bool stop;
	bool shown;
	bool warned_format;
	bool created;
	int fd;
};

/* ------------------------------------------------------------ device */

/* First video device of the elgato_gchd driver, as /dev/videoN; false if none. */
static bool gchd_find(char *path, size_t size)
{
	DIR *dir = opendir("/sys/class/video4linux");
	struct dirent *e;
	bool found = false;
	int best = -1;

	if (!dir)
		return false;
	while ((e = readdir(dir))) {
		char p[512], target[512];
		const char *drv;
		ssize_t len;
		int n;

		if (strncmp(e->d_name, "video", 5))
			continue;
		snprintf(p, sizeof(p), "/sys/class/video4linux/%s/device/driver",
			 e->d_name);
		len = readlink(p, target, sizeof(target) - 1);
		if (len <= 0)
			continue;
		target[len] = 0;
		drv = strrchr(target, '/');
		drv = drv ? drv + 1 : target;
		if (strcmp(drv, "elgato_gchd"))
			continue;
		n = atoi(e->d_name + 5);
		if (best < 0 || n < best) {
			best = n;
			snprintf(path, size, "/dev/%s", e->d_name);
			found = true;
		}
	}
	closedir(dir);
	return found;
}

static void gchd_ctrl(int fd, unsigned int id, int value)
{
	struct v4l2_control c = { .id = id, .value = value };

	if (ioctl(fd, VIDIOC_S_CTRL, &c) < 0)
		blog(LOG_WARNING, "[gchd] control 0x%x = %d: %s", id, value,
		     strerror(errno));
}

/* kbit/s of a shown bitrate: hundredths of Mbit/s, rounded half up. */
static int gchd_kbps(double shown, bool vbr)
{
	return (int)(shown * 100.0 + 0.5) * (vbr ? 20 : 10);
}

static int gchd_group(int input)
{
	return input >= 2 ? 1 : 0;
}

/* Bitrate mode in effect: 480x360 is VBR only. */
static int gchd_eff_mode(int output, int mode)
{
	return output == 2 ? V4L2_MPEG_VIDEO_BITRATE_MODE_VBR : mode;
}

static bool gchd_rate_equal(double a, double b)
{
	return (int)(a * 100.0 + 0.5) == (int)(b * 100.0 + 0.5);
}

/* Same settings. */
static bool gchd_vals_equal(const struct gchd_vals *a, const struct gchd_vals *b)
{
	return a->input == b->input && a->output == b->output &&
	       a->mode == b->mode && a->profile == b->profile &&
	       a->stretch == b->stretch &&
	       (int)(a->rate * 100.0 + 0.5) == (int)(b->rate * 100.0 + 0.5);
}

/*
 * Apply the settings V that differ from the device's values B (all with
 * ALL), so that a stale value of the source never overwrites a setting
 * changed elsewhere. Mode, bitrate and profile that the properties filled
 * in themselves (SH: the driver's kept values after a change of bitrate
 * mode or setting group) are not sent: the driver loads them itself. A new
 * bitrate mode is sent alone unless the bitrate was moved too. 480x360 is
 * VBR only.
 */
static void gchd_apply_changes(int fd, const struct gchd_vals *v,
			       const struct gchd_vals *b,
			       const struct gchd_vals *sh, bool all)
{
	int o = v->output >= 0 && v->output <= 2 ? v->output : 0;
	bool analog = v->input >= 2;
	bool vbr = o == 2 || v->mode == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
	bool group = gchd_group(v->input) != gchd_group(b->input);
	bool mobile = (o == 2) != (b->output == 2);
	/* After a group change the device loads that group's kept values. */
	const struct gchd_vals *g = group ? sh : b;
	bool mode_changed = all || mobile || v->mode != g->mode;
	/* After a mode change the device loads that mode's kept bitrate. */
	bool rate_changed = all || !gchd_rate_equal(v->rate,
						    mode_changed || group ?
							    sh->rate : b->rate);
	bool profile_changed = all || v->profile != g->profile;

	if (all || v->input != b->input) {
		unsigned int input = (unsigned int)v->input, now = 99;

		/* Only a different input: selecting the same one re-detects it. */
		if (ioctl(fd, VIDIOC_G_INPUT, &now) < 0 || now != input) {
			if (ioctl(fd, VIDIOC_S_INPUT, &input) < 0)
				blog(LOG_WARNING, "[gchd] input %u: %s", input,
				     strerror(errno));
		}
	}
	if (analog && (all || v->output != b->output)) {
		static const unsigned int w[] = { 720, 640, 480 };
		static const unsigned int h[] = { 480, 480, 360 };
		struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };

		if (ioctl(fd, VIDIOC_G_FMT, &fmt) == 0) {
			fmt.fmt.pix.width = w[o];
			fmt.fmt.pix.height = h[o];
			if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
				blog(LOG_WARNING, "[gchd] output size: %s",
				     strerror(errno));
		}
	}
	/* The driver then uses the bitrate last set in that mode. */
	if (analog && mode_changed)
		gchd_ctrl(fd, V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
			  vbr ? V4L2_MPEG_VIDEO_BITRATE_MODE_VBR :
				V4L2_MPEG_VIDEO_BITRATE_MODE_CBR);
	if (analog && rate_changed) {
		double lo = vbr ? gchd_vbr_min[o] : GCHD_CBR_MIN;
		double hi = vbr ? gchd_vbr_max[o] : GCHD_CBR_MAX;
		double r = v->rate < lo ? lo : v->rate > hi ? hi : v->rate;

		gchd_ctrl(fd, V4L2_CID_MPEG_VIDEO_BITRATE, gchd_kbps(r, vbr) * 1000);
	}
	if (profile_changed)
		gchd_ctrl(fd, V4L2_CID_MPEG_VIDEO_H264_PROFILE, v->profile);
	if (analog && (all || v->stretch != b->stretch))
		gchd_ctrl(fd, GCHD_CID_STRETCH, v->stretch);
}

/* The transport stream format, keeping the device's output size. */
static void gchd_set_mpeg(int fd)
{
	struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };

	if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0)
		return;
	if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MPEG)
		return;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MPEG;
	if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
		blog(LOG_WARNING, "[gchd] format: %s", strerror(errno));
}

/*
 * Read the device's settings (composite/S-video bitrate as shown: VBR the
 * average). False when the device cannot be opened.
 */
static bool gchd_read_device(const char *path, struct gchd_vals *v)
{
	struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
	struct v4l2_control c;
	unsigned int input = 0;
	int fd, kb = 11250;

	fd = open(path, O_RDWR | O_NONBLOCK);
	if (fd < 0)
		return false;
	v->mode = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
	v->output = 0;
	if (ioctl(fd, VIDIOC_G_INPUT, &input) == 0)
		v->input = (int)input;
	if (ioctl(fd, VIDIOC_G_FMT, &fmt) == 0) {
		if (fmt.fmt.pix.width == 640 && fmt.fmt.pix.height == 480)
			v->output = 1;
		else if (fmt.fmt.pix.width == 480 && fmt.fmt.pix.height == 360)
			v->output = 2;
	}
	c.id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE;
	if (ioctl(fd, VIDIOC_G_CTRL, &c) == 0)
		v->mode = c.value;
	c.id = V4L2_CID_MPEG_VIDEO_BITRATE;
	if (ioctl(fd, VIDIOC_G_CTRL, &c) == 0)
		kb = c.value / 1000;
	if (v->output == 2)
		v->mode = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
	v->rate = v->mode == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR ? kb / 2000.0 :
								kb / 1000.0;
	c.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
	if (ioctl(fd, VIDIOC_G_CTRL, &c) == 0)
		v->profile = c.value;
	c.id = GCHD_CID_STRETCH;
	if (ioctl(fd, VIDIOC_G_CTRL, &c) == 0)
		v->stretch = c.value != 0;
	close(fd);
	return true;
}

/*
 * Device values into the settings, all of them or (with OLD) those that
 * differ from OLD. The "shown_" keys record what the properties show
 * without a change by the user: mode, bitrate and profile are applied only
 * when they differ from them.
 */
static void gchd_vals_to_settings(const struct gchd_vals *v,
				  const struct gchd_vals *old, obs_data_t *st)
{
	if (!old || v->input != old->input)
		obs_data_set_int(st, "input", v->input);
	if (!old || v->output != old->output)
		obs_data_set_int(st, "output", v->output);
	if (!old || v->mode != old->mode) {
		obs_data_set_int(st, "bitrate_mode", v->mode);
		obs_data_set_int(st, "shown_mode", v->mode);
	}
	if (!old || !gchd_rate_equal(v->rate, old->rate)) {
		obs_data_set_double(st, "rate", v->rate);
		obs_data_set_double(st, "shown_rate", v->rate);
	}
	if (!old || v->profile != old->profile) {
		obs_data_set_int(st, "profile", v->profile);
		obs_data_set_int(st, "shown_profile", v->profile);
	}
	if (!old || v->stretch != old->stretch)
		obs_data_set_bool(st, "stretch", v->stretch);
	if (!old || gchd_group(v->input) != gchd_group(old->input))
		obs_data_set_int(st, "shown_group", gchd_group(v->input));
	if (!old || gchd_eff_mode(v->output, v->mode) !=
			    gchd_eff_mode(old->output, old->mode))
		obs_data_set_int(st, "shown_eff",
				 gchd_eff_mode(v->output, v->mode));
}

/* Remember the group's values read from the device (lock held). */
static void gchd_mem_note(struct gchd_src *s, const struct gchd_vals *v)
{
	struct gchd_mem *m = &s->mem[gchd_group(v->input)];
	int eff = gchd_eff_mode(v->output, v->mode);

	m->known = true;
	m->mode = v->mode;
	m->rate[eff ? 1 : 0] = v->rate;
	m->profile = v->profile;
}

/* ------------------------------------------------------------ decoding */

/* Read from the device without blocking the stop request. */
static int gchd_read(void *opaque, uint8_t *buf, int size)
{
	struct gchd_src *s = opaque;

	while (!s->stop) {
		struct pollfd p = { .fd = s->fd, .events = POLLIN };
		ssize_t n;

		if (poll(&p, 1, 100) <= 0)
			continue;
		n = read(s->fd, buf, (size_t)size);
		if (n > 0)
			return (int)n;
		if (n == 0)
			return AVERROR_EOF;
		if (errno != EAGAIN && errno != EINTR)
			return AVERROR(errno);
	}
	return AVERROR_EXIT;
}

static enum audio_format gchd_audio_format(enum AVSampleFormat f)
{
	switch (f) {
	case AV_SAMPLE_FMT_S16:
		return AUDIO_FORMAT_16BIT;
	case AV_SAMPLE_FMT_S16P:
		return AUDIO_FORMAT_16BIT_PLANAR;
	case AV_SAMPLE_FMT_FLT:
		return AUDIO_FORMAT_FLOAT;
	case AV_SAMPLE_FMT_FLTP:
		return AUDIO_FORMAT_FLOAT_PLANAR;
	case AV_SAMPLE_FMT_S32:
		return AUDIO_FORMAT_32BIT;
	case AV_SAMPLE_FMT_S32P:
		return AUDIO_FORMAT_32BIT_PLANAR;
	default:
		return AUDIO_FORMAT_UNKNOWN;
	}
}

struct gchd_clock {
	bool set;
	int64_t base_us;	/* stream time of the first frame */
	uint64_t base_ns;	/* system time of the first frame */
};

/* OBS timestamp of a stream time (both streams share the base). */
static uint64_t gchd_ts(struct gchd_clock *c, int64_t pts, AVRational tb)
{
	int64_t us;

	if (pts == AV_NOPTS_VALUE)
		return os_gettime_ns();
	us = av_rescale_q(pts, tb, AV_TIME_BASE_Q);
	if (!c->set) {
		c->set = true;
		c->base_us = us;
		c->base_ns = os_gettime_ns();
	}
	return c->base_ns + (uint64_t)((us - c->base_us) * 1000);
}

static void gchd_out_video(struct gchd_src *s, AVFrame *f, uint64_t ts)
{
	struct obs_source_frame2 frame = { 0 };
	enum video_colorspace cs = f->height > 576 ? VIDEO_CS_709 :
						     VIDEO_CS_601;

	if (f->format != AV_PIX_FMT_YUV420P && f->format != AV_PIX_FMT_YUVJ420P) {
		if (!s->warned_format)
			blog(LOG_WARNING, "[gchd] picture format %d not supported",
			     f->format);
		s->warned_format = true;
		return;
	}
	for (int i = 0; i < 3; i++) {
		frame.data[i] = f->data[i];
		frame.linesize[i] = (uint32_t)f->linesize[i];
	}
	frame.width = (uint32_t)f->width;
	frame.height = (uint32_t)f->height;
	frame.format = VIDEO_FORMAT_I420;
	frame.range = f->format == AV_PIX_FMT_YUVJ420P ? VIDEO_RANGE_FULL :
							   VIDEO_RANGE_PARTIAL;
	frame.timestamp = ts;
	video_format_get_parameters(cs, frame.range, frame.color_matrix,
				    frame.color_range_min,
				    frame.color_range_max);
	obs_source_output_video2(s->source, &frame);
}

static void gchd_out_audio(struct gchd_src *s, AVFrame *f, uint64_t ts)
{
	struct obs_source_audio a = { 0 };
	int ch = f->ch_layout.nb_channels;

	a.format = gchd_audio_format(f->format);
	if (a.format == AUDIO_FORMAT_UNKNOWN || ch < 1 || ch > 2)
		return;
	for (int i = 0; i < MAX_AV_PLANES && f->data[i]; i++)
		a.data[i] = f->data[i];
	a.frames = (uint32_t)f->nb_samples;
	a.speakers = ch == 1 ? SPEAKERS_MONO : SPEAKERS_STEREO;
	a.samples_per_sec = (uint32_t)f->sample_rate;
	a.timestamp = ts;
	obs_source_output_audio(s->source, &a);
}

static AVCodecContext *gchd_open_codec(AVStream *st)
{
	const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
	AVCodecContext *c;

	if (!codec)
		return NULL;
	c = avcodec_alloc_context3(codec);
	if (!c)
		return NULL;
	if (avcodec_parameters_to_context(c, st->codecpar) < 0 ||
	    avcodec_open2(c, codec, NULL) < 0) {
		avcodec_free_context(&c);
		return NULL;
	}
	return c;
}

/* One capture session: open, decode until stop or error. */
static void gchd_session(struct gchd_src *s, const char *path)
{
	AVFormatContext *fc = NULL;
	AVIOContext *io = NULL;
	AVCodecContext *vc = NULL, *ac = NULL;
	AVPacket *pkt = av_packet_alloc();
	AVFrame *frm = av_frame_alloc();
	AVDictionary *opt = NULL;
	struct gchd_clock clock = { 0 };
	unsigned char *buf;
	int vi = -1, ai = -1;

	s->fd = open(path, O_RDWR | O_NONBLOCK);
	if (s->fd < 0) {
		blog(LOG_WARNING, "[gchd] %s: %s", path, strerror(errno));
		goto out;
	}
	gchd_set_mpeg(s->fd);

	buf = av_malloc(GCHD_IO_SIZE);
	io = buf ? avio_alloc_context(buf, GCHD_IO_SIZE, 0, s, gchd_read, NULL,
				      NULL)
		 : NULL;
	fc = avformat_alloc_context();
	if (!io || !fc)
		goto out;
	fc->pb = io;
	fc->flags |= AVFMT_FLAG_CUSTOM_IO;
	av_dict_set(&opt, "probesize", "1000000", 0);
	av_dict_set(&opt, "analyzeduration", "1000000", 0);
	if (avformat_open_input(&fc, NULL, av_find_input_format("mpegts"),
				&opt) < 0) {
		blog(LOG_WARNING, "[gchd] %s: no transport stream", path);
		fc = NULL;
		goto out;
	}
	if (avformat_find_stream_info(fc, NULL) < 0) {
		blog(LOG_WARNING, "[gchd] %s: stream not recognized", path);
		goto out;
	}
	vi = av_find_best_stream(fc, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	ai = av_find_best_stream(fc, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (vi >= 0)
		vc = gchd_open_codec(fc->streams[vi]);
	if (ai >= 0)
		ac = gchd_open_codec(fc->streams[ai]);
	blog(LOG_INFO, "[gchd] capturing from %s (video %s, audio %s)", path,
	     vc ? "yes" : "no", ac ? "yes" : "no");

	while (!s->stop && av_read_frame(fc, pkt) >= 0) {
		AVCodecContext *c = pkt->stream_index == vi ? vc :
				    pkt->stream_index == ai ? ac : NULL;
		AVRational tb = fc->streams[pkt->stream_index]->time_base;

		if (c && avcodec_send_packet(c, pkt) >= 0) {
			while (avcodec_receive_frame(c, frm) >= 0) {
				uint64_t ts = gchd_ts(&clock,
						      frm->best_effort_timestamp,
						      tb);

				if (c == vc)
					gchd_out_video(s, frm, ts);
				else
					gchd_out_audio(s, frm, ts);
				av_frame_unref(frm);
			}
		}
		av_packet_unref(pkt);
	}

out:
	av_dict_free(&opt);
	avcodec_free_context(&vc);
	avcodec_free_context(&ac);
	if (fc)
		avformat_close_input(&fc);
	if (io) {
		av_freep(&io->buffer);
		avio_context_free(&io);
	}
	av_frame_free(&frm);
	av_packet_free(&pkt);
	if (s->fd >= 0)
		close(s->fd);
	s->fd = -1;
}

static bool gchd_path(struct gchd_src *s, char *path, size_t size);

static void *gchd_thread(void *data)
{
	struct gchd_src *s = data;
	char path[300];

	bool told = false;

	os_set_thread_name("gchd-capture");
	blog(LOG_INFO, "[gchd] capture started (source shown)");
	while (!s->stop) {
		if (!gchd_path(s, path, sizeof(path)))
			path[0] = 0;
		if (path[0]) {
			told = false;
			gchd_session(s, path);
			if (!s->stop)
				blog(LOG_INFO, "[gchd] capture from %s ended; retrying",
				     path);
		} else if (!told) {
			blog(LOG_INFO, "[gchd] no device of the elgato_gchd driver; waiting");
			told = true;
		}
		if (s->stop)
			break;
		/* No picture until the device is back. */
		obs_source_output_video2(s->source, NULL);
		for (int i = 0; i < 10 && !s->stop; i++)
			os_sleep_ms(100);
	}
	return NULL;
}

static void gchd_start(struct gchd_src *s)
{
	if (s->thread_running)
		return;
	s->stop = false;
	if (pthread_create(&s->thread, NULL, gchd_thread, s) == 0)
		s->thread_running = true;
}

static void gchd_stop(struct gchd_src *s)
{
	if (!s->thread_running)
		return;
	s->stop = true;
	pthread_join(s->thread, NULL);
	s->thread_running = false;
	obs_source_output_video2(s->source, NULL);
	blog(LOG_INFO, "[gchd] capture stopped (source hidden or settings changed)");
}

/* ------------------------------------------------------------ OBS source */

static const char *gchd_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Elgato Game Capture HD";
}

/* Device path: the chosen one, else the first device of the driver. */
static bool gchd_path(struct gchd_src *s, char *path, size_t size)
{
	bool chosen;

	pthread_mutex_lock(&s->lock);
	chosen = s->device && *s->device;
	if (chosen)
		snprintf(path, size, "%s", s->device);
	pthread_mutex_unlock(&s->lock);
	return chosen || gchd_find(path, size);
}

/*
 * Device values that differ from OLD (changed elsewhere, or kept by the
 * driver) into the source's settings, without update(); an open
 * properties dialog shows them (reload queued to its thread).
 */
static void gchd_push_settings(struct gchd_src *s, const struct gchd_vals *old,
			       const struct gchd_vals *v)
{
	obs_data_t *st = obs_source_get_settings(s->source);

	gchd_vals_to_settings(v, old, st);
	obs_data_release(st);
	obs_source_update_properties(s->source);
}

/*
 * Worker: applies the settings 0.5 s after the last change and takes over
 * changes made elsewhere every 1.5 s. Device calls only here.
 */
static void *gchd_worker(void *data)
{
	struct gchd_src *s = data;
	uint64_t next_sync = 0;

	os_set_thread_name("gchd-settings");
	while (!s->worker_stop) {
		uint64_t now = os_gettime_ns();
		struct gchd_vals cur = { 0 }, base = { 0 }, shown = { 0 }, v;
		bool apply = false, all = false, sync = false;
		char path[300];

		pthread_mutex_lock(&s->lock);
		if (s->apply_pending && now >= s->apply_at) {
			apply = true;
			s->apply_pending = false;
			cur = s->cur;
			base = s->base;
			shown = s->dlg;
			all = !s->have_base;
		} else if (!s->apply_pending && now >= next_sync) {
			sync = true;
		}
		pthread_mutex_unlock(&s->lock);

		if ((apply || sync) && gchd_path(s, path, sizeof(path))) {
			if (apply) {
				int fd = open(path, O_RDWR | O_NONBLOCK);

				if (fd >= 0) {
					gchd_apply_changes(fd, &cur, &base, &shown,
							   all);
					close(fd);
					blog(LOG_INFO,
					     "[gchd] settings applied: input %d, output %d, mode %d, %.2f Mbit/s, profile %d, stretch %d",
					     cur.input, cur.output, cur.mode, cur.rate,
					     cur.profile, cur.stretch);
				}
			}
			v = apply ? cur : base;
			if (gchd_read_device(path, &v)) {
				struct gchd_vals old;
				bool push = false, had = false;

				pthread_mutex_lock(&s->lock);
				gchd_mem_note(s, &v);
				/* A newer change waits: leave it alone. */
				if (!s->apply_pending) {
					/* Only values the source does not have yet. */
					had = s->have_base;
					old = s->cur;
					push = !had || !gchd_vals_equal(&v, &old);
					s->base = v;
					s->cur = v;
					s->dlg = v;
					s->have_base = true;
				}
				pthread_mutex_unlock(&s->lock);
				if (push)
					gchd_push_settings(s, had ? &old : NULL, &v);
			}
			next_sync = os_gettime_ns() + GCHD_SYNC_NS;
		}
		os_sleep_ms(100);
	}
	return NULL;
}

/*
 * Settings from the properties (OBS calls this for every change while the
 * dialog is open, for every step of a moved slider, and with the old
 * settings on Cancel): the worker applies the changes 0.5 s after the last
 * one, so the picture follows the dialog. Nothing is applied when the
 * source is created (OBS start): the device keeps its own (shared)
 * settings.
 */
static void gchd_update(void *data, obs_data_t *settings)
{
	struct gchd_src *s = data;

	pthread_mutex_lock(&s->lock);
	bfree(s->device);
	s->device = bstrdup(obs_data_get_string(settings, "device"));
	s->cur.input = (int)obs_data_get_int(settings, "input");
	s->cur.output = (int)obs_data_get_int(settings, "output");
	s->cur.rate = obs_data_get_double(settings, "rate");
	s->cur.mode = (int)obs_data_get_int(settings, "bitrate_mode");
	s->cur.profile = (int)obs_data_get_int(settings, "profile");
	s->cur.stretch = obs_data_get_bool(settings, "stretch");
	if (obs_data_has_user_value(settings, "shown_mode")) {
		s->dlg.mode = (int)obs_data_get_int(settings, "shown_mode");
		s->dlg.rate = obs_data_get_double(settings, "shown_rate");
		s->dlg.profile = (int)obs_data_get_int(settings, "shown_profile");
	}
	if (s->created && s->have_base && !gchd_vals_equal(&s->cur, &s->base)) {
		s->apply_pending = true;
		s->apply_at = os_gettime_ns() + GCHD_APPLY_DELAY_NS;
	}
	pthread_mutex_unlock(&s->lock);
}

static void *gchd_create(obs_data_t *settings, obs_source_t *source)
{
	struct gchd_src *s = bzalloc(sizeof(*s));

	s->source = source;
	s->fd = -1;
	pthread_mutex_init(&s->lock, NULL);
	/* OBS deinterlacing, once (the user can change it afterwards). */
	if (!obs_data_get_bool(settings, "deinterlace_set")) {
		obs_source_set_deinterlace_mode(source,
						OBS_DEINTERLACE_MODE_YADIF_2X);
		obs_source_set_deinterlace_field_order(
			source, OBS_DEINTERLACE_FIELD_ORDER_TOP);
		obs_data_set_bool(settings, "deinterlace_set", true);
	}
	gchd_update(s, settings);
	s->created = true;
	/* The worker reads the device's (shared) settings at once. */
	s->worker_stop = false;
	if (pthread_create(&s->worker, NULL, gchd_worker, s) == 0)
		s->worker_running = true;
	return s;
}

static void gchd_destroy(void *data)
{
	struct gchd_src *s = data;

	gchd_stop(s);
	if (s->worker_running) {
		s->worker_stop = true;
		pthread_join(s->worker, NULL);
	}
	pthread_mutex_destroy(&s->lock);
	bfree(s->device);
	bfree(s);
}

/* Capture while the source is shown (program or preview). */
static void gchd_show(void *data)
{
	struct gchd_src *s = data;

	s->shown = true;
	gchd_start(s);
}

static void gchd_hide(void *data)
{
	struct gchd_src *s = data;

	s->shown = false;
	gchd_stop(s);
}

static void gchd_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "device", "");
	obs_data_set_default_int(settings, "input", 2);
	obs_data_set_default_int(settings, "output", 0);
	obs_data_set_default_double(settings, "rate", GCHD_CBR_DEF);
	obs_data_set_default_int(settings, "bitrate_mode",
				 V4L2_MPEG_VIDEO_BITRATE_MODE_CBR);
	obs_data_set_default_int(settings, "profile",
				 V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);
	obs_data_set_default_bool(settings, "stretch", false);
	obs_data_set_default_int(settings, "shown_group", -1);
	obs_data_set_default_int(settings, "shown_eff", -1);
}

/*
 * Visible settings and slider limits: output size, stretch, bitrate mode and
 * bitrate for composite/S-video; 480x360 VBR only (mode hidden); the slider
 * range follows the mode and the output size; a value outside it is set to
 * the default of the mode.
 */
static bool gchd_settings_changed(obs_properties_t *props, obs_property_t *p,
				  obs_data_t *settings)
{
	struct gchd_src *s = obs_properties_get_param(props);
	int input = (int)obs_data_get_int(settings, "input");
	int g = gchd_group(input);
	bool analog = input >= 2;
	int o = (int)obs_data_get_int(settings, "output");
	bool mobile, vbr;
	double lo, hi, def, r;
	int eff;

	UNUSED_PARAMETER(p);
	if (o < 0 || o > 2)
		o = 0;
	mobile = o == 2;

	/* Other setting group: show the values the driver keeps for it. */
	if (g != obs_data_get_int(settings, "shown_group")) {
		if (s) {
			pthread_mutex_lock(&s->lock);
			if (s->mem[g].known) {
				obs_data_set_int(settings, "bitrate_mode",
						 s->mem[g].mode);
				obs_data_set_int(settings, "shown_mode",
						 s->mem[g].mode);
				obs_data_set_int(settings, "profile",
						 s->mem[g].profile);
				obs_data_set_int(settings, "shown_profile",
						 s->mem[g].profile);
			}
			pthread_mutex_unlock(&s->lock);
		}
		obs_data_set_int(settings, "shown_group", g);
		obs_data_set_int(settings, "shown_eff", -1);
	}

	eff = gchd_eff_mode(o, (int)obs_data_get_int(settings, "bitrate_mode"));
	vbr = eff == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
	lo = vbr ? gchd_vbr_min[o] : GCHD_CBR_MIN;
	hi = vbr ? gchd_vbr_max[o] : GCHD_CBR_MAX;
	def = vbr ? gchd_vbr_def[o] : GCHD_CBR_DEF;
	r = obs_data_get_double(settings, "rate");

	/* Other bitrate mode: show the bitrate last used in it. */
	if (eff != obs_data_get_int(settings, "shown_eff")) {
		double known = 0.0;

		if (s) {
			pthread_mutex_lock(&s->lock);
			if (s->mem[g].known)
				known = s->mem[g].rate[vbr ? 0 : 1];
			pthread_mutex_unlock(&s->lock);
		}
		if (known > 0.0)
			r = known;
		if (r < lo - 1e-6 || r > hi + 1e-6)
			r = def;
		obs_data_set_double(settings, "rate", r);
		obs_data_set_double(settings, "shown_rate", r);
		obs_data_set_int(settings, "shown_eff", eff);
	} else if (r < lo - 1e-6 || r > hi + 1e-6) {
		obs_data_set_double(settings, "rate", def);
	}

	obs_property_set_visible(obs_properties_get(props, "output"), analog);
	obs_property_set_visible(obs_properties_get(props, "stretch"), analog);
	obs_property_set_visible(obs_properties_get(props, "bitrate_mode"),
				 analog && !mobile);
	obs_property_set_visible(obs_properties_get(props, "rate"), analog);
	obs_property_float_set_limits(obs_properties_get(props, "rate"), lo, hi,
				      0.01);
	return true;
}

static obs_properties_t *gchd_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;
	char path[300];

	obs_properties_set_param(props, data, NULL);

	/* Show the device's current (shared) settings. */
	if (data) {
		struct gchd_src *s = data;
		obs_data_t *settings = obs_source_get_settings(s->source);
		struct gchd_vals v;
		bool pending;

		pthread_mutex_lock(&s->lock);
		v = s->cur;
		pthread_mutex_unlock(&s->lock);
		if (gchd_path(s, path, sizeof(path)) &&
		    gchd_read_device(path, &v)) {
			/* A change of the user still waits: keep it. */
			pthread_mutex_lock(&s->lock);
			pending = s->apply_pending;
			pthread_mutex_unlock(&s->lock);
			if (pending)
				goto read_done;
			gchd_vals_to_settings(&v, NULL, settings);
			pthread_mutex_lock(&s->lock);
			gchd_mem_note(s, &v);
			s->cur = v;
			s->base = v;
			s->dlg = v;
			s->have_base = true;
			s->apply_pending = false;
			pthread_mutex_unlock(&s->lock);
			blog(LOG_INFO,
			     "[gchd] properties: device settings read: input %d, output %d, mode %d, %.2f Mbit/s",
			     v.input, v.output, v.mode, v.rate);
		}
read_done:
		obs_data_release(settings);
	}
	p = obs_properties_add_list(props, "device", "Device",
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, gchd_find(path, sizeof(path)) ?
					   "Automatic (connected device)" :
					   "Automatic (no device connected)",
				     "");

	p = obs_properties_add_list(props, "input", "Input", OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, "HDMI", 0);
	obs_property_list_add_int(p, "Component", 1);
	obs_property_list_add_int(p, "Composite", 2);
	obs_property_list_add_int(p, "S-Video", 3);
	obs_property_set_modified_callback(p, gchd_settings_changed);

	p = obs_properties_add_list(props, "output", "Output size",
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, "720x480 NTSC / 720x576 PAL", 0);
	obs_property_list_add_int(p, "640x480", 1);
	obs_property_list_add_int(p, "480x360", 2);
	obs_property_set_modified_callback(p, gchd_settings_changed);

	p = obs_properties_add_list(props, "bitrate_mode", "Bitrate mode",
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, "Constant (CBR)",
				  V4L2_MPEG_VIDEO_BITRATE_MODE_CBR);
	obs_property_list_add_int(p, "Variable (VBR)",
				  V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);
	obs_property_set_modified_callback(p, gchd_settings_changed);

	p = obs_properties_add_float_slider(props, "rate", "Bitrate", 7.0,
					    11.25, 0.01);
	obs_property_float_set_suffix(p, " Mbit/s");
	obs_property_set_long_description(
		p, "VBR: the average bitrate (half of the setting); CBR: the bitrate");

	p = obs_properties_add_list(props, "profile", "H.264 profile",
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, "High", V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);
	obs_property_list_add_int(p, "Main", V4L2_MPEG_VIDEO_H264_PROFILE_MAIN);
	obs_property_list_add_int(p, "Baseline",
				  V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE);

	obs_properties_add_bool(props, "stretch", "Stretch SD input");
	return props;
}

static struct obs_source_info gchd_source_info = {
	.id = "elgato_gchd_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO |
			OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name = gchd_get_name,
	.create = gchd_create,
	.destroy = gchd_destroy,
	.update = gchd_update,
	.get_defaults = gchd_defaults,
	.get_properties = gchd_properties,
	.show = gchd_show,
	.hide = gchd_hide,
	.icon_type = OBS_ICON_TYPE_CAMERA,
};

bool obs_module_load(void)
{
	obs_register_source(&gchd_source_info);
	return true;
}

const char *obs_module_description(void)
{
	return "Elgato Game Capture HD (elgato_gchd driver)";
}
