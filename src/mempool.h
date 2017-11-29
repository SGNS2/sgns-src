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

/* mempool.h

MemoryPool class contents:
	- Memory pool for objects we burn through quickly

*/

#ifndef _MEMPOOL_H
#define _MEMPOOL_H

#include <cassert>
#include <vector>
#include <cstdlib>

template <typename T>
class MemoryPool {
public:
	// ----------------------------------------------------------------------------
	explicit MemoryPool (int allocIncrement = (4096 - 8) / sizeof(T))
		: pools(0), freeObjects(0), allocIncrement(allocIncrement)
	{ }

	// ----------------------------------------------------------------------------
	~MemoryPool() {
		for (unsigned int i = 0; i < pools.size(); i++)
			free( pools[i] );
	}

	// ----------------------------------------------------------------------------
	void reset() {
		for (unsigned int i = 0; i < pools.size(); i++)
			free( pools[i] );
		pools.clear();
		freeObjects.clear();
	}

	// ----------------------------------------------------------------------------
	T *alloc() {
		if (freeObjects.size() == 0) {
			// allocate more
			T *newObjs = (T*)malloc( allocIncrement * sizeof(T) );
			pools.push_back (newObjs);
			for (int i = allocIncrement - 1; i >= 0; i--) {
				freeObjects.push_back (newObjs + i);
			}
		}

		T *freeObj = freeObjects.back();
		freeObjects.pop_back();
#ifdef DEBUG
		*(unsigned int*)obj = 'aliv';
#endif
		return freeObj;
	}

	// ----------------------------------------------------------------------------
	void free (T *obj) {
#ifdef DEBUG
		// Simple check for double-freeing
		unsigned int *uio = (unsigned int*)obj;
		assert( *uio != 'dead', "Object freed twice!" );
		*uio = 'dead';
#endif
		freeObjects.push_back (obj);
	}

private:
	std::vector<T*> pools;
	std::vector<T*> freeObjects;
	int allocIncrement;
};

#endif
