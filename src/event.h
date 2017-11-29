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

/* event.h/cpp

EventQueueBase class contents:
	- Abstract class representing a priority queue
	- Stores the 'current time' for the queue

EventQueue_BinaryHeap class contents:
	- Binary Heap implementation of EventQueueBase

Event class contents:
	- Base class for any event which is inserted into an EventQueue
	- Keeps track of its index in its containing queue

EventStream class contents:
	- Base class for a recurring event

EventStreamQueue class contents:
	- EventQueue specialized for containing EventStreams

*/

#ifndef EVENTQUEUE_H
#define EVENTQUEUE_H

#include "simtypes.h"
#include "simplesll.h"
#include <limits>

namespace sgns2
{

class Event;

// ===========================================================================
class EventQueueBase {
public:
	EventQueueBase() throw();
	virtual ~EventQueueBase() throw();

	// Base time management
	inline double getBaseTime() const { return baseTime; }
	inline void setBaseTime( double t ) { baseTime = t; }
	virtual double getUpdatedBaseTime() { return getBaseTime(); }

private:
	// The base time is the current time in each event queue
	// It's used to have different parts of the simulation run
	// at different relative speeds (see the Umbrella Reaction)
	double baseTime;

protected:
	static void SGNS_FASTCALL empty_newMin( EventQueueBase *heap );

	void (SGNS_FASTCALL *newMin)( EventQueueBase *heap );
};

// ===========================================================================
class EventQueue_BinaryHeap : public EventQueueBase {
	friend class Event;
public:
	EventQueue_BinaryHeap() throw();
	virtual ~EventQueue_BinaryHeap() throw();

	// Access the next event's time, assumes !isEmpty()
	inline double getNextEventTimeRaw() const throw() { return heap[1].time; }
	// Access to the next event's time
	inline double getNextEventTime() const throw() {
		return isEmpty() ? std::numeric_limits<double>::infinity() : getNextEventTimeRaw(); }
	// Peek at the next event's stream, assumes !isEmpty()
	inline Event *getNextEvent() const throw() { return heap[1].evt; }
	// Is the wait list empty?
	inline bool isEmpty() const throw() { return heapSize == 1; }

private:
	struct EventQueueEntry {
		double time;
		Event *evt;
	};

	// Adds an event stream
	inline void add( EventQueueEntry *entry ) throw();
	// Removes an event stream
	inline void remove( uint i ) throw();
	// Resize the heap to fit more entries
	void resize( uint newCapacity ) throw();
	// Move the entry up or down in the heap, depending on what is at i
	void bubbleAround( EventQueueEntry *entry, uint i ) throw();
	// Move the entry up in the heap from i
	void bubbleUp( EventQueueEntry *entry, uint i ) throw();
	// Move the entry down in the heap from i
	void bubbleDown( EventQueueEntry *entry, uint i ) throw();

	uint heapSize;
	uint heapCapacity;
	EventQueueEntry *heap;
};

typedef EventQueue_BinaryHeap EventQueue;

// ===========================================================================
class Event {
	friend class EventQueue_BinaryHeap;
	friend class EventQueue_BinomialHeap;
public:
	explicit Event( EventQueue *parent ) throw();
	~Event() throw();

	// Get the time of the next event in the stream
	inline double getNextEventTime() throw() {
		return isInQueue() ? getNextEventTimeRaw() : std::numeric_limits<double>::infinity(); }

protected:
	// Place the event stream at a particular point in the queue
	// Bubbles the stream up or down if it is already in the queue
	void enqueue( double newTime ) throw();
	// Remove the event stream from the queue
	void dequeue() throw();
	// Is the event stream in the queue?
	inline bool isInQueue() const throw() { return queueIndex > 0; }
	// Change the event queue this is a part of
	inline void changeEventQueue( EventQueue *newParent ) throw() {
		dequeue(); parentQueue = newParent; }
	// Get the queue that this event is a part of
	inline EventQueue *getQueue() { return parentQueue; }

private:
	inline double getNextEventTimeRaw() throw();

	// The current index in the parent queue
	uint queueIndex;
	// The EventQueue this event is a part of
	EventQueue *parentQueue;
};

// ---------------------------------------------------------------------------
inline double Event::getNextEventTimeRaw() throw() {
	return parentQueue->heap[queueIndex].time;
}

// ---------------------------------------------------------------------------
inline void EventQueue_BinaryHeap::add( EventQueueEntry *entry ) throw() {
	if( heapSize >= heapCapacity ) {
		resize( heapCapacity << 1 );
	} else if( heapSize == 1 ) {
		// New root
		heap[1] = *entry;
		entry->evt->queueIndex = 1;
		heapSize++;

		newMin( this );
		return;
	}
	heapSize++;
	bubbleUp( entry, heapSize-1 );
}

// ---------------------------------------------------------------------------
inline void EventQueue_BinaryHeap::remove( uint i ) throw() {
	heap[i].evt->queueIndex = 0;
	heapSize--;
	if( !isEmpty() ) {
		bubbleAround( &heap[heapSize], i );
	} else {
		newMin( this );
	}
}

// ===========================================================================
class EventStreamQueue;
class EventStream : public Event {
public:
	explicit EventStream( EventQueue *parent ) throw()
		: Event( parent ) { }
	virtual ~EventStream() throw() { };

	// Triggers the event
	virtual void trigger() = 0;
	// Called after triggering the event
	virtual void update() = 0;

	// Schedules the EventStream for an update() call
	inline void scheduleForUpdate();
};

// ===========================================================================
class EventStreamQueue : public EventQueue {
public:
	EventStreamQueue( SimpleSLL *updateQueue ) throw()
		: toUpdate(updateQueue)
	{ }
	~EventStreamQueue() throw() { }

	inline void addToUpdate( EventStream *s ) {
		// EventStreams to update
		toUpdate->push_back( s );
	}

	inline SimpleSLL *getUpdateQueue() const { return toUpdate; }

	inline EventStream *getNextEvent() const throw() {
		return static_cast<EventStream*>(EventQueue::getNextEvent());
	}

protected:
	SimpleSLL *toUpdate;
};

// ---------------------------------------------------------------------------
inline void EventStream::scheduleForUpdate() {
	static_cast<EventStreamQueue*>(getQueue())->addToUpdate( this );
}

} // namespace

#endif
