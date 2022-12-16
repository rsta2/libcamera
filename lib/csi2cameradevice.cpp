//
// csi2cameradevice.cpp
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// Based on the Linux driver:
//	drivers/media/platform/bcm2835/bcm2835-unicam.c:
//
// SPDX-License-Identifier: GPL-2.0-only
/*
 * BCM283x / BCM271x Unicam Capture Driver
 *
 * Copyright (C) 2017-2020 - Raspberry Pi (Trading) Ltd.
 *
 * Dave Stevenson <dave.stevenson@raspberrypi.com>
 *
 * Based on TI am437x driver by
 *   Benoit Parrot <bparrot@ti.com>
 *   Lad, Prabhakar <prabhakar.csengg@gmail.com>
 *
 * and TI CAL camera interface driver by
 *    Benoit Parrot <bparrot@ti.com>
 */
#include <camera/csi2cameradevice.h>
#include <camera/camerabuffer.h>
#include <circle/bcmpropertytags.h>
#include <circle/synchronize.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/macros.h>
#include <circle/util.h>
#include <assert.h>
#include "vc4-regs-unicam.h"

// Stride is a 16 bit register, but also has to be a multiple of 32.
#define BPL_ALIGNMENT		32
#define MAX_BYTESPERLINE	((1 << 16) - BPL_ALIGNMENT)

// Max width is therefore determined by the max stride divided by
// the number of bits per pixel. Take 32bpp as a worst case.
// No imposed limit on the height, so adopt a square image for want
// of anything better.
#define MAX_WIDTH		(MAX_BYTESPERLINE / 4)
#define MAX_HEIGHT		MAX_WIDTH
// Define a nominal minimum image size.
#define MIN_WIDTH		16
#define MIN_HEIGHT		16

#define GENMASK(h, l)	(  (~0UL - (1UL << (l)) + 1)	\
			 & (~0UL >> (AARCH-1 - (h))))

#define __ALIGN(n, m)	(((n) + (m) - 1) & ~((m) - 1))

LOGMODULE ("csi2");

CCSI2CameraDevice::CCSI2CameraDevice (CInterruptSystem *pInterruptSystem)
:	m_pInterruptSystem (pInterruptSystem),
	m_bIRQConnected (false),
	m_bActive (false),
#if RASPPI <= 3
	m_CAM1Clock (GPIOClockCAM1),
#else
	m_CAM1Clock (GPIOClockCAM1, GPIOClockSourcePLLD),
#endif
	m_nWidth (0),
	m_nHeight (0),
	m_nBytesPerLine (0),
	m_nImageSize (0),
	m_pCurrentBuffer (nullptr),
	m_pDummyBuffer (new u8[4096])
{
}

CCSI2CameraDevice::~CCSI2CameraDevice (void)
{
	if (m_bActive)
	{
		DisableRX ();
	}

	SetPower (false);

	if (m_bIRQConnected)
	{
		assert (m_pInterruptSystem);
		m_pInterruptSystem->DisconnectIRQ (ARM_IRQ_CAM1);
	}

	delete [] m_pDummyBuffer;
	m_pDummyBuffer = nullptr;

	assert (!m_pCurrentBuffer);
}

bool CCSI2CameraDevice::Initialize (void)
{
	assert (m_pInterruptSystem);
	m_pInterruptSystem->ConnectIRQ (ARM_IRQ_CAM1, InterruptStub, this);
	m_bIRQConnected = true;

	if (!SetPower (true))
	{
		LOGERR ("Cannot enable power");

		return false;
	}

	return true;
}

bool CCSI2CameraDevice::SetFormat (unsigned nWidth, unsigned nHeight, unsigned nDepth)
{
	assert (!m_bActive);

	// call camera driver
	if (!SetMode (nWidth, nHeight, nDepth))
	{
		return false;
	}

	m_nWidth = nWidth;
	m_nHeight = nHeight;

	// calculate format sizes
	assert (m_nWidth);
	if (m_nWidth < MIN_WIDTH) m_nWidth = MIN_WIDTH;
	if (m_nWidth > MAX_WIDTH) m_nWidth = MAX_WIDTH;
	m_nWidth &= ~3UL;

	assert (m_nHeight);
	if (m_nHeight < MIN_WIDTH) m_nHeight = MIN_WIDTH;
	if (m_nHeight > MAX_WIDTH) m_nHeight = MAX_WIDTH;

	if (GetFormatDepth (GetLogicalFormat ()) == 8)
	{
		m_nBytesPerLine = __ALIGN (m_nWidth, BPL_ALIGNMENT);
	}
	else
	{
		assert (GetFormatDepth (GetLogicalFormat ()) == 10);
		m_nBytesPerLine = __ALIGN (m_nWidth * 2, BPL_ALIGNMENT);	// occupies 16 bits
	}

	m_nImageSize = m_nHeight * m_nBytesPerLine;

	LOGDBG ("Image size is %lu (line %u)", m_nImageSize, m_nBytesPerLine);

	return true;
}

