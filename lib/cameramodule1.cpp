//
// cameramodule1.cpp
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// Based on the Linux driver:
//	drivers/media/i2c/ov5647.c
//	by Ramiro Oliveira <roliveir@synopsys.com>
//
// SPDX-License-Identifier: GPL-2.0
/*
 * A V4L2 driver for OmniVision OV5647 cameras.
 *
 * Based on Samsung S5K6AAFX SXGA 1/6" 1.3M CMOS Image Sensor driver
 * Copyright (C) 2011 Sylwester Nawrocki <s.nawrocki@samsung.com>
 *
 * Based on Omnivision OV7670 Camera Driver
 * Copyright (C) 2006-7 Jonathan Corbet <corbet@lwn.net>
 *
 * Copyright (C) 2016, Synopsys, Inc.
 */
#include <camera/cameramodule1.h>
#include <circle/bcmpropertytags.h>
#include <circle/devicenameservice.h>
#include <circle/machineinfo.h>
#include <circle/logger.h>
#include <circle/timer.h>
#include <circle/macros.h>
#include <circle/util.h>
#include <assert.h>
#include "math.h"

#define OV5647_I2C_SLAVE_ADDRESS	0x36

/*
 * From the datasheet, "20ms after PWDN goes low or 20ms after RESETB goes
 * high if reset is inserted after PWDN goes high, host can access sensor's
 * SCCB to initialize sensor."
 */
#define PWDN_ACTIVE_DELAY_MS		20

#define MIPI_CTRL00_CLOCK_LANE_GATE	BIT(5)
#define MIPI_CTRL00_LINE_SYNC_ENABLE	BIT(4)
#define MIPI_CTRL00_BUS_IDLE		BIT(2)
#define MIPI_CTRL00_CLOCK_LANE_DISABLE	BIT(0)

#define OV5647_SW_STANDBY		0x0100
#define OV5647_SW_RESET			0x0103
#define OV5647_REG_CHIPID_H		0x300a
#define OV5647_REG_CHIPID_L		0x300b
#define OV5640_REG_PAD_OUT		0x300d
#define OV5647_REG_EXP_HI		0x3500
#define OV5647_REG_EXP_MID		0x3501
#define OV5647_REG_EXP_LO		0x3502
#define OV5647_REG_AEC_AGC		0x3503
#define OV5647_REG_GAIN_HI		0x350a
#define OV5647_REG_GAIN_LO		0x350b
#define OV5647_REG_VTS_HI		0x380e
#define OV5647_REG_VTS_LO		0x380f
#define OV5647_REG_VFLIP		0x3820
#define OV5647_REG_HFLIP		0x3821
#define OV5647_REG_FRAME_OFF_NUMBER	0x4202
#define OV5647_REG_MIPI_CTRL00		0x4800
#define OV5647_REG_MIPI_CTRL14		0x4814
#define OV5647_REG_AWB			0x5001

/* OV5647 native and active pixel array size */
#define OV5647_NATIVE_WIDTH		2624U
#define OV5647_NATIVE_HEIGHT		1956U

#define OV5647_PIXEL_ARRAY_LEFT		16U
#define OV5647_PIXEL_ARRAY_TOP		6U
#define OV5647_PIXEL_ARRAY_WIDTH	2592U
#define OV5647_PIXEL_ARRAY_HEIGHT	1944U

#define OV5647_VBLANK_MIN		24
#define OV5647_VTS_MAX			32767

#define OV5647_EXPOSURE_MIN		4
#define OV5647_EXPOSURE_STEP		1
#define OV5647_EXPOSURE_DEFAULT		1000
#define OV5647_EXPOSURE_MAX		65535

LOGMODULE ("camera1");

static const char DeviceName[] = "cam1";

CCameraModule1::CCameraModule1 (CInterruptSystem *pInterrupt)
:	CCSI2CameraDevice (pInterrupt),
	m_CameraInfo (CMachineInfo::Get ()->GetMachineModel ()),
	m_I2CMaster (m_CameraInfo.GetI2CDevice (), true, m_CameraInfo.GetI2CConfig ()),
	m_bPoweredOn (false),
	m_pMode (nullptr),
	m_PhysicalFormat (FormatUnknown),
	m_LogicalFormat (FormatUnknown),
	m_bIgnoreErrors (false)
{
}

