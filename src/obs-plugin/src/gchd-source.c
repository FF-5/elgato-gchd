// SPDX-License-Identifier: GPL-2.0
/*
 * OBS source "Elgato Game Capture HD".
 *
 * Reads the MPEG transport stream of the elgato_gchd driver's video device,
 * decodes video and audio with FFmpeg and passes them to OBS. The device is
 * found by its driver in sysfs; the input, output size and encoder settings are
 * properties of the source (OBS's own settings) and are applied to the
 * device with V4L2 ioctls when the capture starts; when it ends, the device
 * settings found before are set again, except those another program changed
 * meanwhile. The capture device runs while the source is shown. Deinterlacing is OBS's
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

struct gchd_src {
	obs_source_t *source;

	/* settings */
	char *device;		/* /dev/videoN, empty: first found */
	int input;		/* 0 HDMI, 1 component, 2 composite, 3 S-video */
	int output;		/* 0 source size, 1 640x480, 2 480x360 */
	double rate;		/* Mbit/s as shown (VBR: average) */
	int bitrate_mode;	/* V4L2_MPEG_VIDEO_BITRATE_MODE_* */
	int profile;		/* V4L2_MPEG_VIDEO_H264_PROFILE_* */
	bool stretch;

	/* capture */
	pthread_t thread;
	bool thread_running;
	volatile bool stop;
	bool shown;
	bool warned_format;
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

/*
 * Apply the settings to the device (input, output size, transport stream,
 * encoder controls). Only while the device is not capturing.
 */
static void gchd_apply(struct gchd_src *s, int fd)
{
	struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
	unsigned int input = (unsigned int)s->input;

	if (ioctl(fd, VIDIOC_S_INPUT, &input) < 0)
		blog(LOG_WARNING, "[gchd] input %u: %s", input,
		     strerror(errno));
	if (ioctl(fd, VIDIOC_G_FMT, &fmt) == 0) {
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MPEG;
		if (s->input >= 2) {
			/* Composite/S-video output size (encoder). */
			static const unsigned int w[] = { 720, 640, 480 };
			static const unsigned int h[] = { 480, 480, 360 };
			int o = s->output >= 0 && s->output <= 2 ? s->output : 0;

			fmt.fmt.pix.width = w[o];
			fmt.fmt.pix.height = h[o];
		}
		if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
			blog(LOG_WARNING, "[gchd] format: %s", strerror(errno));
	}
	if (s->input >= 2) {
		int o = s->output >= 0 && s->output <= 2 ? s->output : 0;
		bool vbr = o == 2 ||
			   s->bitrate_mode == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
		double lo = vbr ? gchd_vbr_min[o] : GCHD_CBR_MIN;
		double hi = vbr ? gchd_vbr_max[o] : GCHD_CBR_MAX;
		double r = s->rate < lo ? lo : s->rate > hi ? hi : s->rate;
		/* kbit/s: VBR shows the average (half). */
		int kb = (int)(r * (vbr ? 2000.0 : 1000.0) / GCHD_STEP_KBPS + 0.5) *
			 GCHD_STEP_KBPS;

		gchd_ctrl(fd, V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
			  vbr ? V4L2_MPEG_VIDEO_BITRATE_MODE_VBR :
				V4L2_MPEG_VIDEO_BITRATE_MODE_CBR);
		gchd_ctrl(fd, V4L2_CID_MPEG_VIDEO_BITRATE, kb * 1000);
	}
	gchd_ctrl(fd, V4L2_CID_MPEG_VIDEO_H264_PROFILE, s->profile);
	if (s->input >= 2)
		gchd_ctrl(fd, GCHD_CID_STRETCH, s->stretch);
}

/* ------------------------------------------------------------ device settings */

/* Device settings the source changes, to set them back afterwards. */
struct gchd_dev_settings {
	bool ok;
	unsigned int input;
	struct v4l2_pix_format pix;
	int ctrl[5];
};

static const unsigned int gchd_ctrl_ids[5] = {
	V4L2_CID_MPEG_VIDEO_BITRATE_MODE, V4L2_CID_MPEG_VIDEO_BITRATE,
	V4L2_CID_MPEG_VIDEO_H264_PROFILE, GCHD_CID_STRETCH,
	V4L2_CID_MPEG_VIDEO_H264_LEVEL,
};

static void gchd_settings_read(int fd, struct gchd_dev_settings *v)
{
	struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };

	memset(v, 0, sizeof(*v));
	if (ioctl(fd, VIDIOC_G_INPUT, &v->input) < 0 ||
	    ioctl(fd, VIDIOC_G_FMT, &fmt) < 0)
		return;
	v->pix = fmt.fmt.pix;
	for (int i = 0; i < 5; i++) {
		struct v4l2_control c = { .id = gchd_ctrl_ids[i] };

		v->ctrl[i] = ioctl(fd, VIDIOC_G_CTRL, &c) == 0 ? c.value : -1;
	}
	v->ok = true;
}

/*
 * Set back the settings found before the capture (BEFORE), where the device
 * still has the source's values (APPLIED): a change by another program
 * while the source captured stays.
 */
