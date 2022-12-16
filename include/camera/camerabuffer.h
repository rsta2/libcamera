//
// camerabuffer.h
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#ifndef _camera_camerabuffer_h
#define _camera_camerabuffer_h

#include <camera/cameradevice.h>
#include <circle/types.h>

class CCameraBuffer
{
public:
	CCameraBuffer (void);
	~CCameraBuffer (void);

	bool Setup (size_t nSize);

	void *GetPtr (void) const;
	uintptr GetDMAAddress (void) const;

	void SetSequenceNumber (unsigned nSequence);
	unsigned GetSequenceNumber (void) const;

	void SetTimestamp (unsigned nTimestamp);
	unsigned GetTimestamp (void) const;

	void SetFormat (unsigned nWidth, unsigned nHeight, unsigned nBytesPerLine,
			CCameraDevice::TFormatCode Format);

	u16 GetPixelRGB565 (unsigned x, unsigned y);

	void InvalidateCache (void);


private:
	size_t m_nSize;
	u8 *m_pBuffer;

	unsigned m_nSequence;
	unsigned m_nTimestamp;

	unsigned m_nWidth;
	unsigned m_nHeight;
	unsigned m_nBytesPerLine;
	CCameraDevice::TFormatCode m_Format;
};

#endif
