//
// csi2cameradevice.h
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#ifndef _camera_csi2cameradevice_h
#define _camera_csi2cameradevice_h

#include <camera/cameradevice.h>
#include <circle/interrupt.h>
#include <circle/gpioclock.h>
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include <circle/types.h>

class CCSI2CameraDevice : public CCameraDevice	/// Camera with CSI-2 interface
{
public:
	CCSI2CameraDevice (CInterruptSystem *pInterruptSystem);
	virtual ~CCSI2CameraDevice (void);

	bool Initialize (void);

	bool SetFormat (unsigned nWidth, unsigned nHeight, unsigned nDepth);

	TFormatInfo GetFormatInfo (void) const;

protected:
	bool EnableRX (void);
	void DisableRX (void);

	// implemented by I2C camera driver
	virtual bool SetMode (unsigned nWidth, unsigned nHeight, unsigned nDepth) = 0;

	virtual TFormatCode GetPhysicalFormat (void) const = 0;
	virtual TFormatCode GetLogicalFormat (void) const = 0;
	virtual const TRect GetCropInfo (void) const = 0;

private:
	void InterruptHandler (void);
	static void InterruptStub (void *pParam);

	bool SetPower (bool bOn);

	void ClockWrite (u32 nValue);

	u32 ReadReg (u32 nOffset)
	{
		return read32 (ARM_CSI1_BASE + nOffset);
	}

	void WriteReg (u32 nOffset, u32 nValue)
	{
		write32 (ARM_CSI1_BASE + nOffset, nValue);
	}

	void WriteRegField (u32 nOffset, u32 nValue, u32 nMask);

	static void SetField (u32 *pValue, u32 nValue, u32 nMask);

private:
	CInterruptSystem *m_pInterruptSystem;
	bool m_bIRQConnected;

	bool m_bActive;

	CGPIOClock m_CAM1Clock;

	unsigned m_nWidth;
	unsigned m_nHeight;
	unsigned m_nBytesPerLine;
	size_t m_nImageSize;

	unsigned m_nSequence;

	CCameraBuffer *m_pCurrentBuffer;
	u8 *m_pDummyBuffer;
};

#endif
