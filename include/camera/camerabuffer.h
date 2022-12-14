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

	void InvalidateCache (void);

private:
	size_t m_nSize;
	u8 *m_pBuffer;

	unsigned m_nSequence;
	unsigned m_nTimestamp;
};

#endif