CCameraModule1::~CCameraModule1 (void)
{
	CDeviceNameService::Get ()->RemoveDevice (DeviceName, FALSE);

	if (m_bPoweredOn)
	{
		m_bIgnoreErrors = true;

		u8 uchBuffer;
		if (   !WriteRegs (s_RegsSensorDisable)
		    || !ReadReg8 (OV5647_SW_STANDBY, &uchBuffer)
		    || !WriteReg8 (OV5647_SW_STANDBY, uchBuffer & ~0x01))
		{
			//LOGWARN ("Cannot enter standby");
		}

		unsigned nPowerPin = m_CameraInfo.GetPowerPin ();
		if (nPowerPin < GPIO_PINS)
		{
			m_PowerGPIOPin.Write (LOW);
			m_PowerGPIOPin.SetMode (GPIOModeInput);
		}
		else
		{
			CBcmPropertyTags Tags;
			TPropertyTagGPIOState GPIOState;
			GPIOState.nGPIO = nPowerPin;
			GPIOState.nState = 0;
			Tags.GetTag (PROPTAG_SET_SET_GPIO_STATE, &GPIOState, sizeof GPIOState, 8);
		}
	}
}

bool CCameraModule1::Probe (void)
{
	m_bIgnoreErrors = true;

	bool bOK = Initialize ();

	m_bIgnoreErrors = false;

	return bOK;
}

bool CCameraModule1::Initialize (void)
{
	if (!m_CameraInfo.IsSupported ())
	{
		LOGERR ("Raspberry Pi model not supported");

		return false;
	}

	if (!CCSI2CameraDevice::Initialize ())
	{
		return false;
	}

	if (!m_I2CMaster.Initialize ())
	{
		LOGERR ("Cannot init I2C master");

		return false;
	}

	unsigned nPowerPin = m_CameraInfo.GetPowerPin ();
	if (nPowerPin < GPIO_PINS)
	{
		m_PowerGPIOPin.AssignPin (nPowerPin);
		m_PowerGPIOPin.SetMode (GPIOModeOutput, false);
		m_PowerGPIOPin.Write (HIGH);
	}
	else
	{
		CBcmPropertyTags Tags;
		TPropertyTagGPIOState GPIOState;
		GPIOState.nGPIO = nPowerPin;
		GPIOState.nState = 1;
		if (!Tags.GetTag (PROPTAG_SET_SET_GPIO_STATE, &GPIOState, sizeof GPIOState, 8))
		{
			LOGERR ("Cannot enable power");

			return false;
		}
	}

	m_bPoweredOn = true;

	CTimer::Get ()->MsDelay (PWDN_ACTIVE_DELAY_MS);

	if (   !WriteRegs (s_RegsSensorEnable)
	    || !WriteReg8 (OV5647_REG_MIPI_CTRL00,   MIPI_CTRL00_CLOCK_LANE_GATE
						   | MIPI_CTRL00_BUS_IDLE
						   | MIPI_CTRL00_CLOCK_LANE_DISABLE)
	    || !WriteReg8 (OV5647_REG_FRAME_OFF_NUMBER, 0x0f)
	    || !WriteReg8 (OV5640_REG_PAD_OUT, 0x01))
	{
		if (!m_bIgnoreErrors)
		{
			LOGERR ("Camera not available, check power");
		}

		return false;
	}

	u8 uchBuffer;
	if (   !WriteReg8 (OV5647_SW_RESET, 0x01)
	    || !ReadReg8 (OV5647_REG_CHIPID_H, &uchBuffer)
	    || uchBuffer != 0x56
	    || !ReadReg8 (OV5647_REG_CHIPID_L, &uchBuffer)
	    || uchBuffer != 0x47
	    || !WriteReg8 (OV5647_SW_RESET, 0x00))
	{
		LOGERR ("Cannot detect camera");

		return false;
	}

	CDeviceNameService::Get ()->AddDevice (DeviceName, this, FALSE);

	LOGNOTE ("Camera Module 1 initialized");

	return true;
}

