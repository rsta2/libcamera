//
// cameracontrol.cpp
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#include <camera/cameracontrol.h>
#include <assert.h>

CCameraControl::CCameraControl (void)
:	m_Info {false, 0, 0, 1, 0},
	m_nValue (0)
{
}

CCameraControl::CCameraControl (int nMin, int nMax, int nStep, int nDefault)
{
	Setup (nMin, nMax, nStep, nDefault);
}

void CCameraControl::Setup (int nMin, int nMax, int nStep, int nDefault)
{
	assert (nMin <= nMax);
	assert (nMin <= nDefault);
	assert (nDefault <= nMax);

	m_Info.Supported = true;
	m_Info.Min = nMin;
	m_Info.Max = nMax;
	m_Info.Step = nStep;
	m_Info.Default = nDefault;

	m_nValue = nDefault;
}

int CCameraControl::GetValue (void) const
{
	return m_nValue;
}

bool CCameraControl::SetValue (int nValue)
{
	if (!m_Info.Supported)
	{
		return false;
	}

	if (m_Info.Min <= nValue && nValue <= m_Info.Max)
	{
		m_nValue = nValue;

		return true;
	}

	return false;
}

CCameraControl::TControlInfo CCameraControl::GetInfo (void) const
{
	return m_Info;
}
