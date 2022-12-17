//
// math.h
//
// libcamera - Camera support for Circle
// Copyright (C) 2022  Rene Stange <rsta2@o2online.de>
//
// SPDX-License-Identifier: GPL-2.0
//
#ifndef _math_h
#define _math_h

#include <math.h>

#define RAND_MAX 32767

static inline int rand_r (unsigned *pSeed)
{
	*pSeed = *pSeed * 1103515245 + 12345;

	return (unsigned) (*pSeed / 65536) % 32768;
}

#endif