bool CCameraModule1::Start (void)
{
	assert (m_pMode);

	// set mode
	u8 uchBuffer;
	if (   !ReadReg8 (OV5647_SW_STANDBY, &uchBuffer)
	    || !WriteRegs (m_pMode->RegList))
	{
		LOGWARN ("Cannot write sensor defaults");

		return false;
	}

	// set virtual channel 0
	u8 uchChannelID;
	if (   !ReadReg8 (OV5647_REG_MIPI_CTRL14, &uchChannelID)
	    || !WriteReg8 (OV5647_REG_MIPI_CTRL14, (uchChannelID & ~(3 << 6)) | (0 << 6)))
	{
		LOGWARN ("Cannot set virtual channel");

		return false;
	}

	u8 uchResetValue;
	if (!ReadReg8 (OV5647_SW_STANDBY, &uchResetValue))
	{
		LOGWARN ("Cannot read standby register");

		return false;
	}
	if (   !(uchResetValue & 0x01)
	    && !WriteReg8 (OV5647_SW_STANDBY, 0x01))
	{
		LOGWARN ("Cannot leave standby");

		return false;
	}

	for (unsigned i = 0; i < ControlUnknown; i++)
	{
		TControl Control = static_cast <TControl> (i);
		if (!IsControlSupported (Control))
		{
			continue;
		}

		int nValue = m_Control[Control].GetValue ();

		if (!SetControlValue (Control, nValue))
		{
			LOGWARN ("Cannot apply control value (%u, %d)", i, nValue);

			return false;
		}
	}

	if (!EnableRX ())
	{
		LOGWARN ("Cannot enable CSI receiver");

		return false;
	}

	if (   !WriteReg8 (OV5647_REG_MIPI_CTRL00,   MIPI_CTRL00_BUS_IDLE
						   | MIPI_CTRL00_CLOCK_LANE_GATE
						   | MIPI_CTRL00_LINE_SYNC_ENABLE)
	    || !WriteReg8 (OV5647_REG_FRAME_OFF_NUMBER, 0x00)
	    || !WriteReg8 (OV5640_REG_PAD_OUT, 0x00))
	{
		LOGWARN ("Cannot enter streaming mode");

		return false;
	}

	LOGDBG ("Streaming started (%ux%u, %s)",
		m_pMode->Width, m_pMode->Height,
		(const char *) FormatToString (m_LogicalFormat));

	return true;
}

void CCameraModule1::Stop (void)
{
	if (   !WriteReg8 (OV5647_REG_MIPI_CTRL00,   MIPI_CTRL00_CLOCK_LANE_GATE
						   | MIPI_CTRL00_BUS_IDLE
						   | MIPI_CTRL00_CLOCK_LANE_DISABLE)
	    || !WriteReg8 (OV5647_REG_FRAME_OFF_NUMBER, 0x0f)
	    || !WriteReg8 (OV5640_REG_PAD_OUT, 0x01))
	{
		LOGWARN ("Cannot stop streaming mode");
	}

	DisableRX ();

	LOGDBG ("Streaming stopped");
}

bool CCameraModule1::SetMode (unsigned *pWidth, unsigned *pHeight, unsigned nDepth)
{
	assert (pWidth);
	assert (pHeight);

	// find best fitting mode
	const TModeInfo *pBestMode = nullptr;
	u32 nMinError = 0xFFFFFFFFU;
	for (const TModeInfo *pMode = s_Modes; pMode->Width; pMode++)
	{
		u32 nError = abs (*pWidth - pMode->Width) + abs (*pHeight - pMode->Height);
		if (nError < nMinError)
		{
			pBestMode = pMode;
			nMinError = nError;
		}
	}

	m_pMode = pBestMode;
	assert (m_pMode);

	*pWidth = m_pMode->Width;
	*pHeight = m_pMode->Height;

	SetupControls ();

	if (!SetupFormat (nDepth))
	{
		m_pMode = nullptr;

		return false;
	}

	return true;
}

CCameraDevice::TFormatCode CCameraModule1::GetPhysicalFormat (void) const
{
	assert (m_PhysicalFormat != FormatUnknown);
	return m_PhysicalFormat;
}

CCameraDevice::TFormatCode CCameraModule1::GetLogicalFormat (void) const
{
	assert (m_LogicalFormat != FormatUnknown);
	return m_LogicalFormat;
}

const CCameraDevice::TRect CCameraModule1::GetCropInfo (void) const
{
	assert (m_pMode);
	return m_pMode->Crop;
}

