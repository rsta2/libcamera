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

#if DEPTH != 16
	#error Screen DEPTH must be 16!
#endif

LOGMODULE ("kernel");

CKernel::CKernel (void)
:	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_CameraManager (&m_Interrupt),
	m_pCamera (nullptr)
{
	m_ActLED.Blink (5);	// show we are alive
}

CKernel::~CKernel (void)
{
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
			pTarget = &m_Serial;
		}

		bOK = m_Logger.Initialize (pTarget);
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
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

	m_pCamera->SetControlValuePercent (CCameraDevice::ControlExposure, EXPOSURE);
	m_pCamera->SetControlValuePercent (CCameraDevice::ControlAnalogGain, ANALOG_GAIN);

	if (m_CameraManager.GetCameraModel () == CCameraManager::CameraModule1)
	{
		m_pCamera->SetControlValue (CCameraDevice::ControlAutoExposure, AUTO_EXPOSURE);
		m_pCamera->SetControlValue (CCameraDevice::ControlAutoGain, AUTO_GAIN);
		m_pCamera->SetControlValue (CCameraDevice::ControlAutoWhiteBalance, AUTO_WHITE_BALANCE);
	}
	else
	{
		m_pCamera->SetControlValuePercent (CCameraDevice::ControlDigitalGain, DIGITAL_GAIN);
	}

	// Start the image capture
	if (!m_pCamera->Start ())
	{
		LOGPANIC ("Cannot start streaming");
	}

	// Get and check the image format info
	m_FormatInfo = m_pCamera->GetFormatInfo ();

	// Use the image or display format, whatever is smaller
	unsigned nMinWidth =   m_FormatInfo.Width <= m_Screen.GetWidth ()
			     ? m_FormatInfo.Width : m_Screen.GetWidth ();
	unsigned nMinHeight =  m_FormatInfo.Height <= m_Screen.GetHeight ()
			     ? m_FormatInfo.Height : m_Screen.GetHeight ();

	// Clear both display areas
	m_Screen.ClearScreen (BLACK_COLOR);
	m_Screen.UpdateDisplay ();
	m_Screen.ClearScreen (BLACK_COLOR);

	unsigned nFrames = 0;
	unsigned nStartTicks = m_Timer.GetClockTicks ();

	while (m_Timer.GetUptime () < 60)	// Run for 1 minute
	{
		// Get the next buffer from the camera
		CCameraBuffer *pBuffer = m_pCamera->WaitForNextBuffer ();
		if (!pBuffer)
		{
			LOGPANIC ("Timeout while waiting for buffer");
		}

		// Ignore the first frames, because they may contain invalid data
		if (pBuffer->GetSequenceNumber () > 5)
		{
			// Convert and draw image
			for (unsigned y = 0; y < nMinHeight; y++)
			{
				for (unsigned x = 0; x < nMinWidth; x++)
				{
					m_Screen.DrawPixel (x, y, pBuffer->GetPixelRGB565 (x, y));
				}
			}
		}

		// Free the buffer to be reused
		m_pCamera->BufferProcessed ();

		nFrames++;

		// Swap display areas, waits for VSYNC
		m_Screen.UpdateDisplay ();

		// Temperature management for CPU
		m_CPUThrottle.Update ();
	}

	// Show the frame rate
	unsigned nEndTicks = m_Timer.GetClockTicks ();
	float fSeconds = (float) (nEndTicks - nStartTicks) / CLOCKHZ;
	LOGNOTE ("Frame rate was %.1f Hz", nFrames / fSeconds);

	// Stop the camera
	m_pCamera->Stop ();

	// Free the camera buffers
	m_pCamera->FreeBuffers ();

	// Clear the display
	m_Screen.ClearScreen (BLACK_COLOR);
	m_Screen.UpdateDisplay ();

	return ShutdownHalt;
}
