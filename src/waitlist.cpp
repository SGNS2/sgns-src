
// See waitlist.h for a description of the contents of this file.

#include "stdafx.h"

#include <new>

#include "compartment.h"
#include "waitlist.h"

namespace sgns2 {

// ---------------------------------------------------------------------------
WaitList::WaitList( Compartment *in ) throw()
: EventStream(in), EventQueue(), countAmount(0)
{
	newMin = &newMinHeap;
}

// ---------------------------------------------------------------------------
WaitList::~WaitList() throw() {
}

// ---------------------------------------------------------------------------
void WaitList::releaseAt( double t, uint idx, Population amt ) {
	// Add a new element at a specific given time
	ReleaseEvent *re = eventPool.alloc();
	new( re ) ReleaseEvent( this, t, idx, amt );
	countAmount = countAmount + amt;
}

// ---------------------------------------------------------------------------
void WaitList::trigger() throw() {
	// Release a product
	ReleaseEvent *re = static_cast<ReleaseEvent*>(getNextEvent());
	static_cast<Compartment*>(getQueue())->modifyPopulation( re->idx, re->amt );
	countAmount = countAmount - re->amt;
	re->~ReleaseEvent(); // Dequeues the event
	eventPool.free( re );
}

// ---------------------------------------------------------------------------
void WaitList::update() throw() {
	// Update does nothing
}

} // namespace sgns2
