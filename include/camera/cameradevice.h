//
// cameradevice.h
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#ifndef _camera_cameradevice_h
#define _camera_cameradevice_h

#include <camera/cameracontrol.h>
#include <circle/device.h>
#include <circle/string.h>
#include <circle/types.h>

#define CAMERA_FORMAT_CODE(tl, tr, bl, br, d, p) ((u16) ((d)-1)    | \
						  (u16) (tl) << 4  | \
						  (u16) (tr) << 6  | \
						  (u16) (bl) << 8  | \
						  (u16) (br) << 10 | \
						  (u16) (p)  << 12)

class CCameraBuffer;

/// \note This class defines the camera API.

class CCameraDevice : public CDevice	/// Generic camera device
{
public:
	// Bayer formats
	enum TColorComponent
	{
		R,
		GR,
		GB,
		B
	};

	enum TFormatCode : u16
	{
		FormatSBGGR8	= CAMERA_FORMAT_CODE (B, GB, GR, R, 8, 0),
		FormatSGBRG8	= CAMERA_FORMAT_CODE (GB, B, R, GR, 8, 0),
		FormatSGRBG8	= CAMERA_FORMAT_CODE (GR, R, B, GB, 8, 0),
		FormatSRGGB8	= CAMERA_FORMAT_CODE (R, GR, GB, B, 8, 0),
		FormatSBGGR10	= CAMERA_FORMAT_CODE (B, GB, GR, R, 10, 0),	// occupies 16 bits
		FormatSGBRG10	= CAMERA_FORMAT_CODE (GB, B, R, GR, 10, 0),	// occupies 16 bits
		FormatSGRBG10	= CAMERA_FORMAT_CODE (GR, R, B, GB, 10, 0),	// occupies 16 bits
		FormatSRGGB10	= CAMERA_FORMAT_CODE (R, GR, GB, B, 10, 0),	// occupies 16 bits
		FormatSBGGR10P	= CAMERA_FORMAT_CODE (B, GB, GR, R, 10, 1),
		FormatSGBRG10P	= CAMERA_FORMAT_CODE (GB, B, R, GR, 10, 1),
		FormatSGRBG10P	= CAMERA_FORMAT_CODE (GR, R, B, GB, 10, 1),
		FormatSRGGB10P	= CAMERA_FORMAT_CODE (R, GR, GB, B, 10, 1),
		FormatUnknown	= 0
	};

	static u8 GetFormatDepth (TFormatCode Format)
	{
		return (Format & 0x0F) + 1;
	}

	// Which color component is at pixel position x / y?
	static TColorComponent GetFormatColor (TFormatCode Format, int x, int y)
	{
		return (TColorComponent) ((Format >> ((((x & 1) | (y & 1) << 1) << 1) + 4)) & 3);
	}

	static bool IsFormatPacked (TFormatCode Format)
	{
		return !!(Format & (1 << 12));
	}

	static CString FormatToString (TFormatCode Format);

	struct TRect
	{
		unsigned Left;
		unsigned Top;
		unsigned Width;
		unsigned Height;
	};

	// Logical format info
	struct TFormatInfo
	{
		unsigned	Width;
		unsigned	Height;
		unsigned	BytesPerLine;
		unsigned	Depth;
		size_t		ImageSize;
		TRect		Crop;
		TFormatCode	Code;
	};

	enum TControl
	{
		ControlVBlank,
		ControlHBlank,
		ControlVFlip,			// do not change, when streaming active
		ControlHFlip,			// do not change, when streaming active
		ControlExposure,
		ControlAnalogGain,
		ControlDigitalGain,
		ControlTestPattern,
		ControlTestPatternRed,
		ControlTestPatternGreenR,
		ControlTestPatternGreenB,
		ControlTestPatternBlue,
		ControlUnknown
	};

	typedef void TBufferReadyHandler (unsigned nSequence, void *pParam);

public:
	CCameraDevice (void);
	virtual ~CCameraDevice (void);

	// Must not be called, when streaming active
	virtual bool SetFormat (unsigned nWidth, unsigned nHeight, unsigned nDepth) = 0;

	virtual bool Start (void) = 0;
	virtual void Stop (void) = 0;

	virtual TFormatInfo GetFormatInfo (void) const = 0;

	// Controls
	virtual int GetControlValue (TControl Control) const = 0;
	virtual bool SetControlValue (TControl Control, int nValue) = 0;
	virtual bool SetControlValuePercent (TControl Control, unsigned nPercent) = 0;
	virtual CCameraControl::TControlInfo GetControlInfo (TControl Control) const = 0;

	bool AllocateBuffers (unsigned nBuffers = 3);
	void FreeBuffers (void);

	CCameraBuffer *GetNextBuffer (void);
	void BufferProcessed (void);

	// pHandler = 0 to unregister
	void RegisterBufferReadyHandler (TBufferReadyHandler *pHandler, void *pParam);

protected:
	CCameraBuffer *GetFreeBuffer (void);
	void BufferReady (unsigned nSequence);

private:
	static const unsigned MaxBuffers = 20;

	CCameraBuffer *m_pBuffer[MaxBuffers];
	unsigned m_nBuffers;
	volatile int m_nInPtr;
	volatile int m_nOutPtr;

	TBufferReadyHandler *m_pBufferReadyHandler;
	void *m_pBufferReadyParam;
};

#endif
