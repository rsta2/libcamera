//
// camerabuffer.cpp
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#include <camera/camerabuffer.h>
#include <circle/synchronize.h>
#include <circle/bcm2835.h>
#include <assert.h>
#include "math.h"

CCameraBuffer::CCameraBuffer (void)
:	m_nSize (0),
	m_pBuffer (nullptr),
	m_nWidth (0),
	m_nHeight (0),
	m_nBytesPerLine (0),
	m_Format (CCameraDevice::FormatUnknown),
	m_ColorFactor {65536, 65536, 65536},
	m_nSeed (1)
{
}

CCameraBuffer::~CCameraBuffer (void)
{
	delete [] m_pBuffer;
	m_pBuffer = nullptr;
}

bool CCameraBuffer::Setup (size_t nSize)
{
	delete [] m_pBuffer;

	assert (nSize);
	m_nSize = nSize;

	m_pBuffer = new u8[nSize];

	return !!m_pBuffer;
}

void *CCameraBuffer::GetPtr (void) const
{
	assert (m_pBuffer);
	return m_pBuffer;
}

uintptr CCameraBuffer::GetDMAAddress (void) const
{
	assert (m_pBuffer);
	return BUS_ADDRESS (reinterpret_cast<uintptr> (m_pBuffer));
}

void CCameraBuffer::SetSequenceNumber (unsigned nSequence)
{
	m_nSequence = nSequence;
}

unsigned CCameraBuffer::GetSequenceNumber (void) const
{
	return m_nSequence;
}

void CCameraBuffer::SetTimestamp (unsigned nTimestamp)
{
	m_nTimestamp = nTimestamp;
}

unsigned CCameraBuffer::GetTimestamp (void) const
{
	return m_nTimestamp;
}

void CCameraBuffer::SetFormat (unsigned nWidth, unsigned nHeight, unsigned nBytesPerLine,
			       CCameraDevice::TFormatCode Format)
{
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_nBytesPerLine = nBytesPerLine;
	m_Format = Format;

	m_ColorFactor[0] = 65536;
	m_ColorFactor[1] = 65536;
	m_ColorFactor[2] = 65536;
}

// This method reads the color values from a captured image in Bayer format
// (normally 16 bits occupied per value, 10 bits valid) and  returns the
// color components.
CCameraBuffer::TPixel CCameraBuffer::GetPixel (unsigned x, unsigned y)
{
	// We ignore the border lines/cols to make the processing more simple.
	if (   x == 0 || x >= m_nWidth-1
	    || y == 0 || y >= m_nHeight-1)
	{
		return {0, 0, 0};
	}

	typedef u16 TImage[][m_nBytesPerLine / sizeof (u16)];
	TImage *p = reinterpret_cast<TImage *> (m_pBuffer);

	// L(x, y) is the color value at position x / y
	#define L(x, y) ((*(p))[(y)][(x)])

	u16 CL = L (x, y);
	u16 CR, CG, CB;

	// For interpolating the missing color values see:
	// http://siliconimaging.com/RGB%20Bayer.htm
	switch (CCameraDevice::GetFormatColor (m_Format, x, y))
	{
	case CCameraDevice::R:
		CR = CL;
		CG = (L (x, y-1) + L (x, y+1) + L (x-1, y) + L (x+1, y)) / 4;
		CB = (L (x-1, y-1) + L (x+1, y-1) + L (x-1, y+1) + L (x+1, y+1)) / 4;
		break;

	case CCameraDevice::GR:
		CR = (L (x-1, y) + L (x+1, y)) / 2;
		CG = CL;
		CB = (L (x, y-1) + L (x, y+1)) / 2;
		break;

	case CCameraDevice::GB:
		CR = (L (x, y-1) + L (x, y+1)) / 2;
		CG = CL;
		CB = (L (x-1, y) + L (x+1, y)) / 2;
		break;

	case CCameraDevice::B:
		CR = (L (x-1, y-1) + L (x+1, y-1) + L (x-1, y+1) + L (x+1, y+1)) / 4;
		CG = (L (x, y-1) + L (x, y+1) + L (x-1, y) + L (x+1, y)) / 4;
		CB = CL;
		break;
	}

	return {CR, CG, CB};
}

