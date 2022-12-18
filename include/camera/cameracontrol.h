//
// cameracontrol.h
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#ifndef _camera_cameracontrol_h
#define _camera_cameracontrol_h

class CCameraControl
{
public:
	struct TControlInfo
	{
		bool Supported;
		int Min;
		int Max;
		int Step;
		int Default;
	};

public:
	CCameraControl (void);
	CCameraControl (int nMin, int nMax, int nStep, int nDefault);

	void Setup (int nMin, int nMax, int nStep, int nDefault);

	int GetValue (void) const;
	bool SetValue (int nValue);

	TControlInfo GetInfo (void) const;

private:
	TControlInfo m_Info;
	int m_nValue;
};

#endif