bool CCameraModule1::SetupFormat (unsigned nDepth)
{
	unsigned nIndex =   (m_Control[ControlVFlip].GetValue () ? 2 : 0)
			  | (m_Control[ControlHFlip].GetValue () ? 1 : 0);

	if (nDepth == 10)
	{
		m_PhysicalFormat = s_Formats[1][nIndex];
		m_LogicalFormat = s_Formats[0][nIndex];
	}
	else
	{
		LOGWARN ("Depth not supported (%u)", nDepth);

		return false;
	}

	return true;
}

void CCameraModule1::SetupControls (void)
{
	assert (m_pMode);

	m_Control[ControlAutoGain].Setup (false, true, 1, false);
	m_Control[ControlAutoWhiteBalance].Setup (false, true, 1, false);
	m_Control[ControlAutoExposure].Setup (false, true, 1, false);

	int nExposureMax = m_pMode->VTS - 4;
	m_Control[ControlExposure].Setup (OV5647_EXPOSURE_MIN, nExposureMax, OV5647_EXPOSURE_STEP,
					    nExposureMax < OV5647_EXPOSURE_DEFAULT
					  ? nExposureMax : OV5647_EXPOSURE_DEFAULT);

	// min: 16 = 1.0x; max (10 bits); default: 32 = 2.0x
	m_Control[ControlAnalogGain].Setup (16, 1023, 1, 32);

	m_Control[ControlVBlank].Setup (OV5647_VBLANK_MIN, OV5647_VTS_MAX - m_pMode->Height, 1,
					m_pMode->VTS - m_pMode->Height);

	m_Control[ControlVFlip].Setup (false, true, 1, false);
	m_Control[ControlHFlip].Setup (false, true, 1, false);
}

bool CCameraModule1::IsControlSupported (TControl Control) const
{
	assert (Control < ControlUnknown);
	CCameraControl::TControlInfo Info = m_Control[Control].GetInfo ();

	return Info.Supported;
}

int CCameraModule1::GetControlValue (TControl Control) const
{
	assert (Control < ControlUnknown);
	return m_Control[Control].GetValue ();
}

bool CCameraModule1::SetControlValue (TControl Control, int nValue)
{
	assert (Control < ControlUnknown);
	assert (m_pMode);

	if (Control == ControlVBlank)
	{
		// Update max exposure while meeting expected vblanking
		CCameraControl::TControlInfo ExposureInfo = m_Control[ControlExposure].GetInfo ();

		int nExposureMax = m_pMode->Height + nValue - 4;
		m_Control[ControlExposure].Setup (ExposureInfo.Min, nExposureMax, ExposureInfo.Step,
						    nExposureMax < OV5647_EXPOSURE_DEFAULT
						  ? nExposureMax : OV5647_EXPOSURE_DEFAULT);
	}

	if (!m_Control[Control].SetValue (nValue))
	{
		return false;
	}

	u8 uchBuffer;
	bool bOK = false;
	switch (Control)
	{
	case ControlAutoWhiteBalance:
		bOK = WriteReg8 (OV5647_REG_AWB, nValue ? 1 : 0);
		break;

	case ControlAutoGain:
		// Non-zero turns on AGC by clearing bit 1.
		bOK =    ReadReg8 (OV5647_REG_AEC_AGC, &uchBuffer)
		      && WriteReg8 (OV5647_REG_AEC_AGC,   nValue
						        ? uchBuffer & ~BIT (1) : uchBuffer | BIT (1));
		break;

	case ControlAutoExposure:
		// Turn on/off AEC by clearing/setting bit 0.
		bOK =    ReadReg8 (OV5647_REG_AEC_AGC, &uchBuffer)
		      && WriteReg8 (OV5647_REG_AEC_AGC,   nValue
						        ? uchBuffer & ~BIT (0) : uchBuffer | BIT (0));
		break;

	case ControlAnalogGain:
		// 10 bits of gain, 2 in the high register.
		bOK =    WriteReg8 (OV5647_REG_GAIN_HI, (nValue >> 8) & 3)
		      && WriteReg8 (OV5647_REG_GAIN_LO, nValue & 0xff);
		break;

	case ControlExposure:
		// Sensor has 20 bits, but the bottom 4 bits are fractions of a line,
		// which we leave as zero (and don't receive in nValue).
		bOK =    WriteReg8 (OV5647_REG_EXP_HI, (nValue >> 12) & 0xf)
		      && WriteReg8 (OV5647_REG_EXP_MID, (nValue >> 4) & 0xff)
		      && WriteReg8 (OV5647_REG_EXP_LO, (nValue & 0xf) << 4);
		break;

	case ControlVBlank:
		bOK = WriteReg16 (OV5647_REG_VTS_HI, m_pMode->Height + nValue);
	 	break;

	case ControlHFlip:
		// There's an in-built hflip in the sensor, so account for that here.
		// Set or clear bit 1 and leave everything else alone.
		bOK =    ReadReg8 (OV5647_REG_HFLIP, &uchBuffer)
		      && WriteReg8 (OV5647_REG_HFLIP, nValue ? uchBuffer & ~2 : uchBuffer | 2);
		if (bOK)
		{
			bOK = SetupFormat (GetFormatDepth (m_LogicalFormat));
		}
		break;

	case ControlVFlip:
		// Set or clear bit 1 and leave everything else alone.
		bOK =    ReadReg8 (OV5647_REG_VFLIP, &uchBuffer)
		      && WriteReg8 (OV5647_REG_VFLIP, nValue ? uchBuffer | 2 : uchBuffer & ~2 );
		if (bOK)
		{
			bOK = SetupFormat (GetFormatDepth (m_LogicalFormat));
		}
		break;

	default:
		break;
	}

	return bOK;
}

