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

void CCameraBuffer::InvalidateCache (void)
{
	CleanAndInvalidateDataCacheRange (reinterpret_cast<uintptr> (m_pBuffer), m_nSize);
}
