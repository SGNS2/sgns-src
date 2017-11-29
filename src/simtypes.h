/*
Copyright (c) 2011, Jason Lloyd-Price, Abhishekh Gupta, and Andre S. Ribeiro
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * The names of the contributors may not be used to endorse or promote
	  products derived from this software without specific prior written
	  permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* simtypes.h

Contents:
	- Simulation types used throughout SGNS2

*/

#ifndef SIMTYPES_H
#define SIMTYPES_H

#include <cstring>
#include <stdint.h>
#include <cstdlib>

namespace sgns2 {

#ifdef _WIN32
#define SGNS_NOVTABLE __declspec( novtable )
#ifndef _M_IX86
#define SGNS_FASTCALL __fastcall
#else
#define SGNS_FASTCALL
#endif
#else
#define SGNS_NOVTABLE
#define SGNS_FASTCALL
#endif

typedef uint_fast32_t uint;
typedef int_fast64_t Population;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint64_t uint64;
typedef int64_t int64;
typedef uintptr_t uintp;

struct StringCmpLt_Functor {
	inline bool operator()( const char *s1, const char *s2 ) const {
		return strcmp( s1, s2 ) < 0;
	}
};

} // namespace

#endif // SIMTYPES_H