static void gchd_settings_restore(const char *path,
				  const struct gchd_dev_settings *before,
				  const struct gchd_dev_settings *applied)
{
	struct gchd_dev_settings now;
	int fd;

	if (!before->ok || !applied->ok)
		return;
	fd = open(path, O_RDWR | O_NONBLOCK);
	if (fd < 0)
		return;
	gchd_settings_read(fd, &now);
	if (now.ok) {
		if (now.input == applied->input && before->input != now.input) {
			unsigned int in = before->input;

			ioctl(fd, VIDIOC_S_INPUT, &in);
		}
		if (now.pix.width == applied->pix.width &&
		    now.pix.height == applied->pix.height &&
		    now.pix.pixelformat == applied->pix.pixelformat &&
		    (before->pix.width != now.pix.width ||
		     before->pix.height != now.pix.height ||
		     before->pix.pixelformat != now.pix.pixelformat)) {
			struct v4l2_format fmt = {
				.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
			};

			fmt.fmt.pix = before->pix;
			ioctl(fd, VIDIOC_S_FMT, &fmt);
		}
		for (int i = 0; i < 5; i++) {
			struct v4l2_control c = { .id = gchd_ctrl_ids[i],
						  .value = before->ctrl[i] };

			if (before->ctrl[i] < 0 || now.ctrl[i] != applied->ctrl[i] ||
			    now.ctrl[i] == before->ctrl[i])
				continue;
			ioctl(fd, VIDIOC_S_CTRL, &c);
		}
	}
	close(fd);
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
	struct gchd_dev_settings before = { 0 }, applied = { 0 };
	unsigned char *buf;
	int vi = -1, ai = -1;

	s->fd = open(path, O_RDWR | O_NONBLOCK);
	if (s->fd < 0) {
		blog(LOG_WARNING, "[gchd] %s: %s", path, strerror(errno));
		goto out;
	}
	gchd_settings_read(s->fd, &before);
	gchd_apply(s, s->fd);
	gchd_settings_read(s->fd, &applied);

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
	/* The device's own settings again (OBS keeps its settings itself). */
	gchd_settings_restore(path, &before, &applied);
}

static void *gchd_thread(void *data)
{
	struct gchd_src *s = data;
	char path[300];

	bool told = false;

	os_set_thread_name("gchd-capture");
	blog(LOG_INFO, "[gchd] capture started (source shown)");
	while (!s->stop) {
		if (s->device && *s->device)
			snprintf(path, sizeof(path), "%s", s->device);
		else if (!gchd_find(path, sizeof(path)))
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

static void gchd_update(void *data, obs_data_t *settings)
{
	struct gchd_src *s = data;
	bool running = s->thread_running;

	if (running)
		gchd_stop(s);
	bfree(s->device);
	s->device = bstrdup(obs_data_get_string(settings, "device"));
	s->input = (int)obs_data_get_int(settings, "input");
	s->output = (int)obs_data_get_int(settings, "output");
	s->rate = obs_data_get_double(settings, "rate");
	s->bitrate_mode = (int)obs_data_get_int(settings, "bitrate_mode");
	s->profile = (int)obs_data_get_int(settings, "profile");
	s->stretch = obs_data_get_bool(settings, "stretch");
	if (running || s->shown)
		gchd_start(s);
}

static void *gchd_create(obs_data_t *settings, obs_source_t *source)
{
	struct gchd_src *s = bzalloc(sizeof(*s));

	s->source = source;
	s->fd = -1;
	/* OBS deinterlacing, once (the user can change it afterwards). */
	if (!obs_data_get_bool(settings, "deinterlace_set")) {
		obs_source_set_deinterlace_mode(source,
						OBS_DEINTERLACE_MODE_YADIF_2X);
		obs_source_set_deinterlace_field_order(
			source, OBS_DEINTERLACE_FIELD_ORDER_TOP);
		obs_data_set_bool(settings, "deinterlace_set", true);
	}
	gchd_update(s, settings);
	return s;
}

static void gchd_destroy(void *data)
{
	struct gchd_src *s = data;

	gchd_stop(s);
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
	bool analog = obs_data_get_int(settings, "input") >= 2;
	int o = (int)obs_data_get_int(settings, "output");
	bool mobile, vbr;
	double lo, hi, def, r;

	UNUSED_PARAMETER(p);
	if (o < 0 || o > 2)
		o = 0;
	mobile = o == 2;
	vbr = mobile || obs_data_get_int(settings, "bitrate_mode") ==
				V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
	lo = vbr ? gchd_vbr_min[o] : GCHD_CBR_MIN;
	hi = vbr ? gchd_vbr_max[o] : GCHD_CBR_MAX;
	def = vbr ? gchd_vbr_def[o] : GCHD_CBR_DEF;
	obs_property_set_visible(obs_properties_get(props, "output"), analog);
	obs_property_set_visible(obs_properties_get(props, "stretch"), analog);
	obs_property_set_visible(obs_properties_get(props, "bitrate_mode"),
				 analog && !mobile);
	obs_property_set_visible(obs_properties_get(props, "rate"), analog);
	obs_property_float_set_limits(obs_properties_get(props, "rate"), lo, hi,
				      0.01);
	r = obs_data_get_double(settings, "rate");
	if (r < lo - 1e-6 || r > hi + 1e-6)
		obs_data_set_double(settings, "rate", def);
	return true;
}

static obs_properties_t *gchd_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;
	char path[300];

	UNUSED_PARAMETER(data);
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
	obs_property_list_add_int(p, "Source size (720x480 / 720x576)", 0);
	obs_property_list_add_int(p, "640x480", 1);
	obs_property_list_add_int(p, "480x360 (VBR only)", 2);
	obs_property_set_modified_callback(p, gchd_settings_changed);

	p = obs_properties_add_list(props, "bitrate_mode", "Bitrate mode",
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, "Constant (CBR), 11250 kbit/s",
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
