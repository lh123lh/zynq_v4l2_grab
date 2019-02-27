#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <linux/videodev2.h>

#include "v4l2grab.h"

#define TRUE 1
#define FALSE 0

#define FILE_VIDEO "/dev/video0"
#define BMP "./image_bmp.bmp"
#define RAW "./image_raw.raw"

#define IMAGEWIDTH 1280
#define IMAGEHEIGHT 720

static int fd;
static struct v4l2_capability cap;
struct v4l2_fmtdesc fmtdesc;
struct v4l2_format fmt, fmtack;
struct v4l2_streamparm setfps;
struct v4l2_requestbuffers req;
struct v4l2_buffer buf;
struct v4l2_plane plane;
enum v4l2_buf_type type;
unsigned char frame_buffer[IMAGEWIDTH * IMAGEHEIGHT * 3];

struct buffer
{
	void *start;
	unsigned int length;
} * buffers;

int init_v4l2(void)
{
	int i;
	int ret = 0;

	//opendev
	if ((fd = open(FILE_VIDEO, O_RDWR)) == -1)
	{
		printf("Error opening V4L interface\n");
		return (FALSE);
	}

	//query cap
	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
	{
		printf("Error opening device %s: unable to query device.\n", FILE_VIDEO);
		return (FALSE);
	}
	else
	{
		printf("driver:\t\t%s\n", cap.driver);
		printf("card:\t\t%s\n", cap.card);
		printf("bus_info:\t%s\n", cap.bus_info);
		printf("version:\t%d\n", cap.version);
		printf("capabilities:\t%x\n", cap.capabilities);

		if (((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == V4L2_CAP_VIDEO_CAPTURE) || ((cap.capabilities & V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE))
		{
			printf("Device %s: supports capture.\n", FILE_VIDEO);
		}

		if ((cap.capabilities & V4L2_CAP_STREAMING) == V4L2_CAP_STREAMING)
		{
			printf("Device %s: supports streaming.\n", FILE_VIDEO);
		}
	}

	//emu all support fmt
	fmtdesc.index = 0;
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	printf("Support format:\n");
	while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != -1)
	{
		printf("\t%d.%s\n", fmtdesc.index + 1, fmtdesc.description);
		fmtdesc.index++;
	}

	//set fmt
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24; //V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.height = IMAGEHEIGHT;
	fmt.fmt.pix.width = IMAGEWIDTH;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

	if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
	{
		printf("Unable to set format\n");
		return FALSE;
	}
	if (ioctl(fd, VIDIOC_G_FMT, &fmt) == -1)
	{
		printf("Unable to get format\n");
		return FALSE;
	}
	{
		printf("fmt.type:\t\t%d\n", fmt.type);
		printf("pix.pixelformat:\t%c%c%c%c\n", fmt.fmt.pix.pixelformat & 0xFF, (fmt.fmt.pix.pixelformat >> 8) & 0xFF, (fmt.fmt.pix.pixelformat >> 16) & 0xFF, (fmt.fmt.pix.pixelformat >> 24) & 0xFF);
		printf("pix.height:\t\t%d\n", fmt.fmt.pix.height);
		printf("pix.width:\t\t%d\n", fmt.fmt.pix.width);
		printf("pix.field:\t\t%d\n", fmt.fmt.pix.field);
	}
	//set fps
	setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	setfps.parm.capture.timeperframe.numerator = 10;
	setfps.parm.capture.timeperframe.denominator = 10;

	printf("init %s \t[OK]\n", FILE_VIDEO);

	return TRUE;
}

int v4l2_grab(void)
{
	unsigned int n_buffers;

	memset(&buf, 0, sizeof buf);
	memset(&plane, 0, sizeof plane);

	//request for 4 buffers
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1)
	{
		printf("request for buffers error\n");
	}

	//mmap for buffers
	buffers = malloc(req.count * sizeof(*buffers));
	if (!buffers)
	{
		printf("Out of memory\n");
		return (FALSE);
	}

	for (n_buffers = 0; n_buffers < req.count; n_buffers++)
	{
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n_buffers;
		buf.length = VIDEO_MAX_PLANES;

		plane.length = VIDEO_MAX_PLANES;
		// plane.m.mem_offset = *(int *)&buffers[n_buffers].start;

		buf.m.planes = &plane;

		//query buffers
		if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
		{
			printf("query buffer error\n");
			return (FALSE);
		}

		buffers[n_buffers].length = buf.length;
		//map
		buffers[n_buffers].start = mmap(NULL, plane.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, plane.m.mem_offset);
		if (buffers[n_buffers].start == MAP_FAILED)
		{
			printf("buffer map error\n");
			return (FALSE);
		}
	}

	//queue
	for (n_buffers = 0; n_buffers < req.count; n_buffers++)
	{
		buf.index = n_buffers;
		ioctl(fd, VIDIOC_QBUF, &buf);
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ioctl(fd, VIDIOC_STREAMON, &type);

	usleep(100000);

	ioctl(fd, VIDIOC_DQBUF, &buf);

	ioctl(fd, VIDIOC_STREAMOFF, &type);

	return (TRUE);
}

int yuyv_2_rgb888(void)
{
	int i, j;
	unsigned char y1, y2, u, v;
	int r1, g1, b1, r2, g2, b2;
	char *pointer;

	pointer = buffers[0].start;

	for (i = 0; i < IMAGEHEIGHT; i++)
	{
		for (j = 0; j < (IMAGEWIDTH / 2); j++)
		{
			y1 = *(pointer + (i * (IMAGEWIDTH / 2) + j) * 4);
			u = *(pointer + (i * (IMAGEWIDTH / 2) + j) * 4 + 1);
			y2 = *(pointer + (i * (IMAGEWIDTH / 2) + j) * 4 + 2);
			v = *(pointer + (i * (IMAGEWIDTH / 2) + j) * 4 + 3);

			r1 = y1 + 1.042 * (v - 128);
			g1 = y1 - 0.34414 * (u - 128) - 0.71414 * (v - 128);
			b1 = y1 + 1.772 * (u - 128);

			r2 = y2 + 1.042 * (v - 128);
			g2 = y2 - 0.34414 * (u - 128) - 0.71414 * (v - 128);
			b2 = y2 + 1.772 * (u - 128);

			if (r1 > 255)
				r1 = 255;
			else if (r1 < 0)
				r1 = 0;

			if (b1 > 255)
				b1 = 255;
			else if (b1 < 0)
				b1 = 0;

			if (g1 > 255)
				g1 = 255;
			else if (g1 < 0)
				g1 = 0;

			if (r2 > 255)
				r2 = 255;
			else if (r2 < 0)
				r2 = 0;

			if (b2 > 255)
				b2 = 255;
			else if (b2 < 0)
				b2 = 0;

			if (g2 > 255)
				g2 = 255;
			else if (g2 < 0)
				g2 = 0;

			*(frame_buffer + ((IMAGEHEIGHT - 1 - i) * (IMAGEWIDTH / 2) + j) * 6) = (unsigned char)b1;
			*(frame_buffer + ((IMAGEHEIGHT - 1 - i) * (IMAGEWIDTH / 2) + j) * 6 + 1) = (unsigned char)g1;
			*(frame_buffer + ((IMAGEHEIGHT - 1 - i) * (IMAGEWIDTH / 2) + j) * 6 + 2) = (unsigned char)r1;
			*(frame_buffer + ((IMAGEHEIGHT - 1 - i) * (IMAGEWIDTH / 2) + j) * 6 + 3) = (unsigned char)b2;
			*(frame_buffer + ((IMAGEHEIGHT - 1 - i) * (IMAGEWIDTH / 2) + j) * 6 + 4) = (unsigned char)g2;
			*(frame_buffer + ((IMAGEHEIGHT - 1 - i) * (IMAGEWIDTH / 2) + j) * 6 + 5) = (unsigned char)r2;
		}
	}
	printf("change to RGB OK \n");
}

int raw_2_rgb888(void)
{
	int i, j;
	int r, g, b;
	char *pointer;

	pointer = buffers[0].start;

	for (i = 0; i < IMAGEHEIGHT; i++)
	{
		for (j = 0; j < (IMAGEWIDTH); j++)
		{
			r = *(pointer + (i * IMAGEWIDTH + j) * 3 + 0);
			g = *(pointer + (i * IMAGEWIDTH + j) * 3 + 1);
			b = *(pointer + (i * IMAGEWIDTH + j) * 3 + 2);

			*(frame_buffer + ((IMAGEHEIGHT - 1 - i) * (IMAGEWIDTH) + j) * 3 + 0) = (unsigned char)r;
			*(frame_buffer + ((IMAGEHEIGHT - 1 - i) * (IMAGEWIDTH) + j) * 3 + 1) = (unsigned char)g;
			*(frame_buffer + ((IMAGEHEIGHT - 1 - i) * (IMAGEWIDTH) + j) * 3 + 2) = (unsigned char)b;
		}
	}

	printf("change raw to RGB OK \n");

	return 0;
}

int close_v4l2(void)
{
	if (fd != -1)
	{
		close(fd);
		return (TRUE);
	}
	return (FALSE);
}

int main(void)
{
	FILE *fp1, *fp2;

	int grab;
	BITMAPFILEHEADER bf;
	BITMAPINFOHEADER bi;

	fp1 = fopen(BMP, "wb");
	if (!fp1)
	{
		printf("open " BMP "error\n");
		return (FALSE);
	}

	fp2 = fopen(RAW, "wb");
	if (!fp2)
	{
		printf("open " RAW "error\n");
		return (FALSE);
	}

	if (init_v4l2() == FALSE)
	{
		return (FALSE);
	}

	//Set BITMAPINFOHEADER
	bi.biSize = 40;
	bi.biWidth = IMAGEWIDTH;
	bi.biHeight = IMAGEHEIGHT;
	bi.biPlanes = 1;
	bi.biBitCount = 24;
	bi.biCompression = 0;
	bi.biSizeImage = IMAGEWIDTH * IMAGEHEIGHT * 3;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	//Set BITMAPFILEHEADER
	bf.bfType = 0x4d42;
	bf.bfSize = 54 + bi.biSizeImage;
	bf.bfReserved = 0;
	bf.bfOffBits = 54;

	grab = v4l2_grab();
	if(grab == FALSE)
	{
		printf("grab failed\n");

		exit(-1);
	}
	else
	{
		printf("grab ok\n");
	}

	usleep(200000);

	fwrite(buffers[0].start, IMAGEHEIGHT * IMAGEWIDTH * 2, 1, fp2);
	printf("save " RAW " OK\n");

	// yuyv_2_rgb888();
	raw_2_rgb888();
	fwrite(&bf, 14, 1, fp1);
	fwrite(&bi, 40, 1, fp1);
	fwrite(frame_buffer, bi.biSizeImage, 1, fp1);
	printf("save " BMP " OK\n");

	fclose(fp1);
	fclose(fp2);
	close_v4l2();

	return (TRUE);
}
