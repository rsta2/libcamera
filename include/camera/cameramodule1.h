//
// cameramodule1.h
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#ifndef _camera_cameramodule1_h
#define _camera_cameramodule1_h

#include <camera/csi2cameradevice.h>
#include <camera/cameracontrol.h>
#include <camera/camerainfo.h>
#include <circle/i2cmaster.h>
#include <circle/gpiopin.h>
#include <circle/types.h>

class CCameraModule1 : public CCSI2CameraDevice	/// Driver for I2C Camera Module 1 (with OV5647)
{
public:
	CCameraModule1 (CInterruptSystem *pInterrupt);
	~CCameraModule1 (void);

	bool Probe (void);
	bool Initialize (void);

	bool Start (void);
	void Stop (void);

	bool IsControlSupported (TControl Control) const;
	int GetControlValue (TControl Control) const;
	bool SetControlValue (TControl Control, int nValue);
	bool SetControlValuePercent (TControl Control, unsigned nPercent);
	CCameraControl::TControlInfo GetControlInfo (TControl Control) const;

private:
	// Called from base class CCSI2CameraDevice
	bool SetMode (unsigned *pWidth, unsigned *pHeight, unsigned nDepth);
	TFormatCode GetPhysicalFormat (void) const;
	TFormatCode GetLogicalFormat (void) const;
	const TRect GetCropInfo (void) const;

private:
	bool SetupFormat (unsigned nDepth);
	void SetupControls (void);

	bool ReadReg8 (u16 usReg, u8 *pValue);
	bool WriteReg8 (u16 usReg, u8 uchValue);
	bool WriteReg16 (u16 usReg, u16 usValue);

	struct TReg
	{
		u16	Reg;
		u8	Value;
	};

	bool WriteRegs (const TReg *pRegs);

	struct TModeInfo
	{
		unsigned	 Width;		// frame width
		unsigned	 Height;	// frame height
		TRect	 	 Crop;		// analog crop rectangle
		unsigned	 HTS;		// horizontal timing
		unsigned	 VTS;		// vertical timing
		const TReg	*RegList;	// default register values
	};

private:
	CCameraInfo m_CameraInfo;
	CI2CMaster m_I2CMaster;
	CGPIOPin m_PowerGPIOPin;

	bool m_bPoweredOn;

	const TModeInfo *m_pMode;
	TFormatCode m_PhysicalFormat;
	TFormatCode m_LogicalFormat;

	CCameraControl m_Control[ControlUnknown];

	bool m_bIgnoreErrors;

	static const TFormatCode s_Formats[2][4];	// 10 and 10P
	static const TModeInfo s_Modes[];

	static const TReg s_Regs2592x1944Mode[];
	static const TReg s_Regs1920x1080Mode[];
	static const TReg s_Regs1296x972Mode[];
	static const TReg s_Regs640x480Mode[];
	static const TReg s_RegsSensorEnable[];
	static const TReg s_RegsSensorDisable[];
};

#endif
