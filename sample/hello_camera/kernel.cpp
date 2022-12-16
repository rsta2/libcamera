//
// kernel.cpp
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#include "kernel.h"
#include <camera/camerabuffer.h>

//#define WIDTH		640
//#define HEIGHT	480

#define WIDTH		1640
#define HEIGHT		1232

//#define WIDTH		1920
//#define HEIGHT	1080

//#define WIDTH		3280
//#define HEIGHT	2464

#define CAM_DEPTH	10	// TODO: depth 8 does not work

#define VFLIP		false
#define HFLIP		false

#define EXPOSURE	40	// percent
#define ANALOG_GAIN	60	// percent
#define DIGITAL_GAIN	60	// percent

#define TEST_PATTERN	CCameraModule2::TestPatternDisable
//#define TEST_PATTERN	CCameraModule2::TestPatternSolidColor
//#define TEST_PATTERN	CCameraModule2::TestPatternColorBars

#if CAM_DEPTH == 8
	typedef u8 TCameraColor;
#elif CAM_DEPTH == 10
	typedef u16 TCameraColor;
#else
	#error CAM_DEPTH must be 8 or 10!
#endif

#if DEPTH != 16
	#error Screen DEPTH must be 16!
#endif

LOGMODULE ("kernel");

CKernel::CKernel (void)
:	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Camera (&m_Interrupt)
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
		bOK = m_Camera.Initialize ();
	}

	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	LOGNOTE ("Compile time: " __DATE__ " " __TIME__);

	// Set the wanted image format first
	if (!m_Camera.SetFormat (WIDTH, HEIGHT, CAM_DEPTH))
	{
		LOGPANIC ("Cannot set format");
	}

	// Then allocate the image buffers
	if (!m_Camera.AllocateBuffers ())
	{
		LOGPANIC ("Cannot allocate buffers");
	}

	// Set the camera control values
	m_Camera.SetControlValue (CCameraDevice::ControlVFlip, VFLIP);
	m_Camera.SetControlValue (CCameraDevice::ControlHFlip, HFLIP);

	m_Camera.SetControlValuePercent (CCameraDevice::ControlExposure, EXPOSURE);
	m_Camera.SetControlValuePercent (CCameraDevice::ControlAnalogGain, ANALOG_GAIN);
	m_Camera.SetControlValuePercent (CCameraDevice::ControlDigitalGain, DIGITAL_GAIN);

	m_Camera.SetControlValue (CCameraDevice::ControlTestPattern, TEST_PATTERN);
	// solid white
	m_Camera.SetControlValuePercent (CCameraDevice::ControlTestPatternRed, 100);
	m_Camera.SetControlValuePercent (CCameraDevice::ControlTestPatternGreenR, 100);
	m_Camera.SetControlValuePercent (CCameraDevice::ControlTestPatternGreenB, 100);
	m_Camera.SetControlValuePercent (CCameraDevice::ControlTestPatternBlue, 100);

	// Start the image capture
	if (!m_Camera.Start ())
	{
		LOGPANIC ("Cannot start streaming");
	}

	// Get and check the image format info
	m_FormatInfo = m_Camera.GetFormatInfo ();
	assert (m_FormatInfo.Width == WIDTH);
	assert (m_FormatInfo.Height == HEIGHT);

	// Use the image or display format, whatever is smaller
	unsigned nMinWidth = WIDTH <= m_Screen.GetWidth () ? WIDTH : m_Screen.GetWidth ();
	unsigned nMinHeight = HEIGHT <= m_Screen.GetHeight () ? HEIGHT : m_Screen.GetHeight ();

	// Clear both display areas
	m_Screen.ClearScreen (BLACK_COLOR);
	m_Screen.UpdateDisplay ();
	m_Screen.ClearScreen (BLACK_COLOR);

	unsigned nFrames = 0;
	unsigned nStartTicks = m_Timer.GetClockTicks ();

	while (m_Timer.GetUptime () < 60)	// Run for 1 minute
	{
		// Get the next buffer from the camera
		CCameraBuffer *pBuffer = m_Camera.GetNextBuffer ();
		if (!pBuffer)
		{
			continue;		// No buffer available yet
		}

		// Convert and draw image
		for (unsigned y = 0; y < nMinHeight; y++)
		{
			for (unsigned x = 0; x < nMinWidth; x++)
			{
				m_Screen.DrawPixel (x, y, pBuffer->GetPixelRGB565 (x, y));
			}
		}

		// Free the buffer to be reused
		m_Camera.BufferProcessed ();

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
	m_Camera.Stop ();

	// Free the camera buffers
	m_Camera.FreeBuffers ();

	// Clear the display
	m_Screen.ClearScreen (BLACK_COLOR);
	m_Screen.UpdateDisplay ();

	return ShutdownHalt;
}
