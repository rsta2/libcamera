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
	/// \brief API: Describes a camera control
	struct TControlInfo
	{
		bool Supported;		///< Is it supported by this camera?
		int Min;		///< Minimum value
		int Max;		///< Maximum value
		int Step;		///< Value must be a multiple of this (currently always 1)
		int Default;		///< Default value
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
