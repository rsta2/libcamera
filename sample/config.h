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

#define WIDTH		640		// define only one WIDTH / HEIGHT combination
#define HEIGHT		480

//#define WIDTH		1920
//#define HEIGHT	1080

#if CAMERA_MODULE == 1
	//#define WIDTH		1296
	//#define HEIGHT	972

	//#define WIDTH		2592
	//#define HEIGHT	1944
#else
	//#define WIDTH		1640
	//#define HEIGHT	1232

	//#define WIDTH		3280
	//#define HEIGHT	2464
#endif

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
