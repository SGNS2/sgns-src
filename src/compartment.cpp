
// See compartment.h for a description of the contents of this file.

#include "stdafx.h"

#include <algorithm>

#include "compartment.h"
#include "simulation.h"

#ifdef _WIN32
#pragma warning( disable: 4355 ) // this used in initializer list
#endif

namespace sgns2
{

// ---------------------------------------------------------------------------
Compartment::Compartment( SimulationInstance *sim, uint initialChemicalCount )
: MarkovUmbrellaReactionInstance<NullStoich>( sim->getSimEventQueue(), NullStoich() )
, sim(sim), X(NULL), dependencies(NULL), chemicalCount(0), removedDepCount(0)
, waitList( this )
{
	if( initialChemicalCount )
		setChemicalCount( initialChemicalCount );
}

// ---------------------------------------------------------------------------
Compartment::~Compartment() throw() {
	setChemicalCount(0); // Frees all memory used by the compartment
}

// ---------------------------------------------------------------------------
void Compartment::setChemicalCount( uint newCount ) {
	// Changes the number of chemicals in the compartment
	// Carries over the dependencies from the previous set of chemicals
	// in the compartment
	// Destroys the dependencies for the chemical indices that are removed

	if( chemicalCount ) {
		if( newCount == 0 ) {
			// Destroy the species
			delete[] X;
			delete[] dependencies;
			newDeps.clear();
		} else {
			// Add extra species on the end
			PopAndDepOffset *newX = new PopAndDepOffset[newCount];
			uint i, n = std::min( chemicalCount, newCount );
			for( i = 0; i < n; i++ ) {
				newX[i].pop = X[i].pop;
				newX[i].depEnd = X[i].depEnd;
			}
			for( ; i < newCount; i++ ) {
				newX[i].pop = 0;
				newX[i].depEnd = newX[i-1].depEnd;
			}
		}
	} else if( newCount ) {
		// Construct from nothing
		X = new PopAndDepOffset[newCount];
		dependencies = NULL;
		for( uint i = 0; i < newCount; i++ ) {
			X[i].pop = 0;
			X[i].depEnd = 0;
		}
	}
	chemicalCount = newCount;
}

// ---------------------------------------------------------------------------
void Compartment::removeDependency( uint index, ReactionInstance *reaction ) {
	// Flags a dependency to be removed

	uint i = 0;
	if( index )
		i = X[index-1].depEnd;
	for( ; i < X[index].depEnd; i++ ) {
		if( dependencies[i] == reaction ) {
			dependencies[i] = NULL;
			if( removedDepCount == 0 && newDeps.size() == 0 )
				removedDepCount++;
			return;
		}
	}
}

// ---------------------------------------------------------------------------
void Compartment::triggerUpdate( uint index ) {
	// Call popUpdate for all reactions dependent on the given reactant

	uint i = 0;
	if( index )
		i = X[index-1].depEnd;
	uint last = X[index].depEnd;
	for( ; i < last; i++ )
		dependencies[i]->popUpdate(0);
}

// ---------------------------------------------------------------------------
void Compartment::rebuildDependencies() {
	// Regenerate the dependency list

	if( chemicalCount == 0 )
		return; // No chemicals.. no dependencies

	// Allocate the total amount of space required
	size_t newDepCount = static_cast<uint>(X[chemicalCount-1].depEnd + newDeps.size() - removedDepCount);
	ReactionInstance **newDepArray = new ReactionInstance*[newDepCount];

	// Sort by species index
	std::sort( newDeps.begin(), newDeps.end() );

	uint destDep = 0, origDep = 0, newDepI = 0;
	for( uint i = 0; i < chemicalCount; i++ ) {
		// Copy over the old dependencies
		while( origDep < X[i].depEnd ) {
			if( dependencies[origDep] )
				newDepArray[destDep++] = dependencies[origDep];
			origDep++;
		}
		// Put in new dependencies
		while( newDepI < newDeps.size() && newDeps[newDepI].index == i )
			newDepArray[destDep++] = newDeps[newDepI++].reaction;
		X[i].depEnd = destDep;
	}

	delete[] dependencies;
	dependencies = newDepArray;
	removedDepCount = 0;
	newDeps.clear();
}

}
