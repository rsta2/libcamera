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

/// \note This class defines the main camera API.

class CCameraDevice : public CDevice	/// API: Generic camera device
{
public:
	/// \brief Color components of a Bayer formatted image frame
	enum TColorComponent
	{
		R,
		GR,
		GB,
		B
	};

	/// \brief The supported (Bayer) image formats
	enum TFormatCode : u16
	{
		FormatSBGGR8	= CAMERA_FORMAT_CODE (B, GB, GR, R, 8, 0),
		FormatSGBRG8	= CAMERA_FORMAT_CODE (GB, B, R, GR, 8, 0),
		FormatSGRBG8	= CAMERA_FORMAT_CODE (GR, R, B, GB, 8, 0),
		FormatSRGGB8	= CAMERA_FORMAT_CODE (R, GR, GB, B, 8, 0),
		FormatSBGGR10	= CAMERA_FORMAT_CODE (B, GB, GR, R, 10, 0),	///< occupies 16 bits
		FormatSGBRG10	= CAMERA_FORMAT_CODE (GB, B, R, GR, 10, 0),	///< occupies 16 bits
		FormatSGRBG10	= CAMERA_FORMAT_CODE (GR, R, B, GB, 10, 0),	///< occupies 16 bits
		FormatSRGGB10	= CAMERA_FORMAT_CODE (R, GR, GB, B, 10, 0),	///< occupies 16 bits
		FormatSBGGR10P	= CAMERA_FORMAT_CODE (B, GB, GR, R, 10, 1),
		FormatSGBRG10P	= CAMERA_FORMAT_CODE (GB, B, R, GR, 10, 1),
		FormatSGRBG10P	= CAMERA_FORMAT_CODE (GR, R, B, GB, 10, 1),
		FormatSRGGB10P	= CAMERA_FORMAT_CODE (R, GR, GB, B, 10, 1),
		FormatUnknown	= 0
	};

	/// \param Format Bayer format code of a frame
	/// \return Number of valid bits of a color component
	static u8 GetFormatDepth (TFormatCode Format)
	{
		return (Format & 0x0F) + 1;
	}

	/// \param Format Bayer format code of a frame
	/// \param x 0-based horizontal pixel coordinate
	/// \param y 0-based vertical pixel coordinate
	/// \return Which color component is located at this pixel position?
	static TColorComponent GetFormatColor (TFormatCode Format, int x, int y)
	{
		return (TColorComponent) ((Format >> ((((x & 1) | (y & 1) << 1) << 1) + 4)) & 3);
	}

	/// \param Format Bayer format code of a frame
	/// \return Is this a packed format?
	static bool IsFormatPacked (TFormatCode Format)
	{
		return !!(Format & (1 << 12));
	}

	/// \param Format Bayer format code of a frame
	/// \return String representation for this format
	static CString FormatToString (TFormatCode Format);

	struct TRect
	{
		unsigned Left;
		unsigned Top;
		unsigned Width;
		unsigned Height;
	};

	/// \brief Logical format info
	struct TFormatInfo
	{
		unsigned	Width;		///< Width of the frame
		unsigned	Height;		///< Height of the frame
		unsigned	BytesPerLine;	///< Number of bytes of a pixel line (with padding)
		unsigned	Depth;		///< Number of valid bits of the color information
		size_t		ImageSize;	///< Total image size in bytes
		TRect		Crop;		///< Crop info from the sensor (currently unused)
		TFormatCode	Code;		///< Format code of the frame
	};

	/// \brief The supported camera controls
	enum TControl
	{
		ControlVBlank,
		ControlHBlank,			///< Camera Module 2 only
		ControlVFlip,			///< do not change, when streaming active
		ControlHFlip,			///< do not change, when streaming active
		ControlExposure,
		ControlAnalogGain,
		ControlDigitalGain,		///< Camera Module 2 only

