//
// config.h
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#ifndef _config_h
#define _config_h

#define CAMERA_MODULE	2		// or 1

// Supported image formats:
//
// Width x Height	Camera Module
//
//  640 x  480		1 & 2
// 1296 x  972		1
// 1640 x 1232		2
// 1920 x 1080		1 & 2
// 2592 x 1944		1
// 3280 x 2464		2

#define WIDTH		m_Screen.GetWidth()	// by default use the screen size
#define HEIGHT		m_Screen.GetHeight()	// will be adjusted to nearest camera format

#define VFLIP		false
#define HFLIP		false

#define EXPOSURE	50	// percent
#define ANALOG_GAIN	50	// percent

#if CAMERA_MODULE == 1
	#define AUTO_EXPOSURE		true
	#define AUTO_GAIN		true
	#define AUTO_WHITE_BALANCE	true
#else
	#define DIGITAL_GAIN	50	// percent
#endif

#endif
