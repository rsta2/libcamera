//
// cameramodule2.cpp
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// Based on the Linux driver:
//	drivers/media/i2c/imx219.c
//	by Dave Stevenson <dave.stevenson@raspberrypi.com
//
// SPDX-License-Identifier: GPL-2.0
/*
 * A V4L2 driver for Sony IMX219 cameras.
 * Copyright (C) 2019, Raspberry Pi (Trading) Ltd
 *
 * Based on Sony imx258 camera driver
 * Copyright (C) 2018 Intel Corporation
 *
 * DT / fwnode changes, and regulator / GPIO control taken from imx214 driver
 * Copyright 2018 Qtechnology A/S
 *
 * Flip handling taken from the Sony IMX319 driver.
 * Copyright (C) 2018 Intel Corporation
 */
#include <camera/cameramodule2.h>
#include <circle/bcmpropertytags.h>
#include <circle/machineinfo.h>
#include <circle/logger.h>
#include <circle/timer.h>
#include <circle/util.h>
#include <assert.h>

#define IMX219_I2C_SLAVE_ADDRESS	0x10

#define IMX219_REG_MODE_SELECT		0x0100
#define IMX219_MODE_STANDBY		0x00
#define IMX219_MODE_STREAMING		0x01

/* Chip ID */
#define IMX219_REG_CHIP_ID		0x0000
#define IMX219_CHIP_ID			0x0219

/* External clock frequency is 24.0M */
#define IMX219_XCLK_FREQ		24000000

/* Pixel rate is fixed at 182.4M for all the modes */
#define IMX219_PIXEL_RATE		182400000

#define IMX219_DEFAULT_LINK_FREQ	456000000

/* V_TIMING internal */
#define IMX219_REG_VTS			0x0160
#define IMX219_VTS_15FPS		0x0dc6
#define IMX219_VTS_30FPS_1080P		0x06e3
#define IMX219_VTS_30FPS_BINNED		0x06e3
#define IMX219_VTS_30FPS_640x480	0x06e3
#define IMX219_VTS_MAX			0xffff

#define IMX219_VBLANK_MIN		32

/*Frame Length Line*/
#define IMX219_FLL_MIN			0x08a6
#define IMX219_FLL_MAX			0xffff
#define IMX219_FLL_STEP			1
#define IMX219_FLL_DEFAULT		0x0c98

/* HBLANK control range */
#define IMX219_PPL_MIN			3448
#define IMX219_PPL_MAX			0x7ff0
#define IMX219_REG_HTS			0x0162

/* Exposure control */
#define IMX219_REG_EXPOSURE		0x015a
#define IMX219_EXPOSURE_MIN		4
#define IMX219_EXPOSURE_STEP		1
#define IMX219_EXPOSURE_DEFAULT		0x640
#define IMX219_EXPOSURE_MAX		65535

/* Analog gain control */
#define IMX219_REG_ANALOG_GAIN		0x0157
#define IMX219_ANA_GAIN_MIN		0
#define IMX219_ANA_GAIN_MAX		232
#define IMX219_ANA_GAIN_STEP		1
#define IMX219_ANA_GAIN_DEFAULT		0x0

/* Digital gain control */
#define IMX219_REG_DIGITAL_GAIN		0x0158
#define IMX219_DGTL_GAIN_MIN		0x0100
#define IMX219_DGTL_GAIN_MAX		0x0fff
#define IMX219_DGTL_GAIN_DEFAULT	0x0100
#define IMX219_DGTL_GAIN_STEP		1

#define IMX219_REG_ORIENTATION		0x0172

/* Test Pattern Control */
#define IMX219_REG_TEST_PATTERN		0x0600
#define IMX219_TEST_PATTERN_DISABLE	0
#define IMX219_TEST_PATTERN_SOLID_COLOR	1
#define IMX219_TEST_PATTERN_COLOR_BARS	2
#define IMX219_TEST_PATTERN_GREY_COLOR	3
#define IMX219_TEST_PATTERN_PN9		4

