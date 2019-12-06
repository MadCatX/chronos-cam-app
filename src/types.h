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
#ifndef TYPES_H
#define TYPES_H


#define	UInt8	unsigned char
#define	UInt16	unsigned short
#define	UInt32	unsigned int
#define	UInt64	unsigned long long

#define	Int8	char
#define	Int16	short
#ifdef Int32
	#undef Int32
	#define	Int32	int
#else
	#define	Int32	int
#endif // Int32
#ifdef Int64
	#undef Int64
	#define Int64 long long
#else
	#define Int64 long long
#endif // Int64

#endif // TYPES_H