CCameraDevice::TFormatInfo CCSI2CameraDevice::GetFormatInfo (void) const
{
	TFormatInfo Info;

	Info.Width = m_nWidth;
	Info.Height = m_nHeight;
	Info.BytesPerLine = m_nBytesPerLine;
	Info.ImageSize = m_nImageSize;
	Info.Crop = GetCropInfo ();
	Info.Code = GetLogicalFormat ();

	Info.Depth = GetFormatDepth (Info.Code);

	return Info;
}

bool CCSI2CameraDevice::EnableRX (void)
{
	assert (!m_bActive);

#if RASPPI <= 3
	if (!m_CAM1Clock.StartRate (100000000))
	{
		LOGERR ("Cannot start CAM1 clock");

		return false;
	}
#else
	m_CAM1Clock.Start (7, 512, 1);
#endif

	m_bActive = true;

	m_nSequence = 0;

	u8 uchDepth = GetFormatDepth (GetPhysicalFormat ());
	assert (uchDepth == 8 || uchDepth == 10);

	// Enable lane clocks (2 lanes)
	ClockWrite (0b010101);

	PeripheralEntry ();

	// Basic init
	WriteReg (UNICAM_CTRL, UNICAM_MEM);

	// Enable analogue control, and leave in reset.
	u32 nValue = UNICAM_AR;
	SetField (&nValue, 7, UNICAM_CTATADJ_MASK);
	SetField (&nValue, 7, UNICAM_PTATADJ_MASK);
	WriteReg (UNICAM_ANA, nValue);

	CTimer::Get ()->usDelay (1000);

	// Come out of reset
	WriteRegField (UNICAM_ANA, 0, UNICAM_AR);

	// Peripheral reset
	WriteRegField (UNICAM_CTRL, 1, UNICAM_CPR);
	WriteRegField (UNICAM_CTRL, 0, UNICAM_CPR);

	WriteRegField (UNICAM_CTRL, 0, UNICAM_CPE);

	// Enable Rx control (CSI2 DPHY)
	nValue = ReadReg (UNICAM_CTRL);
	SetField (&nValue, UNICAM_CPM_CSI2, UNICAM_CPM_MASK);
	SetField (&nValue, UNICAM_DCM_STROBE, UNICAM_DCM_MASK);

	// Packet framer timeout
	SetField (&nValue, 0xf, UNICAM_PFT_MASK);
	SetField (&nValue, 128, UNICAM_OET_MASK);
	WriteReg (UNICAM_CTRL, nValue);

	WriteReg (UNICAM_IHWIN, 0);
	WriteReg (UNICAM_IVWIN, 0);

	// AXI bus access QoS setup
	nValue = ReadReg (UNICAM_PRI);
	SetField (&nValue, 0, UNICAM_BL_MASK);
	SetField (&nValue, 0, UNICAM_BS_MASK);
	SetField (&nValue, 0xe, UNICAM_PP_MASK);
	SetField (&nValue, 8, UNICAM_NP_MASK);
	SetField (&nValue, 2, UNICAM_PT_MASK);
	SetField (&nValue, 1, UNICAM_PE);
	WriteReg (UNICAM_PRI, nValue);

	WriteRegField (UNICAM_ANA, 0, UNICAM_DDL);

	u32 nLineIntFreq = m_nHeight >> 2;
	nValue = UNICAM_FSIE | UNICAM_FEIE | UNICAM_IBOB;
	SetField (&nValue, nLineIntFreq >= 128 ? nLineIntFreq : 128, UNICAM_LCIE_MASK);
	WriteReg (UNICAM_ICTL, nValue);
	WriteReg (UNICAM_STA, UNICAM_STA_MASK_ALL);
	WriteReg (UNICAM_ISTA, UNICAM_ISTA_MASK_ALL);

	WriteRegField (UNICAM_CLT, 2, UNICAM_CLT1_MASK);	// tclk_term_en
	WriteRegField (UNICAM_CLT, 6, UNICAM_CLT2_MASK);	// tclk_settle
	WriteRegField (UNICAM_DLT, 2, UNICAM_DLT1_MASK);	// td_term_en
	WriteRegField (UNICAM_DLT, 6, UNICAM_DLT2_MASK);	// ths_settle
	WriteRegField (UNICAM_DLT, 0, UNICAM_DLT3_MASK);	// trx_enable

	WriteRegField (UNICAM_CTRL, 0, UNICAM_SOE);

	// Packet compare setup - required to avoid missing frame ends
	nValue = 0;
	SetField (&nValue, 1, UNICAM_PCE);
	SetField (&nValue, 1, UNICAM_GI);
	SetField (&nValue, 1, UNICAM_CPH);
	SetField (&nValue, 0, UNICAM_PCVC_MASK);
	SetField (&nValue, 1, UNICAM_PCDT_MASK);
	WriteReg (UNICAM_CMP0, nValue);

	// Enable clock lane and set up terminations (CSI2 DPHY, non-continous clock)
	nValue = 0;
	SetField (&nValue, 1, UNICAM_CLE);
	SetField (&nValue, 1, UNICAM_CLLPE);
	WriteReg (UNICAM_CLK, nValue);

	// Enable required data lanes with appropriate terminations.
	// The same value needs to be written to UNICAM_DATn registers for
	// the active lanes, and 0 for inactive ones.
	// (CSI2 DPHY, non-continous clock, 2 data lanes)
	nValue = 0;
	SetField (&nValue, 1, UNICAM_DLE);
	SetField (&nValue, 1, UNICAM_DLLPE);
	WriteReg (UNICAM_DAT0, nValue);
	WriteReg (UNICAM_DAT1, nValue);

	assert (m_nBytesPerLine);
	WriteReg (UNICAM_IBLS, m_nBytesPerLine);

	// Write DMA buffer address
	assert (m_pDummyBuffer);
	uintptr nDMAAddress = BUS_ADDRESS (reinterpret_cast<uintptr> (m_pDummyBuffer));
	WriteReg (UNICAM_IBSA0, nDMAAddress);
	WriteReg (UNICAM_IBEA0, nDMAAddress + 0);

	// Set packing configuration
	u32 nUnPack = UNICAM_PUM_NONE;
	u32 nPack = UNICAM_PPM_NONE;
	if (uchDepth == 10)
	{
		nUnPack = UNICAM_PUM_UNPACK10;
		nPack = UNICAM_PPM_PACK16;		// Repacking to 16bpp
	}

	nValue = 0;
	SetField (&nValue, nUnPack, UNICAM_PUM_MASK);
	SetField (&nValue, nPack, UNICAM_PPM_MASK);
	WriteReg (UNICAM_IPIPE, nValue);

	// CSI2 mode, hardcode VC 0 for now.
	WriteReg (UNICAM_IDI0, (0 << 6) | (uchDepth == 8 ? 0x2a : 0x2b));

	nValue = ReadReg (UNICAM_MISC);
	SetField (&nValue, 1, UNICAM_FL0);
	SetField (&nValue, 1, UNICAM_FL1);
	WriteReg (UNICAM_MISC, nValue);

	// Clear ED setup
	WriteReg (UNICAM_DCS, 0);

	// Enable peripheral
	WriteRegField (UNICAM_CTRL, 1, UNICAM_CPE);

	// Load image pointers
	WriteRegField (UNICAM_ICTL, 1, UNICAM_LIP_MASK);

	PeripheralExit ();

	return true;
}

