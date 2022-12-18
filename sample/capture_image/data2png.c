//
// data2png.c
//
// Converts RGB565 raw image files to PNG files
// Requires LibGD (https://libgd.github.io/) for build
//
#include <stdio.h>
#include <assert.h>
#include <gd.h>
#include "../config.h"

typedef unsigned short color_t;
typedef color_t image_t[HEIGHT][WIDTH];

int main (int argc, char **argv)
{
	if (argc != 3)
	{
		fprintf (stderr, "Usage: %s infile outfile\n", argv[0]);

		return 1;
	}

	gdImagePtr im = gdImageCreateTrueColor (WIDTH, HEIGHT);
	assert (im);

	FILE *fin = fopen (argv[1], "rb");
	if (!fin)
	{
		fprintf (stderr, "%s: Cannot open: %s\n", argv[0], argv[1]);

		return 1;
	}

	image_t *buf = (image_t *) malloc (sizeof (image_t));
	assert (buf);

	int cnt = fread (buf, WIDTH * sizeof (color_t), HEIGHT, fin);
	if (cnt != HEIGHT)
	{
		fprintf (stderr, "%s: File is too small\n", argv[0]);

		fclose (fin);
		free (buf);

		return 1;
	}

	fclose (fin);

	for (int y = 0; y < HEIGHT; y++)
	{
		for (int x = 0; x < WIDTH; x++)
		{
			color_t colorin = (*buf)[y][x];

			unsigned colorout = 0xFFFFFF;
			if (colorin != 0xFFFF)
			{
				unsigned red   = ((colorin >> 11) & 0x1F) << 3;
				unsigned green = ((colorin >> 5)  & 0x3F) << 2;
				unsigned blue  = ((colorin >> 0)  & 0x1F) << 3;

				colorout = blue | green << 8 | red << 16;
			}

			gdImageSetPixel (im, x, y, colorout);
		}
	}

	free (buf);

	FILE *fout = fopen (argv[2], "wb");
	if (!fout)
	{
		fprintf (stderr, "%s: Cannot create: %s\n", argv[0], argv[2]);

		gdImageDestroy (im);

		return 1;
	}

	gdImagePng (im, fout);

	fclose (fout);

	gdImageDestroy (im);

	return 0;
}
