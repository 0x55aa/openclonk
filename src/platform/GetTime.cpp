/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2005, 2011  Günther Brammer
 * Copyright (c) 2012  Martin Plicht
 * Copyright (c) 2001-2009, RedWolf Design GmbH, http://www.clonk.de
 *
 * Portions might be copyrighted by other authors who have contributed
 * to OpenClonk.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * See isc_license.txt for full license and disclaimer.
 *
 * "Clonk" is a registered trademark of Matthes Bender.
 * See clonk_trademark_license.txt for full license.
 */

#include "C4Include.h"
#include "C4TimeMilliseconds.h"

#ifdef _WIN32

#include <C4windowswrapper.h>
#include <mmsystem.h>

C4TimeMilliseconds GetTime()
{
	return C4TimeMilliseconds(timeGetTime());
}

#else

#ifdef __APPLE__
#include <sys/time.h>
#else
#include <time.h>
#endif

C4TimeMilliseconds GetTime()
{
#ifdef __APPLE__
	static time_t sec_offset;
	timeval tv;
	gettimeofday(&tv, 0);
	if (!sec_offset) sec_offset = tv.tv_sec;
	return C4TimeMilliseconds((tv.tv_sec - sec_offset) * 1000 + tv.tv_usec / 1000);
#else
	timespec tv;
	clock_gettime(CLOCK_MONOTONIC, &tv);
	static time_t sec_offset = tv.tv_sec;
	return C4TimeMilliseconds((tv.tv_sec - sec_offset) * 1000 + tv.tv_nsec / 1000000);
#endif
}

#endif