bool CCameraModule1::SetControlValuePercent (TControl Control, unsigned nPercent)
{
	assert (Control < ControlUnknown);
	CCameraControl::TControlInfo Info = m_Control[Control].GetInfo ();

	assert (nPercent <= 100);
	return SetControlValue (Control, Info.Min + (Info.Max - Info.Min) * nPercent / 100);
}

CCameraControl::TControlInfo CCameraModule1::GetControlInfo (TControl Control) const
{
	assert (Control < ControlUnknown);
	return m_Control[Control].GetInfo ();
}

bool CCameraModule1::ReadReg8 (u16 usReg, u8 *pValue)
{
	usReg = le2be16 (usReg);
	int nResult = m_I2CMaster.Write (OV5647_I2C_SLAVE_ADDRESS, &usReg, sizeof usReg);
	if (nResult != sizeof usReg)
	{
		if (!m_bIgnoreErrors)
		{
			LOGWARN ("I2C write failed (%d)", nResult);
		}

		return false;
	}

	assert (pValue);
	nResult = m_I2CMaster.Read (OV5647_I2C_SLAVE_ADDRESS, pValue, 1);
	if (nResult != 1)
	{
		if (!m_bIgnoreErrors)
		{
			LOGWARN ("I2C read failed (%d)", nResult);
		}

		return false;
	}

	return true;
}

bool CCameraModule1::WriteReg8 (u16 usReg, u8 uchValue)
{
	u8 Buffer[3] = {(u8) (usReg >> 8), (u8) (usReg & 0xFF), uchValue};

	int nResult = m_I2CMaster.Write (OV5647_I2C_SLAVE_ADDRESS, Buffer, sizeof Buffer);
	if (nResult != sizeof Buffer)
	{
		if (!m_bIgnoreErrors)
		{
			LOGWARN ("I2C write failed (%d)", nResult);
		}

		return false;
	}

	return true;
}

bool CCameraModule1::WriteReg16 (u16 usReg, u16 usValue)
{
	u8 Buffer[4] = {(u8) (usReg >> 8), (u8) (usReg & 0xFF),
			(u8) (usValue >> 8), (u8) (usValue & 0xFF)};

	int nResult = m_I2CMaster.Write (OV5647_I2C_SLAVE_ADDRESS, Buffer, sizeof Buffer);
	if (nResult != sizeof Buffer)
	{
		if (!m_bIgnoreErrors)
		{
			LOGWARN ("I2C write failed (%d)", nResult);
		}

		return false;
	}

	return true;
}

bool CCameraModule1::WriteRegs (const TReg *pRegs)
{
	assert (pRegs);

	for (; pRegs->Reg; pRegs++)
	{
		if (!WriteReg8 (pRegs->Reg, pRegs->Value))
		{
			return false;
		}
	}

	return true;
}