/* Test pattern colour components */
#define IMX219_REG_TESTP_RED		0x0602
#define IMX219_REG_TESTP_GREENR		0x0604
#define IMX219_REG_TESTP_BLUE		0x0606
#define IMX219_REG_TESTP_GREENB		0x0608
#define IMX219_TESTP_COLOUR_MIN		0
#define IMX219_TESTP_COLOUR_MAX		0x03ff
#define IMX219_TESTP_COLOUR_STEP	1
#define IMX219_TESTP_RED_DEFAULT	IMX219_TESTP_COLOUR_MAX
#define IMX219_TESTP_GREENR_DEFAULT	0
#define IMX219_TESTP_BLUE_DEFAULT	0
#define IMX219_TESTP_GREENB_DEFAULT	0

/* IMX219 native and active pixel array size. */
#define IMX219_NATIVE_WIDTH		3296U
#define IMX219_NATIVE_HEIGHT		2480U
#define IMX219_PIXEL_ARRAY_LEFT		8U
#define IMX219_PIXEL_ARRAY_TOP		8U
#define IMX219_PIXEL_ARRAY_WIDTH	3280U
#define IMX219_PIXEL_ARRAY_HEIGHT	2464U

LOGMODULE ("camera2");

CCameraModule2::CCameraModule2 (CInterruptSystem *pInterrupt)
:	CCSI2CameraDevice (pInterrupt),
	m_CameraInfo (CMachineInfo::Get ()->GetMachineModel ()),
	m_I2CMaster (m_CameraInfo.GetI2CDevice (), true, m_CameraInfo.GetI2CConfig ()),
	m_pMode (nullptr),
	m_PhysicalFormat (FormatUnknown),
	m_LogicalFormat (FormatUnknown)
{
}

CCameraModule2::~CCameraModule2 (void)
{
	// TODO: Set power GPIO pin to LOW
}

bool CCameraModule2::Initialize (void)
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

	CTimer::Get ()->usDelay (6200);

	u16 usChipID;
	if (   !ReadReg (IMX219_REG_CHIP_ID, 2, &usChipID)
	    || usChipID != IMX219_CHIP_ID)
	{
		LOGERR ("Invalid chip ID");

		return false;
	}

	// cannot go to standby directly
	if (!WriteReg (IMX219_REG_MODE_SELECT, 1, IMX219_MODE_STREAMING))
	{
		LOGERR ("Cannot select streaming mode");

		return false;
	}

	CTimer::Get ()->usDelay (100);

	if (!WriteReg (IMX219_REG_MODE_SELECT, 1, IMX219_MODE_STANDBY))
	{
		LOGERR ("Cannot select standby mode");

		return false;
	}

	CTimer::Get ()->usDelay (100);

	LOGNOTE ("Camera Module 2 initialized");

	return true;
}

