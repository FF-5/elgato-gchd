/*
 * stream-restart: STREAMOFF -> STREAMON on one open file handle.
 *
 * Opens the device, maps 4 buffers, and runs N rounds of: STREAMON,
 * dequeue F buffers, STREAMOFF. The file handle stays open between rounds.
 * Prints per round the number of buffers and bytes received.
 *
 *   gcc -O2 -o stream-restart stream-restart.c
 *   ./stream-restart /dev/video0 3 100
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define NBUF 4

static double now(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
}

static int xioctl(int fd, unsigned long req, void *arg, const char *name)
{
	int r;

	do {
		r = ioctl(fd, req, arg);
	} while (r < 0 && errno == EINTR);
	if (r < 0)
		fprintf(stderr, "%s: %s\n", name, strerror(errno));
	return r;
}

int main(int argc, char **argv)
{
	const char *dev = argc > 1 ? argv[1] : "/dev/video0";
	int rounds = argc > 2 ? atoi(argv[2]) : 3;
	int frames = argc > 3 ? atoi(argv[3]) : 100;
	struct v4l2_requestbuffers req;
	struct v4l2_buffer b;
	void *map[NBUF];
	size_t len[NBUF];
	int fd, i, round;

	fd = open(dev, O_RDWR);
	if (fd < 0) {
		perror(dev);
		return 1;
	}
	memset(&req, 0, sizeof(req));
	req.count = NBUF;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if (xioctl(fd, VIDIOC_REQBUFS, &req, "REQBUFS") || req.count < NBUF)
		return 1;
	for (i = 0; i < NBUF; ++i) {
		memset(&b, 0, sizeof(b));
		b.type = req.type;
		b.memory = req.memory;
		b.index = i;
		if (xioctl(fd, VIDIOC_QUERYBUF, &b, "QUERYBUF"))
			return 1;
		len[i] = b.length;
		map[i] = mmap(NULL, b.length, PROT_READ, MAP_SHARED, fd, b.m.offset);
		if (map[i] == MAP_FAILED) {
			perror("mmap");
			return 1;
		}
	}

	for (round = 1; round <= rounds; ++round) {
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		double t0 = now(), first = 0;
		long bytes = 0;
		int got = 0;

		for (i = 0; i < NBUF; ++i) {
			memset(&b, 0, sizeof(b));
			b.type = type;
			b.memory = V4L2_MEMORY_MMAP;
			b.index = i;
			if (xioctl(fd, VIDIOC_QBUF, &b, "QBUF"))
				return 1;
		}
		printf("round %d: STREAMON\n", round);
		fflush(stdout);
		if (xioctl(fd, VIDIOC_STREAMON, &type, "STREAMON"))
			return 1;
		while (got < frames) {
			struct pollfd p = { .fd = fd, .events = POLLIN };

			if (poll(&p, 1, 30000) <= 0) {
				printf("round %d: no buffer for 30 s\n", round);
				break;
			}
			memset(&b, 0, sizeof(b));
			b.type = type;
			b.memory = V4L2_MEMORY_MMAP;
			if (xioctl(fd, VIDIOC_DQBUF, &b, "DQBUF"))
				break;
			if (!got)
				first = now() - t0;
			++got;
			bytes += b.bytesused;
			if (xioctl(fd, VIDIOC_QBUF, &b, "QBUF"))
				break;
		}
		if (xioctl(fd, VIDIOC_STREAMOFF, &type, "STREAMOFF"))
			return 1;
		printf("round %d: %d buffers, %ld bytes, first after %.2f s, STREAMOFF after %.2f s\n",
		       round, got, bytes, first, now() - t0);
		fflush(stdout);
	}
	for (i = 0; i < NBUF; ++i)
		munmap(map[i], len[i]);
	close(fd);
	return 0;
}
