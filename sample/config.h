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

#define WIDTH			m_Screen.GetWidth()	// by default use the screen size
#define HEIGHT			m_Screen.GetHeight()	// will be adjusted to nearest camera format

#define VFLIP			false			// set both to true, to rotate by 180 degrees
#define HFLIP			false

#define EXPOSURE		50			// percent
#define ANALOG_GAIN		50			// percent

#define DIGITAL_GAIN		50			// percent, Camera Module 2 only

#define AUTO_EXPOSURE		true			// Camera Module 1 only
#define AUTO_GAIN		true			// Camera Module 1 only
#define AUTO_WHITE_BALANCE	true			// Camera Module 1 only

#endif