bool CCameraModule2::Start (void)
{
	assert (m_pMode);
	if (!WriteRegs (m_pMode->RegList))
	{
		LOGWARN ("Cannot init mode");

		return false;
	}

	assert (m_PhysicalFormat != FormatUnknown);
	if (!WriteRegs (  GetFormatDepth (m_PhysicalFormat) == 10
			? s_RegsRaw10Frame : s_RegsRaw8Frame))
	{
		LOGWARN ("Cannot set frame format");

		return false;
	}

	for (unsigned i = 0; i < ControlUnknown; i++)
	{
		TControl Control = static_cast <TControl> (i);
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

	if (!WriteReg (IMX219_REG_MODE_SELECT, 1, IMX219_MODE_STREAMING))
	{
		LOGWARN ("Cannot select streaming mode");

		return false;
	}

	LOGDBG ("Streaming started (%ux%u, %s)",
		m_pMode->Width, m_pMode->Height,
		(const char *) FormatToString (m_LogicalFormat));

	return true;
}

void CCameraModule2::Stop (void)
{
	if (!WriteReg (IMX219_REG_MODE_SELECT, 1, IMX219_MODE_STANDBY))
	{
		LOGWARN ("Cannot stop streaming mode");
	}

	DisableRX ();

	LOGDBG ("Streaming stopped");
}

bool CCameraModule2::SetMode (unsigned nWidth, unsigned nHeight, unsigned nDepth)
{
	const TModeInfo *pMode;
	for (pMode = s_Modes; pMode->Width; pMode++)
	{
		if (   pMode->Width == nWidth
		    && pMode->Height == nHeight)
		{
			break;
		}
	}

	if (!pMode->Width)
	{
		LOGWARN ("Resolution not supported (%ux%u)", nWidth, nHeight);

		return false;
	}

	m_pMode = pMode;

	SetupControls ();

	if (!SetupFormat (nDepth))
	{
		m_pMode = nullptr;

		return false;
	}

	return true;
}

CCameraDevice::TFormatCode CCameraModule2::GetPhysicalFormat (void) const
{
	assert (m_PhysicalFormat != FormatUnknown);
	return m_PhysicalFormat;
}

CCameraDevice::TFormatCode CCameraModule2::GetLogicalFormat (void) const
{
	assert (m_LogicalFormat != FormatUnknown);
	return m_LogicalFormat;
}

const CCameraDevice::TRect CCameraModule2::GetCropInfo (void) const
{
	assert (m_pMode);
	return m_pMode->Crop;
}

bool CCameraModule2::SetupFormat (unsigned nDepth)
{
	unsigned nIndex =   (m_Control[ControlVFlip].GetValue () ? 2 : 0)
			  | (m_Control[ControlHFlip].GetValue () ? 1 : 0);

	if (nDepth == 8)
	{
		m_PhysicalFormat = s_Formats[0][nIndex];
		m_LogicalFormat = m_PhysicalFormat;
	}
	else if (nDepth == 10)
	{
		m_PhysicalFormat = s_Formats[2][nIndex];
		m_LogicalFormat = s_Formats[1][nIndex];
	}
	else
	{
		LOGWARN ("Depth not supported (%u)", nDepth);

		return false;
	}

	return true;
}

void CCameraModule2::SetupControls (void)
{
	assert (m_pMode);

	// Initial vblank / hblank / exposure parameters based on current mode
	m_Control[ControlVBlank].Setup (IMX219_VBLANK_MIN, IMX219_VTS_MAX - m_pMode->Height, 1,
					m_pMode->VTSDef - m_pMode->Height);

	int nHBlank = IMX219_PPL_MIN - m_pMode->Width;
	m_Control[ControlHBlank].Setup (nHBlank, IMX219_PPL_MAX - m_pMode->Width, 1, nHBlank);

	int nExposureMax = m_pMode->VTSDef - 4;
	m_Control[ControlExposure].Setup (IMX219_EXPOSURE_MIN, nExposureMax, IMX219_EXPOSURE_STEP,
					    nExposureMax < IMX219_EXPOSURE_DEFAULT
					  ? nExposureMax : IMX219_EXPOSURE_DEFAULT);

	m_Control[ControlAnalogGain].Setup (IMX219_ANA_GAIN_MIN, IMX219_ANA_GAIN_MAX,
					    IMX219_ANA_GAIN_STEP, IMX219_ANA_GAIN_DEFAULT);
	m_Control[ControlDigitalGain].Setup (IMX219_DGTL_GAIN_MIN, IMX219_DGTL_GAIN_MAX,
					     IMX219_DGTL_GAIN_STEP, IMX219_DGTL_GAIN_DEFAULT);

	m_Control[ControlVFlip].Setup (false, true, 1, false);
	m_Control[ControlHFlip].Setup (false, true, 1, false);

	m_Control[ControlTestPattern].Setup (IMX219_TEST_PATTERN_DISABLE, IMX219_TEST_PATTERN_PN9,
					     1, IMX219_TEST_PATTERN_DISABLE);
	m_Control[ControlTestPatternRed].Setup (IMX219_TESTP_COLOUR_MIN,
						IMX219_TESTP_COLOUR_MAX,
						IMX219_TESTP_COLOUR_STEP,
						IMX219_TESTP_RED_DEFAULT);
	m_Control[ControlTestPatternGreenR].Setup (IMX219_TESTP_COLOUR_MIN,
						   IMX219_TESTP_COLOUR_MAX,
						   IMX219_TESTP_COLOUR_STEP,
						   IMX219_TESTP_GREENR_DEFAULT);
	m_Control[ControlTestPatternGreenB].Setup (IMX219_TESTP_COLOUR_MIN,
						   IMX219_TESTP_COLOUR_MAX,
						   IMX219_TESTP_COLOUR_STEP,
						   IMX219_TESTP_GREENB_DEFAULT);
	m_Control[ControlTestPatternBlue].Setup (IMX219_TESTP_COLOUR_MIN,
						 IMX219_TESTP_COLOUR_MAX,
						 IMX219_TESTP_COLOUR_STEP,
						 IMX219_TESTP_BLUE_DEFAULT);
}

int CCameraModule2::GetControlValue (TControl Control) const
{
	assert (Control < ControlUnknown);
	return m_Control[Control].GetValue ();
}

bool CCameraModule2::SetControlValue (TControl Control, int nValue)
{
	assert (Control < ControlUnknown);
	assert (m_pMode);

	if (Control == ControlVBlank)
	{
		// Update max exposure while meeting expected vblanking
		CCameraControl::TControlInfo ExposureInfo = m_Control[ControlExposure].GetInfo ();

		int nExposureMax = m_pMode->Height + nValue - 4;
		m_Control[ControlExposure].Setup (ExposureInfo.Min, nExposureMax, ExposureInfo.Step,
						    nExposureMax < IMX219_EXPOSURE_DEFAULT
						  ? nExposureMax : IMX219_EXPOSURE_DEFAULT);
	}

	if (!m_Control[Control].SetValue (nValue))
	{
		return false;
	}

	assert (0 <= nValue && nValue <= 0xFFFF);

	bool bOK = false;
	switch (Control)
	{
	case ControlVBlank:
		bOK = WriteReg (IMX219_REG_VTS, 2, (m_pMode->Height + nValue) / m_pMode->RateFactor);
	 	break;

	case ControlHBlank:
		bOK = WriteReg (IMX219_REG_HTS, 2, m_pMode->Width + nValue);
		break;

	case ControlVFlip:
	case ControlHFlip:
		bOK = WriteReg (IMX219_REG_ORIENTATION, 1,   m_Control[ControlVFlip].GetValue () << 1
							   | m_Control[ControlHFlip].GetValue ());
		if (bOK)
		{
			bOK = SetupFormat (GetFormatDepth (m_LogicalFormat));
		}
		break;

	case ControlExposure:
		bOK = WriteReg (IMX219_REG_EXPOSURE, 2, nValue / m_pMode->RateFactor);
		break;

	case ControlAnalogGain:
		assert (nValue <= 0xFF);
		bOK = WriteReg (IMX219_REG_ANALOG_GAIN, 1, nValue);
		break;

	case ControlDigitalGain:
		bOK = WriteReg (IMX219_REG_DIGITAL_GAIN, 2, nValue);
		break;

	case ControlTestPattern:
		bOK = WriteReg (IMX219_REG_TEST_PATTERN, 2, nValue);
		break;

	case ControlTestPatternRed:
		bOK = WriteReg (IMX219_REG_TESTP_RED, 2, nValue);
		break;

	case ControlTestPatternGreenR:
		bOK = WriteReg (IMX219_REG_TESTP_GREENR, 2, nValue);
		break;

	case ControlTestPatternGreenB:
		bOK = WriteReg (IMX219_REG_TESTP_GREENB, 2, nValue);
		break;

	case ControlTestPatternBlue:
		bOK = WriteReg (IMX219_REG_TESTP_BLUE, 2, nValue);
		break;

	default:
		assert (0);
		break;
	}

	return bOK;
}

bool CCameraModule2::SetControlValuePercent (TControl Control, unsigned nPercent)
{
	assert (Control < ControlUnknown);
	CCameraControl::TControlInfo Info = m_Control[Control].GetInfo ();

	assert (nPercent <= 100);
	return SetControlValue (Control, Info.Min + (Info.Max - Info.Min) * nPercent / 100);
}

CCameraControl::TControlInfo CCameraModule2::GetControlInfo (TControl Control) const
{
	assert (Control < ControlUnknown);
	return m_Control[Control].GetInfo ();
}

bool CCameraModule2::ReadReg (u16 usReg, unsigned nBytes, u16 *pValue)
{
	usReg = le2be16 (usReg);
	int nResult = m_I2CMaster.Write (IMX219_I2C_SLAVE_ADDRESS, &usReg, sizeof usReg);
	if (nResult != sizeof usReg)
	{
		LOGWARN ("I2C write failed (%d)", nResult);

		return false;
	}

	assert (nBytes == 1 || nBytes == 2);
	u16 usBuffer;
	nResult = m_I2CMaster.Read (IMX219_I2C_SLAVE_ADDRESS, &usBuffer, nBytes);
	if (nResult != (int) nBytes)
	{
		LOGWARN ("I2C read failed (%d)", nResult);

		return false;
	}

	assert (pValue);
	if (nBytes == 1)
	{
		*pValue = usBuffer & 0xFF;
	}
	else
	{
		*pValue = be2le16 (usBuffer);
	}

	return true;
}

bool CCameraModule2::WriteReg (u16 usReg, unsigned nBytes, u16 usValue)
{
	u8 Buffer[4] = {(u8) (usReg >> 8), (u8) (usReg & 0xFF)};

	assert (nBytes == 1 || nBytes == 2);
	if (nBytes == 1)
	{
		Buffer[2] = usValue & 0xFF;
	}
	else
	{
		Buffer[2] = usValue >> 8;
		Buffer[3] = usValue & 0xFF;
	}

	int nResult = m_I2CMaster.Write (IMX219_I2C_SLAVE_ADDRESS, Buffer, nBytes + 2);
	if (nResult != (int) (nBytes + 2))
	{
		LOGWARN ("I2C write failed (%d)", nResult);

		return false;
	}

	return true;
}

bool CCameraModule2::WriteRegs (const TReg *pRegs)
{
	assert (pRegs);

	for (; pRegs->Reg; pRegs++)
	{
		if (!WriteReg (pRegs->Reg, 1, pRegs->Value))
		{
			return false;
		}
	}

	return true;
}

/*
 * The supported formats.
 * This table MUST contain 4 entries per format, to cover the various flip
 * combinations in the order
 * - no flip
 * - h flip
 * - v flip
 * - h&v flips
 */
const CCameraDevice::TFormatCode CCameraModule2::s_Formats[][4] =
{
	{
		// physical and logical
		FormatSRGGB8,
		FormatSGRBG8,
		FormatSGBRG8,
		FormatSBGGR8,
	}, {
		// logical only
		FormatSRGGB10,
		FormatSGRBG10,
		FormatSGBRG10,
		FormatSBGGR10,
	}, {
		// physical only
		FormatSRGGB10P,
		FormatSGRBG10P,
		FormatSGBRG10P,
		FormatSBGGR10P,
	}
};

/* Mode configs */
const CCameraModule2::TModeInfo CCameraModule2::s_Modes[] =
{
	{
		/* 8MPix 15fps mode */
		.Width = 3280,
		.Height = 2464,
		.Crop = {
			.Left = IMX219_PIXEL_ARRAY_LEFT,
			.Top = IMX219_PIXEL_ARRAY_TOP,
			.Width = 3280,
			.Height = 2464
		},
		.VTSDef = IMX219_VTS_15FPS,
		.RegList = s_Regs3280x2464Mode,
		.RateFactor = 1,
	}, {
		/* 1080P 30fps cropped */
		.Width = 1920,
		.Height = 1080,
		.Crop = {
			.Left = 688,
			.Top = 700,
			.Width = 1920,
			.Height = 1080
		},
		.VTSDef = IMX219_VTS_30FPS_1080P,
		.RegList = s_Regs1920x1080Mode,
		.RateFactor = 1,
	}, {
		/* 2x2 binned 30fps mode */
		.Width = 1640,
		.Height = 1232,
		.Crop = {
			.Left = IMX219_PIXEL_ARRAY_LEFT,
			.Top = IMX219_PIXEL_ARRAY_TOP,
			.Width = 3280,
			.Height = 2464
		},
		.VTSDef = IMX219_VTS_30FPS_BINNED,
		.RegList = s_Regs1640x1232Mode,
		.RateFactor = 1,
	}, {
		/* 640x480 30fps mode */
		.Width = 640,
		.Height = 480,
		.Crop = {
			.Left = 1008,
			.Top = 760,
			.Width = 1280,
			.Height = 960
		},
		.VTSDef = IMX219_VTS_30FPS_640x480,
		.RegList = s_Regs640x480Mode,
		/*
		 * This mode uses a special 2x2 binning that doubles the
		 * the internal pixel clock rate.
		 */
		.RateFactor = 2,
	}, {
		.Width  = 0,
		.Height = 0
	}
};

/*
 * Register sets lifted off the i2C interface from the Raspberry Pi firmware
 * driver.
 * 3280x2464 = mode 2, 1920x1080 = mode 1, 1640x1232 = mode 4, 640x480 = mode 7.
 */
const CCameraModule2::TReg CCameraModule2::s_Regs3280x2464Mode[] =
{
	{0x0100, 0x00},
	{0x30eb, 0x0c},
	{0x30eb, 0x05},
	{0x300a, 0xff},
	{0x300b, 0xff},
	{0x30eb, 0x05},
	{0x30eb, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012a, 0x18},
	{0x012b, 0x00},
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0c},
	{0x0167, 0xcf},
	{0x0168, 0x00},
	{0x0169, 0x00},
	{0x016a, 0x09},
	{0x016b, 0x9f},
	{0x016c, 0x0c},
	{0x016d, 0xd0},
	{0x016e, 0x09},
	{0x016f, 0xa0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x39},
	{0x030b, 0x01},
	{0x030c, 0x00},
	{0x030d, 0x72},
	{0x0624, 0x0c},
	{0x0625, 0xd0},
	{0x0626, 0x09},
	{0x0627, 0xa0},
	{0x455e, 0x00},
	{0x471e, 0x4b},
	{0x4767, 0x0f},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47b4, 0x14},
	{0x4713, 0x30},
	{0x478b, 0x10},
	{0x478f, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0e},
	{0x479b, 0x0e},
	{0}
};