// The supported formats.
// This table MUST contain 4 entries per format, to cover the various flip
// combinations in the order
// - no flip
// - h flip
// - v flip
// - h&v flips
const CCameraDevice::TFormatCode CCameraModule1::s_Formats[][4] =
{
	{
		// logical only
		FormatSGBRG10,
		FormatSBGGR10,
		FormatSRGGB10,
		FormatSGRBG10,
	}, {
		// physical only
		FormatSGBRG10P,
		FormatSBGGR10P,
		FormatSRGGB10P,
		FormatSGRBG10P,
	}
};

// Mode configs
const CCameraModule1::TModeInfo CCameraModule1::s_Modes[] =
{
	// 2592x1944 full resolution full FOV 10-bit mode.
	{
		.Width		= 2592,
		.Height		= 1944,
		.Crop = {
			.Left		= OV5647_PIXEL_ARRAY_LEFT,
			.Top		= OV5647_PIXEL_ARRAY_TOP,
			.Width		= 2592,
			.Height		= 1944
		},
		.HTS		= 2844,
		.VTS		= 0x7b0,
		.RegList	= s_Regs2592x1944Mode
	},
	// 1080p30 10-bit mode. Full resolution centre-cropped down to 1080p.
	{
		.Width		= 1920,
		.Height		= 1080,
		.Crop = {
			.Left		= 348 + OV5647_PIXEL_ARRAY_LEFT,
			.Top		= 434 + OV5647_PIXEL_ARRAY_TOP,
			.Width		= 1928,
			.Height		= 1080
		},
		.HTS		= 2416,
		.VTS		= 0x450,
		.RegList	= s_Regs1920x1080Mode
	},
	// 2x2 binned full FOV 10-bit mode.
	{
		.Width		= 1296,
		.Height		= 972,
		.Crop = {
			.Left		= OV5647_PIXEL_ARRAY_LEFT,
			.Top		= OV5647_PIXEL_ARRAY_TOP,
			.Width		= 2592,
			.Height		= 1944
		},
		.HTS		= 1896,
		.VTS		= 0x59b,
		.RegList	= s_Regs1296x972Mode
	},
	// 10-bit VGA full FOV 60fps. 2x2 binned and subsampled down to VGA.
	{
		.Width		= 640,
		.Height		= 480,
		.Crop = {
			.Left		= 16 + OV5647_PIXEL_ARRAY_LEFT,
			.Top		= OV5647_PIXEL_ARRAY_TOP,
			.Width		= 2560,
			.Height		= 1920
		},
		.HTS		= 1852,
		.VTS		= 0x1f8,
		.RegList	= s_Regs640x480Mode
	}, {
		.Width  = 0,
		.Height = 0
	}
};

const CCameraModule1::TReg CCameraModule1::s_Regs2592x1944Mode[] =
{
	{0x0100, 0x00},
	{0x0103, 0x01},
	{0x3034, 0x1a},
	{0x3035, 0x21},
	{0x3036, 0x69},
	{0x303c, 0x11},
	{0x3106, 0xf5},
	{0x3821, 0x00},
	{0x3820, 0x00},
	{0x3827, 0xec},
	{0x370c, 0x03},
	{0x3612, 0x5b},
	{0x3618, 0x04},
	{0x5000, 0x06},
	{0x5002, 0x41},
	{0x5003, 0x08},
	{0x5a00, 0x08},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3016, 0x08},
	{0x3017, 0xe0},
	{0x3018, 0x44},
	{0x301c, 0xf8},
	{0x301d, 0xf0},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3c01, 0x80},
	{0x3b07, 0x0c},
	{0x380c, 0x0b},
	{0x380d, 0x1c},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3708, 0x64},
	{0x3709, 0x12},
	{0x3808, 0x0a},
	{0x3809, 0x20},
	{0x380a, 0x07},
	{0x380b, 0x98},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x07},
	{0x3807, 0xa3},
	{0x3811, 0x10},
	{0x3813, 0x06},
	{0x3630, 0x2e},
	{0x3632, 0xe2},
	{0x3633, 0x23},
	{0x3634, 0x44},
	{0x3636, 0x06},
	{0x3620, 0x64},
	{0x3621, 0xe0},
	{0x3600, 0x37},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x3731, 0x02},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3f05, 0x02},
	{0x3f06, 0x10},
	{0x3f01, 0x0a},
	{0x3a08, 0x01},
	{0x3a09, 0x28},
	{0x3a0a, 0x00},
	{0x3a0b, 0xf6},
	{0x3a0d, 0x08},
	{0x3a0e, 0x06},
	{0x3a0f, 0x58},
	{0x3a10, 0x50},
	{0x3a1b, 0x58},
	{0x3a1e, 0x50},
	{0x3a11, 0x60},
	{0x3a1f, 0x28},
	{0x4001, 0x02},
	{0x4004, 0x04},
	{0x4000, 0x09},
	{0x4837, 0x19},
	{0x4800, 0x24},
	{0x3503, 0x03},
	{0x0100, 0x01},
	{0}
};

