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

/* multithread.h/cpp

Contents:
	- Provides platform-specific multithreading functions

*/

#ifndef MULTITHREAD_H
#define MULTITHREAD_H

namespace mt {

// Returns the optimal number of threads to spawn to use all
// available resources
unsigned coreCount();
// Starts a new thread
void spawnThread( void (*start)( void* ), void *cookie );

// Create a new mutual exclusion object
void *newMutex();
// Delete a mutex
void deleteMutex( void *mut );
// Locks a mutex - only one thread may hold a lock at once (may block)
void lock( void *mut );
// Unlocks a mutex - allows other threads to gain the lock
void unlock( void *mut );

// Create a new semaphore with the given start count
void *newSemaphore( int startCount = 1 );
// Delete a semaphore
void deleteSemaphore( void *sema );
// Attempts to decrement the semaphore's count (may block)
// A semaphore's count is never allowed to drop below 0
void p( void *sema );
// Increases a semaphore's count
void v( void *sema );

} // namespace mt

#endif