const CCameraModule2::TReg CCameraModule2::s_Regs1920x1080Mode[] =
{
	{0x0100, 0x00},
	{0x30eb, 0x05},
	{0x30eb, 0x0c},
	{0x300a, 0xff},
	{0x300b, 0xff},
	{0x30eb, 0x05},
	{0x30eb, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012a, 0x18},
	{0x012b, 0x00},
	{0x0164, 0x02},
	{0x0165, 0xa8},
	{0x0166, 0x0a},
	{0x0167, 0x27},
	{0x0168, 0x02},
	{0x0169, 0xb4},
	{0x016a, 0x06},
	{0x016b, 0xeb},
	{0x016c, 0x07},
	{0x016d, 0x80},
	{0x016e, 0x04},
	{0x016f, 0x38},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x39},
	{0x030b, 0x01},
	{0x030c, 0x00},
	{0x030d, 0x72},
	{0x0624, 0x07},
	{0x0625, 0x80},
	{0x0626, 0x04},
	{0x0627, 0x38},
	{0x455e, 0x00},
	{0x471e, 0x4b},
	{0x4767, 0x0f},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47b4, 0x14},
	{0x4713, 0x30},
	{0x478b, 0x10},
	{0x478f, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0e},
	{0x479b, 0x0e},
	{0}
};

