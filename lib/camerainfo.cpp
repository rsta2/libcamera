//
// camerainfo.cpp
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#include <camera/camerainfo.h>
#include <assert.h>

const CCameraInfo::TMachineInfo CCameraInfo::s_MachineInfo[] =
{	// Machine			I2C PWR LED
	//{MachineModelBRelease1MB256,	0,  27,  5},
	//{MachineModelBRelease2MB256,	0,  21,  5},
	{MachineModelBRelease2MB512,	0,  21,  5},
	{MachineModelAPlus,		1,  41, 32},
	{MachineModelBPlus,		1,  41, 32},
	//{MachineModelZero,		1,  41, 32},	// v1.3 only
	{MachineModelZeroW,		1,  44, 40},
	{MachineModelZero2W,		2,  40,  0},
	{MachineModel2B,		1,  41, 32},
	{MachineModel3B,		2, 133,  0},
	{MachineModel3APlus,		2, 133,  0},
	{MachineModel3BPlus,		2, 133,  0},
	//{MachineModelCM3,		0,   3,  2},	// cam1 only
	{MachineModel4B,		2, 133,  0},
	//{MachineModelCM4,		0, 133,  0},	// cam1 only

	{MachineModelUnknown,		0,   0,  0}
};

CCameraInfo::CCameraInfo (TMachineModel Model)
:	m_pMachineInfo (nullptr)
{
	const TMachineInfo *pInfo;
	for (pInfo = s_MachineInfo; pInfo->Model != MachineModelUnknown; pInfo++)
	{
		if (pInfo->Model == Model)
		{
			break;
		}
	}

	m_pMachineInfo = pInfo;
}

CCameraInfo::~CCameraInfo (void)
{
	m_pMachineInfo = nullptr;
}

bool CCameraInfo::IsSupported (void) const
{
	assert (m_pMachineInfo);
	return m_pMachineInfo->Model != MachineModelUnknown;
}

unsigned CCameraInfo::GetI2CDevice (void) const
{
	return 0;
}

unsigned CCameraInfo::GetI2CConfig (void) const
{
	assert (m_pMachineInfo);
	return m_pMachineInfo->I2CConfig;
}

unsigned CCameraInfo::GetPowerPin (void) const
{
	assert (m_pMachineInfo);
	return m_pMachineInfo->PowerPin;
}

unsigned CCameraInfo::GetLEDPin (void) const
{
	assert (m_pMachineInfo);
	return m_pMachineInfo->LEDPin;
}