void CCSI2CameraDevice::DisableRX (void)
{
	assert (m_bActive);

	PeripheralEntry ();

	// Analogue lane control disable
	WriteRegField (UNICAM_ANA, 1, UNICAM_DDL);

	// Stop the output engine
	WriteRegField (UNICAM_CTRL, 1, UNICAM_SOE);

	// Disable the data lanes
	WriteReg (UNICAM_DAT0, 0);
	WriteReg (UNICAM_DAT1, 0);

	// Peripheral reset
	WriteRegField (UNICAM_CTRL, 1, UNICAM_CPR);
	CTimer::Get ()->usDelay (50);
	WriteRegField (UNICAM_CTRL, 0, UNICAM_CPR);

	// Disable peripheral
	WriteRegField (UNICAM_CTRL, 0, UNICAM_CPE);

	PeripheralExit ();

	// Disable all lane clocks
	ClockWrite (0);

	m_CAM1Clock.Stop ();

	m_bActive = false;

	m_pCurrentBuffer = nullptr;
}

void CCSI2CameraDevice::InterruptHandler (void)
{
	PeripheralEntry ();

	u32 nSTA = ReadReg (UNICAM_STA);
	WriteReg (UNICAM_STA, nSTA);		// Write value back to clear the interrupts

	u32 nISTA = ReadReg (UNICAM_ISTA);
	WriteReg (UNICAM_ISTA, nISTA);		// Write value back to clear the interrupts

	if (   !(nSTA & (UNICAM_IS | UNICAM_PI0))
	    || !m_bActive)
	{
		PeripheralExit ();

		return;
	}

	// Look for either the Frame End interrupt or the Packet Capture status
	// to signal a frame end.
	if ((nISTA & UNICAM_FEI) || (nSTA & UNICAM_PI0))
	{
		if (m_pCurrentBuffer)
		{
			m_pCurrentBuffer->SetSequenceNumber (m_nSequence);

			m_pCurrentBuffer->SetFormat (m_nWidth, m_nHeight, m_nBytesPerLine,
						     GetLogicalFormat ());

			BufferReady (m_nSequence);

			m_pCurrentBuffer = nullptr;
		}

		m_nSequence++;
	}

	// Frame start?
	if (nISTA & UNICAM_FSI)
	{
		if (!m_pCurrentBuffer)
		{
			m_pCurrentBuffer = GetFreeBuffer ();
		}

		if (m_pCurrentBuffer)
		{
			m_pCurrentBuffer->SetTimestamp (CTimer::Get ()->GetClockTicks ());

			assert (m_nImageSize);
			uintptr nDMAAddress = m_pCurrentBuffer->GetDMAAddress ();
			WriteReg (UNICAM_IBSA0, nDMAAddress);
			WriteReg (UNICAM_IBEA0, nDMAAddress + m_nImageSize);
		}
		else
		{
			assert (m_pDummyBuffer);
			uintptr nDMAAddress = BUS_ADDRESS (
					reinterpret_cast<uintptr> (m_pDummyBuffer));
			WriteReg (UNICAM_IBSA0, nDMAAddress);
			WriteReg (UNICAM_IBEA0, nDMAAddress + 0);
		}
	}

	PeripheralExit ();
}