const CCameraModule2::TReg CCameraModule2::s_Regs1640x1232Mode[] =
{
	{0x0100, 0x00},
	{0x30eb, 0x0c},
	{0x30eb, 0x05},
	{0x300a, 0xff},
	{0x300b, 0xff},
	{0x30eb, 0x05},
	{0x30eb, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012a, 0x18},
	{0x012b, 0x00},
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0c},
	{0x0167, 0xcf},
	{0x0168, 0x00},
	{0x0169, 0x00},
	{0x016a, 0x09},
	{0x016b, 0x9f},
	{0x016c, 0x06},
	{0x016d, 0x68},
	{0x016e, 0x04},
	{0x016f, 0xd0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x01},
	{0x0175, 0x01},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x39},
	{0x030b, 0x01},
	{0x030c, 0x00},
	{0x030d, 0x72},
	{0x0624, 0x06},
	{0x0625, 0x68},
	{0x0626, 0x04},
	{0x0627, 0xd0},
	{0x455e, 0x00},
	{0x471e, 0x4b},
	{0x4767, 0x0f},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47b4, 0x14},
	{0x4713, 0x30},
	{0x478b, 0x10},
	{0x478f, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0e},
	{0x479b, 0x0e},
	{0}
};

const CCameraModule2::TReg CCameraModule2::s_Regs640x480Mode[] =
{
	{0x0100, 0x00},
	{0x30eb, 0x05},
	{0x30eb, 0x0c},
	{0x300a, 0xff},
	{0x300b, 0xff},
	{0x30eb, 0x05},
	{0x30eb, 0x09},
	{0x0114, 0x01},
	{0x0128, 0x00},
	{0x012a, 0x18},
	{0x012b, 0x00},
	{0x0164, 0x03},
	{0x0165, 0xe8},
	{0x0166, 0x08},
	{0x0167, 0xe7},
	{0x0168, 0x02},
	{0x0169, 0xf0},
	{0x016a, 0x06},
	{0x016b, 0xaf},
	{0x016c, 0x02},
	{0x016d, 0x80},
	{0x016e, 0x01},
	{0x016f, 0xe0},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x03},
	{0x0175, 0x03},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x39},
	{0x030b, 0x01},
	{0x030c, 0x00},
	{0x030d, 0x72},
	{0x0624, 0x06},
	{0x0625, 0x68},
	{0x0626, 0x04},
	{0x0627, 0xd0},
	{0x455e, 0x00},
	{0x471e, 0x4b},
	{0x4767, 0x0f},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47b4, 0x14},
	{0x4713, 0x30},
	{0x478b, 0x10},
	{0x478f, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0e},
	{0x479b, 0x0e},
	{0}
};

const CCameraModule2::TReg CCameraModule2::s_RegsRaw8Frame[] =
{
	{0x018c, 0x08},
	{0x018d, 0x08},
	{0x0309, 0x08},
	{0}
};

const CCameraModule2::TReg CCameraModule2::s_RegsRaw10Frame[] =
{
	{0x018c, 0x0a},
	{0x018d, 0x0a},
	{0x0309, 0x0a},
	{0}
};
