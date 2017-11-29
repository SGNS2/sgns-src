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

/* reactioninstance.h

ReactionInstance class contents:
	- Abstract base class for the run-time description of reactions

ReactionStoichInstance class contents:
	- Implementation of ReactionInstance
	- Manages the next reaction firing time
	- Contains a Stoichiometry object which determines how the propensity of the
	  reaction is calculated and what happens when the reaction is executed
	- Contains a Tau object which determines how the next reaction time
	  is calculated from the propensity

FireOnceReactionInstance class contents:
	- Specialization of ReactionStoichInstance for reactions which will not
	  exist after they are executed (e.g. compartment destruction)

MarkovTau class contents:
	- tau-distribution class for ReactionStoichInstance
	- Generates a tau-distribution with constant probability per unit time (exponential)

InstantTau class contents:
	- tau-distribution class for ReactionStoichInstance
	- For reactions which occur instantaneously once
	  their propensity function becomes positive

MarkovUmbrellaReactionInstance class contents:
	- Implementation of ReactionInstance
	- Manages the next reaction firing time for a group of sub-reactions, scaled
	  by the propensity produced by a Stoichiometry object

NullStoich class contents:
	- Empty Stoichiometry object for use in MarkovUmbrellaReactionInstance
*/

#ifndef REACTIONINSTANCE_H
#define REACTIONINSTANCE_H

#include <cassert>
#include <climits>
#include <cfloat>

#include "simulation.h"
#include "event.h"
#include "rng.h"

namespace sgns2
{

// ===========================================================================
class ReactionInstance : public EventStream {
public:
	explicit ReactionInstance( EventQueue *queue ) throw()
		: EventStream( queue ) { }
	virtual ~ReactionInstance() throw() { }

	// Additional functions expected of a reaction EventStream
	virtual void begin() throw() = 0;
	virtual void popUpdate( uint cookie ) throw() = 0;
};


template< typename Stoichiometry >
class MarkovTau;

// ===========================================================================
template< typename Stoichiometry, typename Tau = MarkovTau<Stoichiometry> >
class ReactionStoichInstance : public ReactionInstance {
public:
	typedef Stoichiometry Stoich;

	ReactionStoichInstance( EventQueue *q, const Stoichiometry &stoich ) throw()
		: ReactionInstance( q ), stoich(stoich) { }
	virtual ~ReactionStoichInstance() throw() {
		stoich.destroy( this );
	}

	// Enqueues the reaction for the first time
	virtual void begin() throw() {
		updSelf = false;
		enqueue( tau.newNextTime( getQueue()->getUpdatedBaseTime(), stoich ) );
		assert( getNextEventTime() >= getQueue()->getBaseTime() );
	}
	// Performs the reaction step
	virtual void trigger() throw() {
		// Perform the reaction
		updSelf = true;
		stoich.doReaction();
		enqueue( tau.newNextTime( getQueue()->getBaseTime(), stoich ) );
		assert( getNextEventTime() >= getQueue()->getBaseTime() );
		updSelf = false;

		stoich.doReactionExtra();
	}
	// Updates the propensity of the reaction and the next time
	virtual void popUpdate( uint ) throw() {
		if( !updSelf ) {
			updSelf = true;
			scheduleForUpdate();
		}
	}

	// Update the next time based on the changed propensities
	virtual void update() throw() {
		updSelf = false;
		enqueue( tau.updateNextTime( getQueue()->getUpdatedBaseTime(), stoich ) );
		assert( getNextEventTime() >= getQueue()->getBaseTime() );
	}

	// To be created in Stoichiometry
	//inline double calcMarkovA() throw() { assert( false ); } // Expected by MarkovTau
	//inline void doReaction() throw() { assert( false ); } // Expected by trigger()
	//inline void doReactionExtra() throw() { assert( false ); } // Expected by trigger()
	//inline RNG::RNG *getRNG() throw() { assert( false ); } // Expected

protected:
	Stoichiometry stoich;
	Tau tau; // Calculation of the next reaction time
	bool updSelf;
};

// ===========================================================================
// This reaction does not re-enqueue itself after firing - for use by
// reactions that destroy themselves
template< typename Stoichiometry, typename Tau = MarkovTau<Stoichiometry> >
class FireOnceReactionInstance : public ReactionStoichInstance<Stoichiometry, Tau> {
public:
	FireOnceReactionInstance( EventQueue *q, const Stoichiometry &stoich ) throw()
		: ReactionStoichInstance<Stoichiometry, Tau>( q, stoich ) { }
	virtual ~FireOnceReactionInstance() throw() { }

	virtual void trigger() throw() {
		// Perform the reaction
		ReactionStoichInstance<Stoichiometry, Tau>::updSelf = true;
		ReactionStoichInstance<Stoichiometry, Tau>::stoich.doReaction();
		ReactionStoichInstance<Stoichiometry, Tau>::stoich.doReactionExtra();
	}
};

// ===========================================================================
// Tau generator for Markov processes
template< typename Stoichiometry >
class MarkovTau {
public:
	MarkovTau() throw() : oldA(0.0), nextT(0.0) { }
	~MarkovTau() throw() { }

