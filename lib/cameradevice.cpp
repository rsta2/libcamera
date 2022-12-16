//
// cameradevice.cpp
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#include <camera/cameradevice.h>
#include <camera/camerabuffer.h>
#include <circle/atomic.h>

CCameraDevice::CCameraDevice (void)
:	m_nBuffers (0),
	m_pBufferReadyHandler (nullptr)
{
}

CCameraDevice::~CCameraDevice (void)
{
	FreeBuffers ();
}

bool CCameraDevice::AllocateBuffers (unsigned nBuffers)
{
	assert (!m_nBuffers);

	if (nBuffers < 3)
	{
		nBuffers = 3;
	}
	else if (nBuffers > MaxBuffers)
	{
		nBuffers = MaxBuffers;
	}

	TFormatInfo Info = GetFormatInfo ();
	assert (Info.ImageSize);

	for (unsigned i = 0; i < nBuffers; i++)
	{
		m_pBuffer[i] = new CCameraBuffer;
		assert (m_pBuffer[i]);

		m_nBuffers++;

		if (!m_pBuffer[i]->Setup (Info.ImageSize))
		{
			FreeBuffers ();

			return false;
		}
	}

	m_nInPtr = 0;
	m_nOutPtr = 0;

	return true;
}

void CCameraDevice::FreeBuffers (void)
{
	for (unsigned i = 0; i < m_nBuffers; i++)
	{
		delete m_pBuffer[i];
	}

	m_nBuffers = 0;
}

CCameraBuffer *CCameraDevice::GetFreeBuffer (void)
{
	assert (m_nBuffers);

	CCameraBuffer *pBuffer = nullptr;

	if ((AtomicGet (&m_nInPtr) + 1) % m_nBuffers != (unsigned) AtomicGet (&m_nOutPtr))
	{
		pBuffer = m_pBuffer[AtomicGet (&m_nInPtr)];
		assert (pBuffer);

		pBuffer->InvalidateCache ();
	}

	return pBuffer;
}

void CCameraDevice::BufferReady (unsigned nSequence)
{
	AtomicSet (&m_nInPtr, (AtomicGet (&m_nInPtr) + 1) % m_nBuffers);

	if (m_pBufferReadyHandler)
	{
		(*m_pBufferReadyHandler) (nSequence, m_pBufferReadyParam);
	}
}

CCameraBuffer *CCameraDevice::GetNextBuffer (void)
{
	assert (m_nBuffers);

	CCameraBuffer *pBuffer = nullptr;

	if (AtomicGet (&m_nInPtr) != AtomicGet (&m_nOutPtr))
	{
		pBuffer = m_pBuffer[AtomicGet (&m_nOutPtr)];
		assert (pBuffer);
	}

	return pBuffer;
}

void CCameraDevice::BufferProcessed (void)
{
	AtomicSet (&m_nOutPtr, (AtomicGet (&m_nOutPtr) + 1) % m_nBuffers);
}

void CCameraDevice::RegisterBufferReadyHandler (TBufferReadyHandler *pHandler, void *pParam)
{
	m_pBufferReadyParam = pParam;
	m_pBufferReadyHandler = pHandler;
}

CString CCameraDevice::FormatToString (TFormatCode Format)
{
	static const char s_ColorComponents[] = "RGGB";		// must match TColorComponent

	CString Result;
	Result.Format ("S%c%c%c%c%u", s_ColorComponents[GetFormatColor (Format, 0, 0)],
				      s_ColorComponents[GetFormatColor (Format, 1, 0)],
				      s_ColorComponents[GetFormatColor (Format, 0, 1)],
				      s_ColorComponents[GetFormatColor (Format, 1, 1)],
				      GetFormatDepth (Format));

	if (IsFormatPacked (Format))
	{
		Result.Append ("P");
	}

	return Result;
}
