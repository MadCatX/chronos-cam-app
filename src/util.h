/****************************************************************************
 *  Copyright (C) 2013-2017 Kron Technologies Inc <http://www.krontech.ca>. *
 *                                                                          *
 *  This program is free software: you can redistribute it and/or modify    *
 *  it under the terms of the GNU General Public License as published by    *
 *  the Free Software Foundation, either version 3 of the License, or       *
 *  (at your option) any later version.                                     *
 *                                                                          *
 *  This program is distributed in the hope that it will be useful,         *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *  GNU General Public License for more details.                            *
 *                                                                          *
 *  You should have received a copy of the GNU General Public License       *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 ****************************************************************************/
#ifndef UTIL_H
#define UTIL_H

#include "types.h"
#include <algorithm>

#if __cplusplus < 201703L

/*
 * The std::clamp function is part of the STL library
 * since C++17 standard. Fake its support in earlier standards
 * to simplify the code that uses it
 */
namespace std {

template <typename T>
inline
const T & clamp(const T& val, const T& min, const T& max)
{
	if (val < min)
		return min;
	else if (val > max)
		return max;
	return val;
}

} // namespace std

#endif // No C++17

void delayms(int ms);

/* delayms_events:
 *   ms - miliseconds to delay for
 *
 * Note this runs the event loop in 10ms blocks (max) so assume
 * the timing will be within 10ms only.
 */
void delayms_events(int ms);

bool checkAndCreateDir(const char * dir);

int path_is_mounted(const char *path);

UInt16 readPixelBuf12(const void * buf, UInt32 pixel);
void writePixelBuf12(void * buf, UInt32 pixel, UInt16 value);

#define ROUND_UP_MULT(x, mult)	(((x) + ((mult) - 1)) & ~((mult) - 1))

#endif // UTIL_H