		ControlAutoExposure,		///< Camera Module 1 only
		ControlAutoGain,		///< Camera Module 1 only
		ControlAutoWhiteBalance,	///< Camera Module 1 only

		ControlTestPattern,		///< Camera Module 2 only
		ControlTestPatternRed,		///< Camera Module 2 only
		ControlTestPatternGreenR,	///< Camera Module 2 only
		ControlTestPatternGreenB,	///< Camera Module 2 only
		ControlTestPatternBlue,		///< Camera Module 2 only

		ControlUnknown
	};

	typedef void TBufferReadyHandler (unsigned nSequence, void *pParam);

public:
	CCameraDevice (void);
	virtual ~CCameraDevice (void);

	/// \brief For internal use only
	virtual bool Probe (void) = 0;

	/// \brief Call this first to define the wanted image frame format
	/// \param nWidth Wanted width of the frame in number of pixels
	/// \param nHeight Wanted height of the frame in number of pixel lines
	/// \param nDepth Number of valid bits in the color information (must be 10 currently)
	/// \return Operation successful?
	/// \note The actual frame size may be different from the wanted one.
	///	  Use GetFormatInfo() to get it.
	/// \note Must not be called, when streaming active.
	virtual bool SetFormat (unsigned nWidth, unsigned nHeight, unsigned nDepth = 10) = 0;

	/// \return Information about the actual image frame format.
	/// \note Must be called after SetFormat().
	/// \note The image frame format may be influenced by control settings (e.g. VFlip, HFlip)
	virtual TFormatInfo GetFormatInfo (void) const = 0;

	/// \brief Allocate the frame buffers for the camera
	/// \param nBuffers Number of buffers (3 .. 20)
	/// \return Operation successful?
	/// \note Must be called after SetFormat(), because the buffer size is not known before.
	bool AllocateBuffers (unsigned nBuffers = 3);
	/// \brief Free the frame buffers
	/// \note Should be called after Stop(), when a Start() with the same format will not follow.
	/// \note In Circle by default buffers with a size greater than 512K cannot be reused.
	///	  This is configurable by the system option HEAP_BLOCK_BUCKET_SIZES.
	void FreeBuffers (void);

	/// \param Control Camera control selector
	/// \return Is this control supported by this camera?
	virtual bool IsControlSupported (TControl Control) const = 0;
	/// \param Control Camera control selector
	/// \return The current value of the control
	virtual int GetControlValue (TControl Control) const = 0;
	/// \brief Set a camera control to new value
	/// \param Control Camera control selector
	/// \param nValue Value to be set
	/// \return New value successfully set
	virtual bool SetControlValue (TControl Control, int nValue) = 0;
	/// \brief Set a camera control to new value in percent of the available range
	/// \param Control Camera control selector
	/// \param nPercent Percent value (0 .. 100)
	/// \return New value successfully set
	virtual bool SetControlValuePercent (TControl Control, unsigned nPercent) = 0;
	/// \param Control Camera control selector
	/// \return Information about this control (see class CCameraControl)
	virtual CCameraControl::TControlInfo GetControlInfo (TControl Control) const = 0;

	/// \brief Start streaming operation
	/// \param bLEDOn Switch camera LED on
	/// \return Operation successful?
	/// \note Not all cameras have a LED and not all Raspberry Pi models can drive it.
	virtual bool Start (bool bLEDOn = true) = 0;
	/// \brief Stop streaming operation
	virtual void Stop (void) = 0;

	/// \brief Get the next frame buffer, which is ready to process (filled with data)
	/// \return Pointer to the buffer instance (or nullptr, if no buffer is available)
	CCameraBuffer *GetNextBuffer (void);
	/// \brief Return a previously processed buffer to the buffer queue
	void BufferProcessed (void);

	/// \brief Register a callback, which gets called, when a buffer is ready to process
	/// \param pHandler Pointer to the handler (nullptr to unregister)
	/// \param pParam User parameter, which will be handed over to the callback
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
