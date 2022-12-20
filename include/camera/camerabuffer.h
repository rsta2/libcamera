//
// camerabuffer.h
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#ifndef _camera_camerabuffer_h
#define _camera_camerabuffer_h

#include <camera/cameradevice.h>
#include <circle/types.h>

class CCameraBuffer	/// API: Manages access to a captured frame (image) from a camera
{
public:
	struct TPixel
	{
		u16	R;
		u16	G;
		u16	B;
	};

public:
	CCameraBuffer (void);
	~CCameraBuffer (void);

	/// \param x 0-based horizontal pixel coordinate
	/// \param y 0-based vertical pixel coordinate
	/// \return Color components of this pixel (depth bits valid)
	TPixel GetPixel (unsigned x, unsigned y);

	/// \param x 0-based horizontal pixel coordinate
	/// \param y 0-based vertical pixel coordinate
	/// \return RGB888-coded color of this pixel
	u32 GetPixelRGB888 (unsigned x, unsigned y);
	/// \brief Convert frame to RGB888-coded image
	/// \param pOutBuffer Write image to this location in main memory
	void ConvertToRGB888 (void *pOutBuffer);

	/// \param x 0-based horizontal pixel coordinate
	/// \param y 0-based vertical pixel coordinate
	/// \return RGB565-coded color of this pixel
	u16 GetPixelRGB565 (unsigned x, unsigned y);
	/// \brief Convert frame to RGB565-coded image
	/// \param pOutBuffer Write image to this location in main memory
	void ConvertToRGB565 (void *pOutBuffer);

	/// \brief Apply white balance algorithm (improved White Patch method)
	/// \param N Number of pixels, the White Patch method is applied to
	/// \param M Number of samples taken
	/// \note See: N. Banic, S. Loncaric: Improving the White Patch method by sampling
	void WhiteBalance (unsigned N = 50, unsigned M = 10);

	/// \return 0-based sequence number of the frame
	unsigned GetSequenceNumber (void) const;
	/// \return Microseconds timestamp of the frame
	unsigned GetTimestamp (void) const;

	/// \return Pointer to the frame buffer
	/// \note The image is in unpacked 10-bit Bayer format. One pixel occupies two bytes.
	void *GetPtr (void) const;

private:
	bool Setup (size_t nSize);
	void InvalidateCache (void);
	friend class CCameraDevice;

	uintptr GetDMAAddress (void) const;
	void SetSequenceNumber (unsigned nSequence);
	void SetTimestamp (unsigned nTimestamp);
	void SetFormat (unsigned nWidth, unsigned nHeight, unsigned nBytesPerLine,
			CCameraDevice::TFormatCode Format);
	friend class CCSI2CameraDevice;

private:
	size_t m_nSize;
	u8 *m_pBuffer;

	unsigned m_nSequence;
	unsigned m_nTimestamp;

	unsigned m_nWidth;
	unsigned m_nHeight;
	unsigned m_nBytesPerLine;
	CCameraDevice::TFormatCode m_Format;

	unsigned m_ColorFactor[3];	// R, G, B
	unsigned m_nSeed;
};

#endif
