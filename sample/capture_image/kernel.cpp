//
// kernel.cpp
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#include "kernel.h"
#include "../config.h"
#include <camera/camerabuffer.h>
#include <circle/string.h>
#include <circle/util.h>

#define DRIVE		"SD:"

#if DEPTH != 16
	#error Screen DEPTH must be 16!
#endif

LOGMODULE ("kernel");

CKernel *CKernel::s_pThis = nullptr;

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_USBHCI (&m_Interrupt, &m_Timer),
	m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
	m_CameraManager (&m_Interrupt),
	m_pCamera (nullptr),
	m_nExposure (EXPOSURE),
	m_nAnalogGain (ANALOG_GAIN),
	m_nDigitalGain (DIGITAL_GAIN),
	m_pKeyboard (nullptr),
	m_Action (ActionNone),
	m_SelectedControl (CCameraDevice::ControlUnknown),
	m_bWhiteBalance (false)
{
	s_pThis = this;

	m_ActLED.Blink (5);	// show we are alive
}

CKernel::~CKernel (void)
{
	s_pThis = nullptr;
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

	if (bOK)
	{
		bOK = m_Interrupt.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Screen.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Serial.Initialize (115200);
	}

	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Screen;
		}

		bOK = m_Logger.Initialize (pTarget);
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
	}

	if (bOK)
	{
		bOK = m_USBHCI.Initialize ();
	}

	if (bOK)
	{
		bOK = m_EMMC.Initialize ();
	}

	if (bOK)
	{
		bOK = m_CameraManager.Initialize ();
	}

	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	LOGNOTE ("Compile time: " __DATE__ " " __TIME__);

	m_pCamera = m_CameraManager.GetCamera ();
	assert (m_pCamera);

	// Get USB keyboard
	m_pKeyboard = static_cast <CUSBKeyboardDevice *> (
		m_DeviceNameService.GetDevice ("ukbd1", FALSE));
	if (!m_pKeyboard)
	{
		LOGPANIC ("USB keyboard not found");
	}

	m_pKeyboard->RegisterKeyPressedHandler (KeyPressedHandler);

	// Mount file system
	if (f_mount (&m_FileSystem, DRIVE, 1) != FR_OK)
	{
		LOGPANIC ("Cannot mount drive: %s", DRIVE);
	}

	// Set the wanted image format first
	if (!m_pCamera->SetFormat (WIDTH, HEIGHT))
	{
		LOGPANIC ("Cannot set format");
	}

	// Then allocate the image buffers
	if (!m_pCamera->AllocateBuffers ())
	{
		LOGPANIC ("Cannot allocate buffers");
	}

	// Set the camera control values
	m_pCamera->SetControlValue (CCameraDevice::ControlVFlip, VFLIP);
	m_pCamera->SetControlValue (CCameraDevice::ControlHFlip, HFLIP);

	m_pCamera->SetControlValuePercent (CCameraDevice::ControlExposure, m_nExposure);
	m_pCamera->SetControlValuePercent (CCameraDevice::ControlAnalogGain, m_nAnalogGain);

	if (m_CameraManager.GetCameraModel () == CCameraManager::CameraModule1)
	{
		m_pCamera->SetControlValue (CCameraDevice::ControlAutoExposure, AUTO_EXPOSURE);
		m_pCamera->SetControlValue (CCameraDevice::ControlAutoGain, AUTO_GAIN);
		m_pCamera->SetControlValue (CCameraDevice::ControlAutoWhiteBalance, AUTO_WHITE_BALANCE);
	}
	else
	{
		m_pCamera->SetControlValuePercent (CCameraDevice::ControlDigitalGain, m_nDigitalGain);
	}

	// Get and check the image format info
	m_FormatInfo = m_pCamera->GetFormatInfo ();

	TScreenColor *pRGBBuffer = new TScreenColor[m_FormatInfo.Width * m_FormatInfo.Height];
	if (!pRGBBuffer)
	{
		LOGPANIC ("Cannot allocate RGB buffer");
	}

	// Start the image capture
	if (!m_pCamera->Start ())
	{
		LOGPANIC ("Cannot start streaming");
	}

	bool bContinue = true;
	while (bContinue)
	{
		// Wait for available buffer from camera
		CCameraBuffer *pBuffer;
		do
		{
			pBuffer = m_pCamera->GetNextBuffer ();
		}
		while (!pBuffer);

		// Optionally apply white balancing
		if (m_bWhiteBalance)
		{
			pBuffer->WhiteBalance ();
		}

		// Convert and draw preview image
		static const unsigned nSizeFactor = 4;
		unsigned x0 = m_Screen.GetWidth () - m_FormatInfo.Width / nSizeFactor;
		for (unsigned y = 0; y < m_FormatInfo.Height; y += nSizeFactor)
		{
			for (unsigned x = 0; x < m_FormatInfo.Width; x += nSizeFactor)
			{
				m_Screen.SetPixel (x0 + x / nSizeFactor, y / nSizeFactor,
						   pBuffer->GetPixelRGB565 (x, y));
			}
		}

		m_pCamera->BufferProcessed ();

		switch (m_Action)
		{
		case ActionUpdateControl: {
			unsigned *pValue = nullptr;
			const char *pControl = nullptr;
			switch (m_SelectedControl)
			{
			case CCameraDevice::ControlExposure:
				pValue = &m_nExposure;
				pControl = "exposure";
				break;

			case CCameraDevice::ControlAnalogGain:
				pValue = &m_nAnalogGain;
				pControl = "analog gain";
				break;

			case CCameraDevice::ControlDigitalGain:
				pValue = &m_nDigitalGain;
				pControl = "digital gain";
				break;

			default:
				assert (0);
				break;
			}

			LOGNOTE ("Set %s to %u%%", pControl, *pValue);

			if (!m_pCamera->SetControlValuePercent (m_SelectedControl, *pValue))
			{
				LOGWARN ("Cannot set control value", pControl);
			}

			m_Action = ActionNone;
			} break;

		case ActionCaptureImage: {
			LOGNOTE ("Capture image");

			// Return all buffers
			while (m_pCamera->GetNextBuffer ())
			{
				m_pCamera->BufferProcessed ();
			}

			// Get six buffers from the camera and take the last one
			for (unsigned i = 0; i < 6; i++)
			{
				do
				{
					pBuffer = m_pCamera->GetNextBuffer ();
				}
				while (!pBuffer);

				if (i < 5)
				{
					m_pCamera->BufferProcessed ();
				}
			}

			if (m_bWhiteBalance)
			{
				LOGNOTE ("Apply white balancing");

				pBuffer->WhiteBalance ();
			}

			// Convert image to RGB565
			pBuffer->ConvertToRGB565 (pRGBBuffer);

			// Return all buffers
			while (m_pCamera->GetNextBuffer ())
			{
				m_pCamera->BufferProcessed ();
			}

			// Find unused file name and save image
			for (unsigned i = 1; i < 100; i++)
			{
				CString Filename;
				Filename.Format ("%s/image-%ux%u-rgb565-%02u.data",
						 DRIVE, m_FormatInfo.Width, m_FormatInfo.Height, i);

				FIL File;
				if (f_open (&File, Filename, FA_WRITE | FA_CREATE_NEW) == FR_OK)
				{
					LOGNOTE ("Saving image to file: %s", (const char *) Filename);

					unsigned nBytesWritten;
					if (f_write (&File, pRGBBuffer,   m_FormatInfo.Width
									* m_FormatInfo.Height
									* sizeof (TScreenColor),
						     &nBytesWritten) != FR_OK)
					{
						LOGWARN ("Write error");
					}

					f_close (&File);

					break;
				}
			}

			LOGNOTE ("Image successfully saved");

			m_Action = ActionNone;
			} break;

		case ActionReboot:
			bContinue = false;
			m_Action = ActionNone;
			break;

		default:
			break;
		}
	}

	// Stop the camera
	m_pCamera->Stop ();

	delete [] pRGBBuffer;

	// Free the camera buffers
	m_pCamera->FreeBuffers ();

	// Unmount file system
	f_mount (0, DRIVE, 0);

	LOGNOTE ("Rebooting ...");

	m_Timer.MsDelay (500);

	return ShutdownReboot;
}

