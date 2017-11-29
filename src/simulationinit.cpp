
// See simulationinit.h for a description of the contents of this file.

#include "stdafx.h"

#include "simulationinit.h"
#include "compartmenttype.h"

using namespace sgns2;
using namespace init;

// ===========================================================================
Context::Context( SimulationInstance *sim, CompartmentType *envType )
	: sim(sim)
{
	env = envType->instantiate( sim );
}

Context::Context( HierCompartment *in )
	: sim(in->getSimulation())
{
	namedCompartments.resize( in->getType()->getDepth() + 1, NULL );
	uint i = in->getType()->getDepth();
	do {
		namedCompartments[i--] = in;
		in = in->getContainer();
	} while( in );

	env = namedCompartments[0];
}

// ===========================================================================
Command::~Command() {
}

Command::Command() {
}

// ===========================================================================
SelectEnv::~SelectEnv()
{ 
}

void SelectEnv::execute( Context *ctx ) {
	ctx->compartments.clear();
	ctx->compartments.push_back( ctx->env );
}

// ===========================================================================
SelectCompartmentType::~SelectCompartmentType() {
}

void SelectCompartmentType::execute( Context *ctx ) {
	// Select all subcompartments of the current compartment set that are
	// of type subtype
	if( ctx->compartments.empty() )
		return;

	CompartmentList::iterator final = ctx->compartments.end();
	--final;

	while( true ) {
		
		HierCompartment *comp = ctx->compartments.front();

		for( HierCompartment *subComp = comp->getFirstSubCompartment(); subComp; subComp = subComp->getNextInContainer() ) {
			if( subComp->getType() == subType )
				ctx->compartments.push_back( subComp );
		}

		if( ctx->compartments.begin() == final )
			break;

		ctx->compartments.pop_front();
	}

	ctx->compartments.pop_front();
}

// ===========================================================================
SelectCompartment::SelectCompartment( sgns2::uint idx )
: namedIndex(idx)
{
}

SelectCompartment::~SelectCompartment() {
}

void SelectCompartment::execute( Context *ctx ) {
	ctx->compartments.clear();
	ctx->compartments.push_back( ctx->namedCompartments[namedIndex] );
}

// ===========================================================================
InstantiateNamedCompartment::InstantiateNamedCompartment( sgns2::uint idx, const sgns2::CompartmentType *type )
: namedIndex(idx)
, type(type)
{
}

InstantiateNamedCompartment::~InstantiateNamedCompartment() {
}

// ===========================================================================
void InstantiateNamedCompartment::execute( Context *ctx ) {
	HierCompartment *container = ctx->compartments.front();
	HierCompartment *newComp = type->instantiate( container );
	if( ctx->namedCompartments.size() == namedIndex ) {
		ctx->namedCompartments.push_back( newComp );
	} else {
		ctx->namedCompartments[namedIndex] = newComp;
	}
}

InstantiateCompartments::InstantiateCompartments( CompartmentType *type, uint n )
: type(type), n(n)
{
}

void InstantiateCompartments::execute( Context *ctx ) {
	for( CompartmentList::const_iterator it = ctx->compartments.begin(); it != ctx->compartments.end(); ++it ) {
		for( uint i = 0; i < n; i++ )
			type->instantiate( *it );
	}
}

// ===========================================================================
SetPopulations::SetPopulations( uint index, const sgns2::RuntimeDistribution *distr, bool add )
: index(index), distr(*distr), add(add)
{
}

SetPopulations::~SetPopulations() {
}

void SetPopulations::execute( Context *ctx ) {
	for( CompartmentList::const_iterator it = ctx->compartments.begin(); it != ctx->compartments.end(); ++it ) {
		sgns2::Population n = (sgns2::Population)floor( distr.sample( ctx->sim->distrCtx() ) );
		if( add ) {
			(*it)->modifyPopulation( index, n );
		} else {
			(*it)->setPopulation( index, n );
		}
	}
}

// ===========================================================================
AddToWaitList::AddToWaitList( uint index, const sgns2::RuntimeDistribution *amount, const sgns2::RuntimeDistribution *when, bool addBaseTime )
: index(index)
, amount(*amount)
, when(*when)
, addBaseTime(addBaseTime)
{
}

AddToWaitList::~AddToWaitList() {
}

void AddToWaitList::execute( Context *ctx ) {
	double baseTime = addBaseTime ? ctx->sim->getTime() : 0.0;
	for( CompartmentList::const_iterator it = ctx->compartments.begin(); it != ctx->compartments.end(); ++it ) {
		sgns2::Population n = (sgns2::Population)floor( amount.sample( ctx->sim->distrCtx() ) );
		(*it)->getWaitList()->releaseAt( baseTime + when.sample( ctx->sim->distrCtx() ), index, n );
	}
}

// ===========================================================================
SplitPopulation::SplitPopulation( uint chemicalIndex, uint splitIndex, const SplitFunction *split )
: chemicalIndex(chemicalIndex), splitIndex(splitIndex), split(*split)
{
}

SplitPopulation::~SplitPopulation() {
}

