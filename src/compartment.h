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

/* compartment.h/cpp

Compartment class contents:
	- Stores the populations of all chemical species contained within
	- Stores the reaction dependency graph
	- Updates reactions dependent on chemicals whose population changes
*/

#ifndef COMPARTMENT_H
#define COMPARTMENT_H

#include <limits>
#include <list>
#include <vector>

#include "waitlist.h"
#include "simtypes.h"
#include "reactioninstance.h"

namespace sgns2
{

namespace reaction {
	class BankInstance;
}
class SimulationInstance;

class Compartment : public MarkovUmbrellaReactionInstance<NullStoich> {
public:
	Compartment( SimulationInstance *inst, uint initialChemicalCount );
	~Compartment() throw();

	// Sets the number of different chemical species present in the compartment
	void setChemicalCount( uint newCount );
	// Get the number of chemicals present in this compartment
	inline uint getChemicalCount() { return chemicalCount; }

	// Add a reaction dependency
	// NOTE: rebuildDependencies() MUST be called after this function is called
	// for the dependency to take effect for the next set/modifyPopulation.
	inline void addDependency( uint index, ReactionInstance *reaction ) {
		NewDependency newDep;
		newDep.index = index;
		newDep.reaction = reaction;
		newDeps.push_back( newDep );
	}
	// Remove a reaction dependency
	// NOTE: rebuildDependencies() MUST be called after this function is called
	// for the dependency to take effect for the next set/modifyPopulation.
	void removeDependency( uint index, ReactionInstance *reaction );

	// Access to the population of the chemical with the given index
	inline Population getPopulation( uint index ) { return X[index].pop; }
	// Directly set the population of a species
	// Does not trigger popUpdate calls
	inline void setPopulationNoUpdate( uint index, Population pop ) { X[index].pop = pop; }
	// Directly modify the population of a species
	// Does not trigger popUpdate calls
	inline void modifyPopulationNoUpdate( uint index, Population popDelta ) { X[index].pop += popDelta; }

	// Set the population of a species
	// Calls popUpdate for all reactions dependent on this species
	inline void setPopulation( uint index, Population pop ) {
		setPopulationNoUpdate( index, pop );
		triggerUpdate( index );
	}
	// Modify the population of a species
	// Calls popUpdate for all reactions dependent on this species
	void modifyPopulation( uint index, Population popDelta ) {
		modifyPopulationNoUpdate( index, popDelta );
		triggerUpdate( index );
	}

	// Regenerate the dependency list
	// Must be called before changing populations with update-full functions
	void rebuildDependencies();

	// Access to the compartment's wait list
	inline WaitList *getWaitList() { return &waitList; }

	// Access to the main simulation instance that this compartment is a part of
	inline SimulationInstance *getSimulation() const { return sim; }

protected:
	// Call popUpdate for all reactions dependent on the given reactant
	void triggerUpdate( uint index );

	SimulationInstance *sim; // Containing simulation

	struct PopAndDepOffset {
		Population pop; // Current Population of the chemical
		uint depEnd; // Index of the last ReactionInstance dependent on
		             // this chemical in the dependency list
	};
	PopAndDepOffset *X; // Chemical populations and dependency indices
	ReactionInstance **dependencies; // Reaction dependency list
	uint chemicalCount; // Number of chemicals in this compartment

	// Temporary structure for new dependencies
	// TODO: Remove this overhead from the dependency construction process
	struct NewDependency {
		uint index; // The chemical index
		ReactionInstance *reaction; // The newly-dependent reaction

		// operator< for sorting
		inline bool operator <( const NewDependency &rhs ) const { return index < rhs.index; }
	};
	std::vector< NewDependency > newDeps; // List of new dependencies
	uint removedDepCount; // Number of dependencies to be removed

	WaitList waitList; // The compartment's wait list
};

}

#endif
