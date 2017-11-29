
// See event.h for a description of the contents of this file.

#include "stdafx.h"

#include <limits>
#include <cassert>

#include "event.h"

namespace sgns2
{

// ---------------------------------------------------------------------------
EventQueueBase::EventQueueBase() throw()
: baseTime(0.0)
, newMin(&empty_newMin)
{
}

// ---------------------------------------------------------------------------
EventQueueBase::~EventQueueBase() throw() {
}

// ---------------------------------------------------------------------------
void SGNS_FASTCALL EventQueueBase::empty_newMin( EventQueueBase *q ) {
	(void)q;
	assert( static_cast<EventQueue*>(q)->getNextEventTime() >= static_cast<EventQueue*>(q)->getBaseTime() );
}

// ---------------------------------------------------------------------------
EventQueue_BinaryHeap::EventQueue_BinaryHeap() throw()
: heapSize(1)
, heapCapacity(8)
, heap(NULL)
{
	heap = new EventQueueEntry[heapCapacity];
	heap[0].time = -std::numeric_limits<double>::infinity();
}

// ---------------------------------------------------------------------------
EventQueue_BinaryHeap::~EventQueue_BinaryHeap() throw() {
	delete[] heap;
}

// ---------------------------------------------------------------------------
void EventQueue_BinaryHeap::resize( uint newCapacity ) throw() {
	assert( heapSize <= newCapacity );

	EventQueueEntry *newHeap = new EventQueueEntry[newCapacity];
	memcpy( newHeap, heap, heapSize * sizeof( EventQueueEntry ) );

	heapCapacity = newCapacity;
	delete[] heap;
	heap = newHeap;
}

// ---------------------------------------------------------------------------
void EventQueue_BinaryHeap::bubbleAround( EventQueueEntry *entry, uint i ) throw() {
	uint ni = i >> 1;
	if( entry->time < heap[ni].time ) {
		heap[ni].evt->queueIndex = i;
		heap[i] = heap[ni];
		bubbleUp( entry, ni );
	} else {
		bubbleDown( entry, i );
	}
}

// ---------------------------------------------------------------------------
void EventQueue_BinaryHeap::bubbleUp( EventQueueEntry *entry, uint i ) throw() {
	double t = entry->time;
	assert( t >= getBaseTime() );
	
	uint ni = i >> 1;
	while( heap[ni].time > t ) {
		heap[ni].evt->queueIndex = i;
		heap[i] = heap[ni];
		ni = (i = ni) >> 1;
	}

	heap[i].time = t;
	heap[i].evt = entry->evt;
	entry->evt->queueIndex = i;

	if( i == 1 )
		newMin( this );
}

// ---------------------------------------------------------------------------
void EventQueue_BinaryHeap::bubbleDown( EventQueueEntry *entry, uint i ) throw() {
	bool callNewMin = i == 1;

	double t = entry->time;
	assert( t >= getBaseTime() );
	while( true ) {
		uint lhs = i << 1;
		uint rhs = lhs + 1;
		uint ni;

		if( rhs < heapSize ) {
			// Both subtrees exist
			if( !(heap[lhs].time < t || heap[rhs].time < t) )
				break; // Neither are earlier than t - don't bubble down further

			// Move the root of the soonest subtree up
			ni = heap[lhs].time < heap[rhs].time ? lhs : rhs;
		} else if( lhs < heapSize ) {
			// Left subtree exists
			if( !(heap[lhs].time < t) )
				break; // Left subtree is later than t - don't bubble down further

			// Left subtree is sooner than t, so move it up
			ni = lhs;
		} else {
			break;
		}

		assert( heap[ni].time >= getBaseTime() );
		heap[ni].evt->queueIndex = i;
		heap[i] = heap[ni];
		i = ni;
	}

	heap[i].time = t;
	heap[i].evt = entry->evt;
	entry->evt->queueIndex = i;

	if( callNewMin )
		newMin( this );
}


// ---------------------------------------------------------------------------
Event::Event( EventQueue *parent ) throw()
: queueIndex(0)
, parentQueue(parent)
{
}

// ---------------------------------------------------------------------------
Event::~Event() throw() {
	dequeue();
}

// ---------------------------------------------------------------------------
void Event::enqueue( double newTime ) throw() {
	EventQueue::EventQueueEntry entry;
	entry.evt = this;
	entry.time = newTime;
	if( isInQueue() ) {
		if( newTime < getNextEventTimeRaw() ) {
			parentQueue->bubbleUp( &entry, queueIndex );
		} else {
			parentQueue->bubbleDown( &entry, queueIndex );
		}
	} else {
		parentQueue->add( &entry );
	}
	assert( parentQueue->getNextEventTime() <= newTime );
}

// ---------------------------------------------------------------------------
void Event::dequeue() throw() {
	if( isInQueue() )
		parentQueue->remove( queueIndex );
}

} // namespace
