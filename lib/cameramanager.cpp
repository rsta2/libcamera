//
// cameramanager.cpp
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#include <camera/cameramanager.h>
#include <camera/cameramodule1.h>
#include <camera/cameramodule2.h>
#include <circle/machineinfo.h>
#include <circle/logger.h>
#include <assert.h>

//#define CAMMAN_DEBUG

LOGMODULE ("camman");

CCameraManager::CCameraManager (CInterruptSystem *pInterrupt)
:	m_pInterrupt (pInterrupt),
	m_CameraInfo (CMachineInfo::Get ()->GetMachineModel ()),
	m_Model (CameraModelUnknown),
	m_pCamera (nullptr)
{
}

CCameraManager::~CCameraManager (void)
{
	delete m_pCamera;
	m_pCamera = nullptr;

	m_pInterrupt = nullptr;
}

bool CCameraManager::Initialize (void)
{
	if (!m_CameraInfo.IsSupported ())
	{
		LOGERR ("Raspberry Pi model not supported");

		return false;
	}

	assert (!m_pCamera);
	m_pCamera = new CCameraModule1 (m_pInterrupt);
#ifndef CAMMAN_DEBUG
	if (m_pCamera->Probe ())
#else
	if (m_pCamera->Initialize ())
#endif
	{
		m_Model = CameraModule1;

		return true;
	}
	delete m_pCamera;

	m_pCamera = new CCameraModule2 (m_pInterrupt);
#ifndef CAMMAN_DEBUG
	if (m_pCamera->Probe ())
#else
	if (m_pCamera->Initialize ())
#endif
	{
		m_Model = CameraModule2;

		return true;
	}
	delete m_pCamera;

	LOGERR ("No camera found");

	return false;
}

CCameraManager::TCameraModel CCameraManager::GetCameraModel (void) const
{
	return m_Model;
}

CCameraDevice *CCameraManager::GetCamera (void) const
{
	return m_pCamera;
}