const CCameraModule1::TReg CCameraModule1::s_Regs1920x1080Mode[] =
{
	{0x0100, 0x00},
	{0x0103, 0x01},
	{0x3034, 0x1a},
	{0x3035, 0x21},
	{0x3036, 0x62},
	{0x303c, 0x11},
	{0x3106, 0xf5},
	{0x3821, 0x00},
	{0x3820, 0x00},
	{0x3827, 0xec},
	{0x370c, 0x03},
	{0x3612, 0x5b},
	{0x3618, 0x04},
	{0x5000, 0x06},
	{0x5002, 0x41},
	{0x5003, 0x08},
	{0x5a00, 0x08},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3016, 0x08},
	{0x3017, 0xe0},
	{0x3018, 0x44},
	{0x301c, 0xf8},
	{0x301d, 0xf0},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3c01, 0x80},
	{0x3b07, 0x0c},
	{0x380c, 0x09},
	{0x380d, 0x70},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3708, 0x64},
	{0x3709, 0x12},
	{0x3808, 0x07},
	{0x3809, 0x80},
	{0x380a, 0x04},
	{0x380b, 0x38},
	{0x3800, 0x01},
	{0x3801, 0x5c},
	{0x3802, 0x01},
	{0x3803, 0xb2},
	{0x3804, 0x08},
	{0x3805, 0xe3},
	{0x3806, 0x05},
	{0x3807, 0xf1},
	{0x3811, 0x04},
	{0x3813, 0x02},
	{0x3630, 0x2e},
	{0x3632, 0xe2},
	{0x3633, 0x23},
	{0x3634, 0x44},
	{0x3636, 0x06},
	{0x3620, 0x64},
	{0x3621, 0xe0},
	{0x3600, 0x37},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x3731, 0x02},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3f05, 0x02},
	{0x3f06, 0x10},
	{0x3f01, 0x0a},
	{0x3a08, 0x01},
	{0x3a09, 0x4b},
	{0x3a0a, 0x01},
	{0x3a0b, 0x13},
	{0x3a0d, 0x04},
	{0x3a0e, 0x03},
	{0x3a0f, 0x58},
	{0x3a10, 0x50},
	{0x3a1b, 0x58},
	{0x3a1e, 0x50},
	{0x3a11, 0x60},
	{0x3a1f, 0x28},
	{0x4001, 0x02},
	{0x4004, 0x04},
	{0x4000, 0x09},
	{0x4837, 0x19},
	{0x4800, 0x34},
	{0x3503, 0x03},
	{0x0100, 0x01},
	{0}
};

