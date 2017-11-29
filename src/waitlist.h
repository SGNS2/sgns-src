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

/* waitlist.h/cpp

WaitList class contents:
	- A specialization of EventQueue to store delayed molecules
*/

#ifndef WAITLIST_H
#define WAITLIST_H

#include "mempool.h"
#include "event.h"
#include "simtypes.h"

namespace sgns2 {

class Compartment;

class WaitList : public EventStream, public EventQueue {
public:
	WaitList( Compartment *in ) throw();
	~WaitList() throw();

	// Add a new element at a specific given time
	void releaseAt( double t, uint idx, Population amt );
	// Release a product
	void trigger() throw();
	// Update the queue
	void update() throw();
	// Returns the total number of molecules currently on the list
	Population getSize() throw() { return countAmount; }


private:
	class ReleaseEvent : public Event {
	public:
		ReleaseEvent( WaitList *wl, double time, uint index, Population amount )
			: Event( wl ), idx(index), amt(amount) { enqueue( time ); }
		~ReleaseEvent() { }

		uint idx;
		Population amt;
	};
	MemoryPool<ReleaseEvent> eventPool;

	Population countAmount;
	inline void newMinHeap_inl() {
		enqueue( EventQueue::getNextEventTime() );
	}
	static void SGNS_FASTCALL newMinHeap( EventQueueBase *q ) {
		static_cast<WaitList*>(q)->newMinHeap_inl();
	}
};

} // namespace sgns2

#endif //WAITLIST_H
