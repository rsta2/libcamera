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

CCameraBuffer::CCameraBuffer (void)
:	m_nSize (0),
	m_pBuffer (nullptr)
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
}

// This method reads the color values from a captured image in Bayer format
// (normally 16 bits occupied per value, 10 bits valid) and  returns a RGB565
// value.
u16 CCameraBuffer::GetPixelRGB565 (unsigned x, unsigned y)
{
	// We ignore the border lines/cols to make the processing more simple.
	if (   x == 0 || x >= m_nWidth-1
	    || y == 0 || y >= m_nHeight-1)
	{
		return 0;
	}

	typedef u16 TImage[][m_nBytesPerLine / sizeof (u16)];
	TImage *p = reinterpret_cast<TImage *> (m_pBuffer);

	unsigned nShift = CCameraDevice::GetFormatDepth (m_Format) - 5;

	// L(x, y) is the color value at position x/y, shifted to the range 0 to 31.
	#define L(x, y) ((*(p))[(y)][(x)] >> (nShift))

	u8 CL = L (x, y);
	u8 CR, CG, CB;

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

	// Normally (CG << 5), but to have a 0-31 range for all colors.
	return (CR << 11) | (CG << 6) | CB;
}

void CCameraBuffer::InvalidateCache (void)
{
	CleanAndInvalidateDataCacheRange (reinterpret_cast<uintptr> (m_pBuffer), m_nSize);
}