const CCameraModule1::TReg CCameraModule1::s_Regs1296x972Mode[] =
{
	{0x0100, 0x00},
	{0x0103, 0x01},
	{0x3034, 0x1a},
	{0x3035, 0x21},
	{0x3036, 0x62},
	{0x303c, 0x11},
	{0x3106, 0xf5},
	{0x3827, 0xec},
	{0x370c, 0x03},
	{0x3612, 0x59},
	{0x3618, 0x00},
	{0x5000, 0x06},
	{0x5002, 0x41},
	{0x5003, 0x08},
	{0x5a00, 0x08},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3016, 0x08},
	{0x3017, 0xe0},
	{0x3018, 0x44},
	{0x301c, 0xf8},
	{0x301d, 0xf0},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3c01, 0x80},
	{0x3b07, 0x0c},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x07},
	{0x3807, 0xa3},
	{0x3808, 0x05},
	{0x3809, 0x10},
	{0x380a, 0x03},
	{0x380b, 0xcc},
	{0x380c, 0x07},
	{0x380d, 0x68},
	{0x3811, 0x0c},
	{0x3813, 0x06},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3630, 0x2e},
	{0x3632, 0xe2},
	{0x3633, 0x23},
	{0x3634, 0x44},
	{0x3636, 0x06},
	{0x3620, 0x64},
	{0x3621, 0xe0},
	{0x3600, 0x37},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x3731, 0x02},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3f05, 0x02},
	{0x3f06, 0x10},
	{0x3f01, 0x0a},
	{0x3a08, 0x01},
	{0x3a09, 0x28},
	{0x3a0a, 0x00},
	{0x3a0b, 0xf6},
	{0x3a0d, 0x08},
	{0x3a0e, 0x06},
	{0x3a0f, 0x58},
	{0x3a10, 0x50},
	{0x3a1b, 0x58},
	{0x3a1e, 0x50},
	{0x3a11, 0x60},
	{0x3a1f, 0x28},
	{0x4001, 0x02},
	{0x4004, 0x04},
	{0x4000, 0x09},
	{0x4837, 0x16},
	{0x4800, 0x24},
	{0x3503, 0x03},
	{0x3820, 0x41},
	{0x3821, 0x01},
	{0x350a, 0x00},
	{0x350b, 0x10},
	{0x3500, 0x00},
	{0x3501, 0x1a},
	{0x3502, 0xf0},
	{0x3212, 0xa0},
	{0x0100, 0x01},
	{0}
};

const CCameraModule1::TReg CCameraModule1::s_Regs640x480Mode[] =
{
	{0x0100, 0x00},
	{0x0103, 0x01},
	{0x3035, 0x11},
	{0x3036, 0x46},
	{0x303c, 0x11},
	{0x3821, 0x01},
	{0x3820, 0x41},
	{0x370c, 0x03},
	{0x3612, 0x59},
	{0x3618, 0x00},
	{0x5000, 0x06},
	{0x5003, 0x08},
	{0x5a00, 0x08},
	{0x3000, 0xff},
	{0x3001, 0xff},
	{0x3002, 0xff},
	{0x301d, 0xf0},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3c01, 0x80},
	{0x3b07, 0x0c},
	{0x380c, 0x07},
	{0x380d, 0x3c},
	{0x3814, 0x35},
	{0x3815, 0x35},
	{0x3708, 0x64},
	{0x3709, 0x52},
	{0x3808, 0x02},
	{0x3809, 0x80},
	{0x380a, 0x01},
	{0x380b, 0xe0},
	{0x3800, 0x00},
	{0x3801, 0x10},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0a},
	{0x3805, 0x2f},
	{0x3806, 0x07},
	{0x3807, 0x9f},
	{0x3630, 0x2e},
	{0x3632, 0xe2},
	{0x3633, 0x23},
	{0x3634, 0x44},
	{0x3620, 0x64},
	{0x3621, 0xe0},
	{0x3600, 0x37},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x3731, 0x02},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3f05, 0x02},
	{0x3f06, 0x10},
	{0x3f01, 0x0a},
	{0x3a08, 0x01},
	{0x3a09, 0x2e},
	{0x3a0a, 0x00},
	{0x3a0b, 0xfb},
	{0x3a0d, 0x02},
	{0x3a0e, 0x01},
	{0x3a0f, 0x58},
	{0x3a10, 0x50},
	{0x3a1b, 0x58},
	{0x3a1e, 0x50},
	{0x3a11, 0x60},
	{0x3a1f, 0x28},
	{0x4001, 0x02},
	{0x4004, 0x02},
	{0x4000, 0x09},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3017, 0xe0},
	{0x301c, 0xfc},
	{0x3636, 0x06},
	{0x3016, 0x08},
	{0x3827, 0xec},
	{0x3018, 0x44},
	{0x3035, 0x21},
	{0x3106, 0xf5},
	{0x3034, 0x1a},
	{0x301c, 0xf8},
	{0x4800, 0x34},
	{0x3503, 0x03},
	{0x0100, 0x01},
	{0}
};

const CCameraModule1::TReg CCameraModule1::s_RegsSensorEnable[] =
{
	{0x3000, 0x0f},
	{0x3001, 0xff},
	{0x3002, 0xe4},
	{0}
};

const CCameraModule1::TReg CCameraModule1::s_RegsSensorDisable[] =
{
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0}
};