void CKernel::ControlUpDown (int nUpDown)
{
	unsigned *pValue = nullptr;
	switch (m_SelectedControl)
	{
	case CCameraDevice::ControlExposure:
		pValue = &m_nExposure;
		break;

	case CCameraDevice::ControlAnalogGain:
		pValue = &m_nAnalogGain;
		break;

	case CCameraDevice::ControlDigitalGain:
		pValue = &m_nDigitalGain;
		break;

	default:
		return;
	}

	assert (pValue);
	int nValue = (int) *pValue;
	nValue += nUpDown;

	if (0 <= nValue && nValue <= 100)
	{
		*pValue = (unsigned) nValue;

		m_Action = ActionUpdateControl;
	}
}

void CKernel::KeyPressedHandler (const char *pString)
{
	assert (s_pThis);

	if (   strcmp (pString, " ") == 0		// Space
	    || strcmp (pString, "\n") == 0		// Enter
	    || strcmp (pString, "c") == 0)
	{
		s_pThis->m_Action = ActionCaptureImage;
	}
	else if (strcmp (pString, "e") == 0)
	{
		s_pThis->m_SelectedControl = CCameraDevice::ControlExposure;
	}
	else if (strcmp (pString, "a") == 0)
	{
		s_pThis->m_SelectedControl = CCameraDevice::ControlAnalogGain;
	}
	else if (   strcmp (pString, "d") == 0
		 && s_pThis->m_CameraManager.GetCameraModel () == CCameraManager::CameraModule2)
	{
		s_pThis->m_SelectedControl = CCameraDevice::ControlDigitalGain;
	}
	else if (   strcmp (pString, "+") == 0
		 || strcmp (pString, "\x1B[A") == 0)	// Up
	{
		s_pThis->ControlUpDown (10);
	}
	else if (   strcmp (pString, "-") == 0
		 || strcmp (pString, "\x1B[B") == 0)	// Down
	{
		s_pThis->ControlUpDown (-10);
	}
	else if (strcmp (pString, "w") == 0)
	{
		s_pThis->m_bWhiteBalance = !s_pThis->m_bWhiteBalance;
	}
	else if (   strcmp (pString, "q") == 0
		 || strcmp (pString, "x") == 0)
	{
		s_pThis->m_Action = ActionReboot;
	}
}
