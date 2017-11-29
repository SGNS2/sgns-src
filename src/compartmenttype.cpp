
// See compartmenttype.h for a description of the contents of this file.

#include "stdafx.h"

#include "compartmenttype.h"
#include "reactionbank.h"
#include "hiercompartment.h"

namespace sgns2 {

// ---------------------------------------------------------------------------
CompartmentType::CompartmentType( const char *name, CompartmentType *parentType )
: name(name)
, depth(parentType ? parentType->depth + 1 : 0)
, superType(parentType)
, outputCompartment(true)
{
	reactions = new reaction::IntraBankTemplate;
}

// ---------------------------------------------------------------------------
CompartmentType::CompartmentType( const std::string &name, CompartmentType *parentType )
: name(name)
, depth(parentType ? parentType->depth + 1 : 0)
, superType(parentType)
, outputCompartment(true)
{
	reactions = new reaction::IntraBankTemplate;
}

// ---------------------------------------------------------------------------
CompartmentType::~CompartmentType() {
	delete reactions;
}

// ---------------------------------------------------------------------------
HierCompartment *CompartmentType::instantiate( HierCompartment *in ) const {
	// Instantiates a new subcompartment

	assert( superType && in && in->getType() == superType );

	HierCompartment *newInst = new HierCompartment( this, in->getSimulation() );
	newInst->moveCompartmentInto( in );

	return newInst;
}

// ---------------------------------------------------------------------------
HierCompartment *CompartmentType::instantiate( SimulationInstance *sim ) const {
	// Instantiates a new root compartment

	HierCompartment *newInst = new HierCompartment( this, sim );
	if( !superType ) {
		newInst->mainBank = reactions->instantiate( newInst, NULL );
		newInst->rebuildDependencies();
	}

	return newInst;
}

// ---------------------------------------------------------------------------
uint CompartmentType::getChemicalIndex( Chemical *chemical ) const {
	ChemicalMap::const_iterator it = chemicalIndices.find( chemical );
	if( it != chemicalIndices.end() )
		return it->second;
	return (uint)-1;
}

// ---------------------------------------------------------------------------
uint CompartmentType::getChemicalIndex( Chemical *chemical, bool add ) {
	ChemicalMap::const_iterator it = chemicalIndices.find( chemical );
	if( it != chemicalIndices.end() )
		return it->second;
	if( add ) {
		uint idx = static_cast<uint>(chemicals.size());
		chemicals.push_back( chemical );
		chemicalIndices[chemical] = idx;
		return idx;
	}
	return (uint)-1;
}

// ---------------------------------------------------------------------------
bool CompartmentType::isSubtypeOf( const CompartmentType *type ) const {
	const CompartmentType *t2 = this;
	while( t2->getDepth() > type->getDepth() )
		t2 = t2->getParentType();
	return t2 == type;
}

// ---------------------------------------------------------------------------
void CompartmentType::instantiateBankIn( HierCompartment *in ) const {
	reaction::BankInstance *parentBanks[16];
	HierCompartment *comp = in->getContainer();
	while( comp ) {
		parentBanks[comp->getType()->getDepth()] = comp->mainBank;
		comp = comp->getContainer();
	}

	in->mainBank = reactions->instantiate( in, parentBanks );
	in->rebuildDependencies();
}

} // namespace sgns2