	// Update the time based on the old time
	inline double updateNextTime( double t, Stoichiometry &stoich ) throw() {
		if( oldA > 0.0 ) {
			// Old propensity was non-zero
			// Update the time as in step 5 of Gibson and Bruck (1999)
			double newA = stoich.calcMarkovA();
			nextT = t + (nextT - t + DBL_MIN) * oldA / newA;
			oldA = newA;
			return nextT;
		}
		// Old propensity was 0 or NaN.. need to use a new random number
		return newNextTime( t, stoich );
	}

	// Generate a new time
	inline double newNextTime( double t, Stoichiometry &stoich ) throw() {
		oldA = stoich.calcMarkovA();
		if( oldA > 0.0 )
			return nextT = t + stoich.getRNG()->exponential( oldA );
		return nextT = std::numeric_limits<double>::infinity();
	}

private:
	double oldA; // The propensity at the last update
	double nextT; // The current next reaction time
};

// ===========================================================================
// Tau generator for instantaneous reactions
template< typename Stoichiometry >
class InstantTau {
public:
	InstantTau() throw() { }
	~InstantTau() throw() { }

	// Update the time based on the old time
	inline double updateNextTime( double t, RNG::RNG *rng, const Stoichiometry &stoich ) throw() {
		return newNextTime( t, stoich );
	}

	// Generate a new time
	inline double newNextTime( double t, RNG::RNG *rng, const Stoichiometry &stoich ) throw() {
		return stoich.calcMarkovA() > 0.0 ? t : std::numeric_limits<double>::infinity();
	}
};

// ===========================================================================
// Umbrella Reaction
template< typename Stoichiometry >
class MarkovUmbrellaReactionInstance : public ReactionInstance, public EventStreamQueue {
public:
	typedef Stoichiometry Stoich;

	MarkovUmbrellaReactionInstance( EventQueue *q, const Stoichiometry &stoich )
		: ReactionInstance(q), EventStreamQueue(static_cast<EventStreamQueue*>(q)->getUpdateQueue())
		, stoich(stoich) { newMin = &newMinHeap; }
	virtual ~MarkovUmbrellaReactionInstance() throw() {
		stoich.destroy( this );
	}

	// Enqueues the reaction for the first time
	virtual void begin() throw() {
		updSelf = false;
		lastBaseT = getQueue()->getUpdatedBaseTime();
		setBaseTime( lastBaseT );
		oldA = stoich.calcMarkovA();
		assert( !(oldA < 0.0) );
		enqueue( std::numeric_limits<double>::infinity() );
	}

	// Performs the reaction and update steps
	virtual void trigger() throw() {
		//adjustTime();
		lastBaseT = getQueue()->getBaseTime();
		setBaseTime( EventStreamQueue::getNextEventTime() );

		// Perform the reaction
		stoich.doReaction();
		stoich.doReactionExtra();

		// Simply perform the next event in this queue's list
		// The bubble up/down of the event will call newMinHeap,
		// keeping the umbrella up-to-date
		((EventStream*)getNextEvent())->trigger();
	}

	// Updates the propensity of the reaction and the next time
	virtual void popUpdate( uint ) throw() {
		if( !updSelf ) {
			updSelf = true;
			scheduleForUpdate();
		}
	}

	virtual void update() throw() {
		updSelf = false;
		adjustTime();
		oldA = stoich.calcMarkovA();
		assert( !(oldA < 0.0) );
		reenqueue();
	}

	virtual double getUpdatedBaseTime() throw() {
		adjustTime();
		return getBaseTime();
	}

protected:
	inline void adjustTime() throw() {
		// Move time forward in our little world
		double curBaseT = getQueue()->getUpdatedBaseTime();
		assert( curBaseT >= lastBaseT && !(getBaseTime() + (curBaseT - lastBaseT) * oldA > EventStreamQueue::getNextEventTime()) );
		if( oldA > 0.0 )
			setBaseTime( getBaseTime() + (curBaseT - lastBaseT) * oldA );
		assert( EventStreamQueue::getNextEventTime() >= getBaseTime() );
		lastBaseT = curBaseT;
	}
	inline void reenqueue() throw() {
		// Calculate the next adjusted event time
		if( oldA > 0.0 ) {
			double dt = EventStreamQueue::getNextEventTime() - getBaseTime();
			assert( dt >= 0.0 );
			enqueue( lastBaseT + dt / oldA );
		} else {
			enqueue( std::numeric_limits<double>::infinity() );
		}
	}
	inline void newMinHeap_inl() throw() {
		// Something happened, changing the times of the events
		// under the umbrella. A new event is now at the front.
		assert( EventStreamQueue::getNextEventTime() >= getBaseTime() );
		if( !updSelf ) {
			updSelf = true;
			scheduleForUpdate();
		}
	}
	static void SGNS_FASTCALL newMinHeap( EventQueueBase *q ) throw() {
		static_cast<MarkovUmbrellaReactionInstance<Stoichiometry>*>(q)->newMinHeap_inl();
	}

	Stoichiometry stoich;
	double oldA;
	double lastBaseT;
	bool updSelf;
};

// ===========================================================================
// Dummy stoichiometry for umbrella reactions with no modification
class NullStoich {
public:
	inline NullStoich() { }
	inline ~NullStoich() throw() { }

	inline void destroy( ReactionInstance* ) throw() { }

	// Stoich functions
	inline double calcMarkovA() throw() { return 1.0; }
	inline void doReaction() throw() { }
	inline void doReactionExtra() throw() { }
};

} // namespace

#endif
