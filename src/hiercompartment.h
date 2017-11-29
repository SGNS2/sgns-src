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

/* hiercompartment.h/cpp

HierCompartment class contents:
	- Represents a Compartment in a hierarchy of compartments (has a
	  parent, siblings and children)
	- Each HierCompartment is of a specific CompartmentType
	- Manages its CompartmentType's reaction::BankInstance
*/

#ifndef HIERCOMPARTMENT_H
#define HIERCOMPARTMENT_H

#include <cassert>

#include "compartment.h"
#include "reactionbank.h"

namespace sgns2 {

class CompartmentType;
class SimulationInstance;

class HierCompartment : public Compartment {
	friend class CompartmentType;

public:
	HierCompartment( const CompartmentType *type, SimulationInstance *sim );
	virtual ~HierCompartment() throw();

	// Completely transplants the compartment from one container to another
	void orphanCompartment();
	// Moves an orphaned compartment into newContainer
	void moveCompartmentInto( HierCompartment *newContainer );

	// Get a unique index associated with this compartment
	inline uint getInstantiationIndex() const { return instantiationIndex; }
	// Get the containing HierCompartment
	inline HierCompartment *getContainer() const { return container; }
	// Get the next sibling compartment
	inline HierCompartment *getNextInContainer() const { return nextInContainer; }
	// Get the first contained compartment
	inline HierCompartment *getFirstSubCompartment() const { return firstSubCompartment; }
	
	// For use by orphaned compartments
	inline void setNextInContainer( HierCompartment *comp ) { assert( !container ); nextInContainer = comp; }
	
	// Get the type of the compartment
	inline const CompartmentType *getType() const { return myType; }
	// Get reaction bank instantiated in this compartment
	inline reaction::BankInstance *getMainReactionBank() const { return mainBank; }

private:
	// Orphans the compartment without removing it from the parent's queue
	void orphanNoRelease();

	uint instantiationIndex;
	reaction::BankInstance *mainBank;
	HierCompartment *container;
	HierCompartment *nextInContainer, **toMeInContainer;
	HierCompartment *firstSubCompartment;
	const CompartmentType *myType;
};

} // namespace sgns2

#endif // HIERCOMPARTMENT_H
