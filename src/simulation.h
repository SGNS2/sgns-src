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

/* simulation.h/cpp

SimulationInstance class contents:
	- Main simulation class
	- Manages the main simulation queue
	- Contains an auxilliary queue for things like sampling, saving, etc..
	  (so that the simulation produces identical results, indepenedent of
	  sampling and saving)
	- Manages the update lists
*/

#ifndef SIMULATION_H
#define SIMULATION_H

#include <list>

#include "event.h"
#include "rng.h"
#include "distribution.h"

struct lua_State;

namespace sgns2
{

class SimulationInstance;
class ReactionInstance;

class SimulationInstance {
public:
	SimulationInstance( unsigned seed, lua_State *L );
	~SimulationInstance();

	// Simulation runners
	void runUntil( double time ) throw();
	inline void runFor( double deltaT ) throw() { runUntil( getTime() + deltaT ); }
	EventStream *runStep() throw();

	// Update the simulation after external changes
	void update() throw();

	// Access to the simulation queues
	inline EventStreamQueue *getSimEventQueue() { return &simQueue; }
	inline EventStreamQueue *getParallelQueue() { return &parallelQueue; }
	// Access to the last top-level event that occurred
	inline EventStream *getLastEvent() { return lastEvent; }
	// Returns the update list (the list of EventStreams that must be update()d
	// before the next simulation step
	inline SimpleSLL *getUpdateList() { return &toUpdate; }
	// Sets the current simulation time
	inline void setTime( double time ) throw() { simQueue.setBaseTime( time ); }
	// Access to the current simulation time
	inline double getTime() const throw() { return simQueue.getBaseTime(); }
	// The Distribution Context - passed to the RuntimeDistribution class to
	// generate random numbers
	inline DistributionContext *distrCtx() { return &distribContext; }
	inline const DistributionContext *distrCtx() const { return &distribContext; }
	// Access to the underlying random number generator
	inline RNG::RNG *getRNG() const throw() { return distribContext.getRNG(); }
	// Access to the runtime lua context
	inline lua_State *getL() const throw() { return L; }
	// Returns the total number of steps performed until here
	inline uint64 getStepCount() const throw() { return totalSteps; }
	// Returns a unique number every time it is called
	inline uint newCompartmentInstantiation() { return compartmentInstantiationIndex++; }

	// Reaction context allocation
	void *allocContext( const void *ctx, uint size );

private:
	// Performs a single step of the NRM
	bool internalStep() throw();

	// The DistributionContext, to be passed to RuntimeDistribution when
	// generating a sample
	DistributionContext distribContext;
	// The Lua state
	lua_State *L;
	// Simulation time at which internalStep should stop
	double stopTime;
	// Next compartment index
	uint compartmentInstantiationIndex;
	// Total number of steps performed
	uint64 totalSteps;
	// The last event that occurred
	EventStream *lastEvent;
	// The Update list
	SimpleSLL toUpdate;
	// The main simulation queue
	EventStreamQueue simQueue;
	// The parallel simulation queue (for sampling events, etc..)
	EventStreamQueue parallelQueue;
};

} // namespace

#endif //SIMULATION_H
