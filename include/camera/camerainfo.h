//
// camerainfo.h
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#ifndef _camera_camerainfo_h
#define _camera_camerainfo_h

#include <circle/machineinfo.h>
#include <circle/types.h>

class CCameraInfo
{
public:
	CCameraInfo (TMachineModel Model);
	~CCameraInfo (void);

	bool IsSupported (void) const;

	unsigned GetI2CDevice (void) const;
	unsigned GetI2CConfig (void) const;

	unsigned GetPowerPin (void) const;
	unsigned GetLEDPin (void) const;

private:
	struct TMachineInfo
	{
		TMachineModel	Model;
		unsigned	I2CConfig;
		unsigned	PowerPin;
		unsigned	LEDPin;
	};

private:
	const TMachineInfo *m_pMachineInfo;

	static const TMachineInfo s_MachineInfo[];
};

#endif