void CCSI2CameraDevice::InterruptStub (void *pParam)
{
	CCSI2CameraDevice *pThis = static_cast<CCSI2CameraDevice *> (pParam);
	assert (pThis);

	pThis->InterruptHandler ();
}

bool CCSI2CameraDevice::SetPower (bool bOn)
{
	CBcmPropertyTags Tags;
	TPropertyTagDomainState DomainState;
	DomainState.nDomainId = DOMAIN_ID_UNICAM1;
	DomainState.nOn = bOn ? DOMAIN_STATE_ON : DOMAIN_STATE_OFF;

	return Tags.GetTag (PROPTAG_SET_DOMAIN_STATE, &DomainState, sizeof DomainState, 8);
}

void CCSI2CameraDevice::ClockWrite (u32 nValue)
{
	PeripheralEntry ();

	write32 (ARM_CSI1_CLKGATE, ARM_CM_PASSWD | nValue);

	PeripheralExit ();
}

void CCSI2CameraDevice::WriteRegField (u32 nOffset, u32 nValue, u32 nMask)
{
	u32 nBuffer = ReadReg (nOffset);

	SetField (&nBuffer, nValue, nMask);

	WriteReg (nOffset, nBuffer);
}

void CCSI2CameraDevice::SetField (u32 *pValue, u32 nValue, u32 nMask)
{
	assert (nMask);
	u32 nTempMask = nMask;
	while (!(nTempMask & 1))
	{
		nValue <<= 1;
		nTempMask >>= 1;
	}

	assert (pValue);
	assert (!(nValue & ~nMask));
	*pValue = (*pValue & ~nMask) | nValue;
}
