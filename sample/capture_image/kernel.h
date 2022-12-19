//
// kernel.h
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#ifndef _kernel_h
#define _kernel_h

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/types.h>
#include <SDCard/emmc.h>
#include <fatfs/ff.h>
#include <camera/cameramanager.h>

enum TShutdownMode
{
	ShutdownNone,
	ShutdownHalt,
	ShutdownReboot
};

class CKernel
{
public:
	CKernel (void);
	~CKernel (void);

	boolean Initialize (void);

	TShutdownMode Run (void);

private:
	void ControlUpDown (int nUpDown);
	static void KeyPressedHandler (const char *pString);

	enum TAction
	{
		ActionUpdateControl,
		ActionCaptureImage,
		ActionReboot,
		ActionNone
	};

private:
	// do not change this order
	CActLED			m_ActLED;
	CKernelOptions		m_Options;
	CDeviceNameService	m_DeviceNameService;
	CExceptionHandler	m_ExceptionHandler;
	CInterruptSystem	m_Interrupt;
	CScreenDevice		m_Screen;
	CSerialDevice		m_Serial;
	CTimer			m_Timer;
	CLogger			m_Logger;
	CUSBHCIDevice		m_USBHCI;
	CEMMCDevice		m_EMMC;
	FATFS			m_FileSystem;

	CCameraManager		m_CameraManager;
	CCameraDevice		*m_pCamera;
	CCameraDevice::TFormatInfo m_FormatInfo;

	unsigned		m_nExposure;
	unsigned 		m_nAnalogGain;
	unsigned 		m_nDigitalGain;

	CUSBKeyboardDevice	*m_pKeyboard;

	volatile TAction	m_Action;
	volatile CCameraDevice::TControl m_SelectedControl;

	volatile bool m_bWhiteBalance;

	static CKernel *s_pThis;
};

#endif
