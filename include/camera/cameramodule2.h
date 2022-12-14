//
// cameramodule2.h
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#ifndef _camera_cameramodule2_h
#define _camera_cameramodule2_h

#include <camera/csi2cameradevice.h>
#include <camera/cameracontrol.h>
#include <circle/machineinfo.h>
#include <circle/i2cmaster.h>
#include <circle/gpiopin.h>
#include <circle/types.h>

class CCameraModule2 : public CCSI2CameraDevice	/// Driver for I2C Camera Module 2 (with IMX219)
{
public:
	enum TTestPattern		// must match IMX219_TEST_PATTERN_*
	{
		TestPatternDisable,
		TestPatternSolidColor,
		TestPatternColorBars,
		TestPatternGreyColorBars,
		TestPatternPN9,
		TestPatternUnknown
	};

public:
	CCameraModule2 (CInterruptSystem *pInterrupt);
	~CCameraModule2 (void);

	bool Initialize (void);

	bool Start (void);
	void Stop (void);

	int GetControlValue (TControl Control) const;
	bool SetControlValue (TControl Control, int nValue);
	bool SetControlValuePercent (TControl Control, unsigned nPercent);
	CCameraControl::TControlInfo GetControlInfo (TControl Control) const;

private:
	// Called from base class CCSI2CameraDevice
	bool SetMode (unsigned nWidth, unsigned nHeight, unsigned nDepth);
	TFormatCode GetPhysicalFormat (void) const;
	TFormatCode GetLogicalFormat (void) const;
	const TRect GetCropInfo (void) const;

private:
	bool SetupFormat (unsigned nDepth);
	void SetupControls (void);

	bool ReadReg (u16 usReg, unsigned nBytes, u16 *pValue);
	bool WriteReg (u16 usReg, unsigned nBytes, u16 usValue);

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
		unsigned	 VTSDef;	// default for vertical timing
		const TReg	*RegList;	// default register values
		unsigned	 RateFactor;	// relative pixel clock rate factor
	};

public:
	struct TMachineInfo
	{
		TMachineModel	Model;
		unsigned	I2CConfig;
		unsigned	PowerPin;
		unsigned	LEDPin;
	};

	static const TMachineInfo *GetMachineInfo (TMachineModel Model);

private:
	const TMachineInfo *m_pMachineInfo;

	CI2CMaster m_I2CMaster;
	CGPIOPin m_PowerGPIOPin;

	const TModeInfo *m_pMode;
	TFormatCode m_PhysicalFormat;
	TFormatCode m_LogicalFormat;

	CCameraControl m_Control[ControlUnknown];

	static const TFormatCode s_Formats[3][4];	// 8, 10 and 10P
	static const TModeInfo s_Modes[];

	static const TReg s_Regs3280x2464Mode[];
	static const TReg s_Regs1920x1080Mode[];
	static const TReg s_Regs1640x1232Mode[];
	static const TReg s_Regs640x480Mode[];
	static const TReg s_RegsRaw8Frame[];
	static const TReg s_RegsRaw10Frame[];

	static const TMachineInfo s_MachineInfo[];
};

#endif