void SplitPopulation::execute( Context *ctx ) {
	Population N = 0;
	for( CompartmentList::const_iterator it = ctx->compartments.begin(); it != ctx->compartments.end(); ++it ) {
		Population X[2];
		X[0] = (*it)->getPopulation( chemicalIndex );

		split.split( &X[0], ctx->sim->distrCtx() );

		(*it)->setPopulation( chemicalIndex, X[0] );
		N += X[1];
	}
	ctx->sim->distrCtx()->getSplitBuffer()[splitIndex] = N;
}

// ===========================================================================
AddPopulationFromSplitBuffer::AddPopulationFromSplitBuffer( uint chemicalIndex, uint splitIndex )
: chemicalIndex(chemicalIndex), splitIndex(splitIndex)
{
}

AddPopulationFromSplitBuffer::~AddPopulationFromSplitBuffer() {
}

void AddPopulationFromSplitBuffer::execute( Context *ctx ) {
	Population pop = ctx->sim->distrCtx()->getSplitBuffer()[splitIndex];
	for( CompartmentList::const_iterator it = ctx->compartments.begin(); it != ctx->compartments.end(); ++it )
		(*it)->modifyPopulation( chemicalIndex, pop );
}

// ===========================================================================
AddToWaitListFromSplitBuffer::AddToWaitListFromSplitBuffer( uint chemicalIndex, uint splitIndex, const RuntimeDistribution *when )
: chemicalIndex(chemicalIndex), splitIndex(splitIndex), when(*when)
{
}

AddToWaitListFromSplitBuffer::~AddToWaitListFromSplitBuffer() {
}

void AddToWaitListFromSplitBuffer::execute( Context *ctx ) {
	Population pop = ctx->sim->distrCtx()->getSplitBuffer()[splitIndex];
	double baseTime = ctx->sim->getTime();
	for( CompartmentList::const_iterator it = ctx->compartments.begin(); it != ctx->compartments.end(); ++it ) {
		(*it)->getWaitList()->releaseAt( baseTime + when.sample( ctx->sim->distrCtx() ), chemicalIndex, pop );
	}
}

// ===========================================================================
SplitCompartments::SplitCompartments( uint splitIndex, uint compSplitIndex, const SplitFunction *fn )
: splitIndex(splitIndex), compSplitIndex(compSplitIndex), split(*fn)
{
}

SplitCompartments::~SplitCompartments() {
}

void SplitCompartments::execute( Context *ctx ) {
	// Clear the update list so that we can safely break reaction banks
	ctx->sim->update();

	Population N = ctx->compartments.size(), X[2];
	X[0] = N;
	split.split( &X[0], ctx->sim->distrCtx() );
	ctx->sim->distrCtx()->getSplitBuffer()[splitIndex] = X[1];

	HierCompartment *lastCompSplit = NULL;
	if( X[0] < N ) {
		X[1] = N - X[0];

		// Selects and orphans a random subset of X[1] of the N compartments
		for( CompartmentList::const_iterator it = ctx->compartments.begin(); it != ctx->compartments.end(); ++it ) {
			if( (Population)(ctx->sim->getRNG()->rand_int32() % N) < X[1] ) {
				if( compSplitIndex == (uint)-1 ) {
					delete *it;
				} else {
					(*it)->orphanCompartment();
					(*it)->setNextInContainer( lastCompSplit );
					lastCompSplit = *it;
				}
				X[1]--;
			}
			N--;
		}
		ctx->compartments.clear();
	}

	if( compSplitIndex != (uint)-1 ) {
		*reinterpret_cast<HierCompartment**>(&ctx->sim->distrCtx()->getSplitBuffer()[compSplitIndex]) = lastCompSplit;
	}
}

// ===========================================================================
InsertSplitCompartments::InsertSplitCompartments( uint splitIndex )
: splitIndex(splitIndex)
{
}

InsertSplitCompartments::~InsertSplitCompartments() {
}

void InsertSplitCompartments::execute( Context *ctx ) {
	HierCompartment *comp = *reinterpret_cast<HierCompartment**>(&ctx->sim->distrCtx()->getSplitBuffer()[splitIndex]);
	while( comp ) {
		HierCompartment *nextComp = comp->getNextInContainer();
		comp->moveCompartmentInto( ctx->compartments.front() );
		comp = nextComp;
	}
}

// ===========================================================================
DeleteCompartments::DeleteCompartments() {
}

DeleteCompartments::~DeleteCompartments() {
}

void DeleteCompartments::execute( Context *ctx ) {
	// Clear the update list so that we can safely destroy compartments
	ctx->sim->update();

	// Destroy the working set of compartments
	for( CompartmentList::const_iterator it = ctx->compartments.begin(); it != ctx->compartments.end(); ++it )
		delete *it;
	ctx->compartments.clear();
}

// ===========================================================================
UpdateSimulation::UpdateSimulation()
{
}

UpdateSimulation::~UpdateSimulation() {
}

void UpdateSimulation::execute( Context *ctx ) {
	ctx->sim->update();
}