u32 CCameraBuffer::GetPixelRGB888 (unsigned x, unsigned y)
{
	const TPixel Pixel = GetPixel (x, y);

	unsigned nShift = CCameraDevice::GetFormatDepth (m_Format) - 8 + 16;
	u16 CR = Pixel.R * m_ColorFactor[0] >> nShift;
	u16 CG = Pixel.G * m_ColorFactor[1] >> nShift;
	u16 CB = Pixel.B * m_ColorFactor[2] >> nShift;

	if (CR > 255) CR = 255;
	if (CG > 255) CG = 255;
	if (CB > 255) CB = 255;

	return (CB << 16) | (CG << 8) | CR;
}

void CCameraBuffer::ConvertToRGB888 (void *pOutBuffer)
{
	assert (m_nWidth);
	assert (m_nHeight);

	u8 *p = static_cast<u8 *> (pOutBuffer);
	assert (p);

	for (unsigned y = 0; y < m_nHeight; y++)
	{
		for (unsigned x = 0; x < m_nWidth; x++)
		{
			const u32 nPixel = GetPixelRGB888 (x, y);

			p[0] = nPixel & 0xFF;
			p[1] = (nPixel >> 8) & 0xFF;
			p[2] = (nPixel >> 16) & 0xFF;

			p += 3;
		}
	}
}

u16 CCameraBuffer::GetPixelRGB565 (unsigned x, unsigned y)
{
	const TPixel Pixel = GetPixel (x, y);

	unsigned nShift = CCameraDevice::GetFormatDepth (m_Format) - 5 + 16;
	u16 CR = Pixel.R * m_ColorFactor[0] >> nShift;
	u16 CG = Pixel.G * m_ColorFactor[1] >> (nShift - 1);
	u16 CB = Pixel.B * m_ColorFactor[2] >> nShift;

	if (CR > 31) CR = 31;
	if (CG > 63) CG = 63;
	if (CB > 31) CB = 31;

	return (CR << 11) | (CG << 5) | CB;
}

void CCameraBuffer::ConvertToRGB565 (void *pOutBuffer)
{
	assert (m_nWidth);
	assert (m_nHeight);

	u16 *p = static_cast<u16 *> (pOutBuffer);
	assert (p);

	for (unsigned y = 0; y < m_nHeight; y++)
	{
		for (unsigned x = 0; x < m_nWidth; x++)
		{
			*p++ = GetPixelRGB565 (x, y);
		}
	}
}

// Based on the file main.cpp from the archive iwp.zip, download here:
//	http://www.fer.unizg.hr/ipg/resources/color_constancy/
//
// Copyright (c) University of Zagreb, Faculty of Electrical Engineering and Computing
// Authors: Nikola Banic <nikola.banic@fer.hr> and Sven Loncaric <sven.loncaric@fer.hr>
void CCameraBuffer::WhiteBalance (unsigned N, unsigned M)
{
	assert (N > 0);
	assert (M > 0);

	m_ColorFactor[0] = 65536;
	m_ColorFactor[1] = 65536;
	m_ColorFactor[2] = 65536;

	unsigned Result[3] = {0, 0, 0};		// R, G, B

	for (unsigned i = 0; i < M; i++)
	{
		unsigned Max[3] = {0, 0, 0};

		for (unsigned j = 0; j < N ; j++)
		{
			// ignore the border pixels
			unsigned x = 1 + rand_r (&m_nSeed) * (m_nWidth - 2) / RAND_MAX;
			unsigned y = 1 + rand_r (&m_nSeed) * (m_nHeight - 2) / RAND_MAX;

			const TPixel Pixel = GetPixel (x, y);

			if (Max[0] < Pixel.R) Max[0] = Pixel.R;
			if (Max[1] < Pixel.G) Max[1] = Pixel.G;
			if (Max[2] < Pixel.B) Max[2] = Pixel.B;
		}

		Result[0] += Max[0];
		Result[1] += Max[1];
		Result[2] += Max[2];
	}

	float fSum = Result[0] * Result[0] + Result[1] * Result[1] + Result[2] * Result[2];
	fSum /= 3;
	fSum = sqrtf (fSum);

	m_ColorFactor[0] = 65536 * fSum / Result[0];
	m_ColorFactor[1] = 65536 * fSum / Result[1];
	m_ColorFactor[2] = 65536 * fSum / Result[2];
}

void CCameraBuffer::InvalidateCache (void)
{
	CleanAndInvalidateDataCacheRange (reinterpret_cast<uintptr> (m_pBuffer), m_nSize);
}
