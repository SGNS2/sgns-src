
// See simulation.h for a description of the contents of this file.

#include "stdafx.h"

#include "simulation.h"
#include "reactioninstance.h"
#include "event.h"

#include <limits>

#ifdef _WIN32
#pragma warning( disable: 4355 ) // this used in initializer list
#endif

namespace sgns2 {

// ---------------------------------------------------------------------------
SimulationInstance::SimulationInstance( unsigned seed, lua_State *L )
: distribContext(this, seed)
, L(L)
, stopTime(0.0)
, compartmentInstantiationIndex(0)
, totalSteps(0)
, lastEvent(NULL)
, simQueue(&toUpdate)
, parallelQueue(&toUpdate)
{
}

// ---------------------------------------------------------------------------
SimulationInstance::~SimulationInstance() {
}

// ---------------------------------------------------------------------------
void SimulationInstance::runUntil( double time ) throw() {
	stopTime = time;
	while( internalStep() ) { }
}

// ---------------------------------------------------------------------------
EventStream *SimulationInstance::runStep() throw() {
	stopTime = std::numeric_limits<double>::infinity();
	return internalStep() ? lastEvent : NULL;
}

// ---------------------------------------------------------------------------
void SimulationInstance::update() throw() {
	while( !toUpdate.empty() )
		toUpdate.pop_front<EventStream*>()->update();
}

// ---------------------------------------------------------------------------
bool SimulationInstance::internalStep() throw() {
	double simTime = getSimEventQueue()->getNextEventTime();
	double parTime = getParallelQueue()->getNextEventTime();
	assert( simTime >= getSimEventQueue()->getBaseTime() );
	assert( parTime >= getSimEventQueue()->getBaseTime() );

	/*
	Major Simulation Steps:

	Selection
	Execution
	Update
	*/

	if( parTime > simTime ) {
		// Main simulation queue is first..
		if( simTime <= stopTime ) {
			// Move the simulation time and do the event
			totalSteps++;
			getSimEventQueue()->setBaseTime( simTime );

			// Selection
			lastEvent = getSimEventQueue()->getNextEvent();
			// Execution
			lastEvent->trigger();
			// Update
			update();
			return true;
		}
	} else if( stopTime >= parTime ) {
		// Parallel queue is first..
		getParallelQueue()->setBaseTime( parTime );
		getSimEventQueue()->setBaseTime( parTime );
		lastEvent = getParallelQueue()->getNextEvent();
		lastEvent->trigger();
		// Update
		update();
		return true;
	}

	// Stop time is first
	getSimEventQueue()->setBaseTime( stopTime );
	return false;
}

} // namespace
