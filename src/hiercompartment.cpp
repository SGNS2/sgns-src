
// See hiercompartment.h for a description of the contents of this file.

#include "stdafx.h"

#include "hiercompartment.h"
#include "compartmenttype.h"

namespace sgns2 {

// ---------------------------------------------------------------------------
HierCompartment::HierCompartment( const CompartmentType *type, SimulationInstance *sim )
: Compartment(sim, type->getChemicalCount())
, instantiationIndex(sim->newCompartmentInstantiation())
, mainBank(NULL) // Will be filled in by CompartmentType::instantiate
, container(NULL)
, nextInContainer(NULL)
, toMeInContainer(NULL)
, firstSubCompartment(NULL)
, myType(type)
{
	begin();
	assert( type );
}

// ---------------------------------------------------------------------------
HierCompartment::~HierCompartment() throw() {
	// Remove the compartment from the container
	if( container ) {
		*toMeInContainer = nextInContainer;
		if( nextInContainer )
			nextInContainer->toMeInContainer = toMeInContainer;
	}

	// The compartment will be removed from the containing heap - don't
	// propagate timing or update changes up the heap while the
	// reactions are being destroyed
	newMin = &empty_newMin;
	removedDepCount = 1;
	SimpleSLL deadEndUpdateList;
	toUpdate = &deadEndUpdateList;

	// Destroy all subcompartments
	while( firstSubCompartment )
		delete firstSubCompartment;

	// Destroy reactions
	delete mainBank;
	getSimulation()->update();
}

// ---------------------------------------------------------------------------
void HierCompartment::orphanCompartment() {
	if( container ) {
		// Remove from old container
		*toMeInContainer = nextInContainer;
		if( nextInContainer )
			nextInContainer->toMeInContainer = toMeInContainer;
		container = NULL;

		adjustTime();
		orphanNoRelease();
	}
}

// ---------------------------------------------------------------------------
void HierCompartment::moveCompartmentInto( HierCompartment *newContainer ) {
	if( container )
		orphanCompartment();
	
	container = newContainer;
	if( container->firstSubCompartment )
		container->firstSubCompartment->toMeInContainer = &nextInContainer;
	nextInContainer = container->firstSubCompartment;
	container->firstSubCompartment = this;
	toMeInContainer = &container->firstSubCompartment;

	//adjustTime();
	myType->instantiateBankIn( this );
}

// ---------------------------------------------------------------------------
void HierCompartment::orphanNoRelease() {
	for( HierCompartment *comp = firstSubCompartment; comp; comp = comp->nextInContainer )
		comp->orphanNoRelease();

	delete mainBank;
}

} // namespace sgns2

