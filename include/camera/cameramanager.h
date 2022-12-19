//
// cameramanager.h
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#ifndef _camera_cameramanager_h
#define _camera_cameramanager_h

#include <camera/cameradevice.h>
#include <camera/camerainfo.h>
#include <circle/interrupt.h>
#include <circle/types.h>

class CCameraManager		/// API: Auto-probes for an available camera
{
public:
	enum TCameraModel
	{
		CameraModule1,
		CameraModule2,
		CameraModelUnknown
	};

public:
	CCameraManager (CInterruptSystem *pInterrupt);
	~CCameraManager (void);

	/// \brief Auto-probe for a camera and initialize it
	/// \return Camera found and ready for operation?
	bool Initialize (void);

	/// \return Which camera model is in use?
	TCameraModel GetCameraModel (void) const;

	/// \return Pointer to the camera instance
	CCameraDevice *GetCamera (void) const;

private:
	CInterruptSystem *m_pInterrupt;
	CCameraInfo m_CameraInfo;

	TCameraModel m_Model;
	CCameraDevice *m_pCamera;
};

#endif
