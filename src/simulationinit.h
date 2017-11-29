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

/* simulationinit.h/cpp

Contents:
	- Initialization classes for a SGNS simulation

Command class contents:
	- Abstract base class for all initialization commands

SelectEnv class contents:
	- Sets the working compartment set to be only Env

SelectCompartmentType class contents:
	- Sets the working compartment set to be the children of the current
	  working set which are of a specific type

SelectCompartment class contents:
	- Sets the working compartment set to be a compartment from the named
	  compartment list

InstantiateNamedCompartment class contents:
	- Creates a compartment on the named compartment list

SetPopulations class contents:
	- Sets/adds a species' population in all compartments in the working set

AddToWaitList class contents:
	- Adds a delayed molecule to the wait lists of all compartments in the
	  working set

SplitPopulation class contents:
	- Splits a population, leaving some in the compartment and some in the
	  split buffer
	- The working set must be only one compartment

AddPopulationFromSplitBuffer class contents:
	- Adds molecules from the split buffer to the compartment in the
	  working set

AddToWaitListFromSplitBuffer class contents:
	- Adds molecules from the split buffer into the wait list of the
	  compartment in the working set

SplitCompartments class contents:
	- Splits the working compartment set, placing some in a list in the
	  split buffer

InsertSplitCompartments class contents:
	- Adds a split compartment list from the split buffer into the
	  compartment in the working set

DeleteCompartments class contents:
	- Deletes all compartments in the working set

UpdateSimulation class contents:
	- Update()s the simulation

*/

#ifndef SIMULATIONINIT_H
#define SIMULATIONINIT_H

#include <list>
#include <vector>

#include "distribution.h"
#include "hiercompartment.h"
#include "simulation.h"
#include "split.h"

namespace sgns2 {

namespace init {

typedef std::list< HierCompartment* > CompartmentList;
typedef std::vector< HierCompartment* > CompartmentVector;

// ===========================================================================
struct Context {
public:
	Context( SimulationInstance *sim, CompartmentType *envType ); // For the sim setup
	Context( HierCompartment *in ); // For run-time init commands

	SimulationInstance *sim;
	CompartmentList compartments;
	HierCompartment *env;
	CompartmentVector namedCompartments;

private:
	Context();
};

// ===========================================================================
class Command {
public:
	virtual ~Command();

	virtual void execute( Context *ctx ) = 0;

protected:
	Command();
};

// ===========================================================================
class SelectEnv : public Command {
public:
	SelectEnv() { }
	virtual ~SelectEnv();

	virtual void execute( Context *ctx );
};

// ===========================================================================
class SelectCompartmentType : public Command {
public:
	SelectCompartmentType( CompartmentType *type )
		: subType(type) { }
	virtual ~SelectCompartmentType();

	virtual void execute( Context *ctx );

private:
	CompartmentType *subType;
};

// ===========================================================================
class SelectCompartment : public Command {
public:
	SelectCompartment( sgns2::uint idx );
	virtual ~SelectCompartment();

	virtual void execute( Context *ctx );

private:
	sgns2::uint namedIndex;
};

// ===========================================================================
class InstantiateNamedCompartment : public Command {
public:
	InstantiateNamedCompartment( sgns2::uint idx, const sgns2::CompartmentType *type );
	virtual ~InstantiateNamedCompartment();

	virtual void execute( Context *ctx );

private:
	sgns2::uint namedIndex;
	const sgns2::CompartmentType *type;
};

// ===========================================================================
class InstantiateCompartments : public Command {
public:
	InstantiateCompartments( CompartmentType *type, uint n );

	virtual void execute( Context *ctx );

private:
	CompartmentType *type;
	uint n;
};

// ===========================================================================
class SetPopulations : public Command {
public:
	SetPopulations( uint index, const RuntimeDistribution *distr, bool add );
	virtual ~SetPopulations();

	virtual void execute( Context *ctx );

private:
	uint index;
	RuntimeDistribution distr;
	bool add;
};


// ===========================================================================
class AddToWaitList : public Command {
public:
	AddToWaitList( uint index, const RuntimeDistribution *amount, const RuntimeDistribution *when, bool addBaseTime = false );
	virtual ~AddToWaitList();

	virtual void execute( Context *ctx );

private:
	uint index;
	RuntimeDistribution amount;
	RuntimeDistribution when;
	bool addBaseTime;
};

// ===========================================================================
class SplitPopulation : public Command {
public:
	SplitPopulation( uint chemicalIndex, uint splitIndex, const SplitFunction *split );
	virtual ~SplitPopulation();

	virtual void execute( Context *ctx );

private:
	uint chemicalIndex;
	uint splitIndex;
	SplitFunction split;
};

// ===========================================================================
class AddPopulationFromSplitBuffer : public Command {
public:
	AddPopulationFromSplitBuffer( uint chemicalIndex, uint splitIndex );
	virtual ~AddPopulationFromSplitBuffer();

	virtual void execute( Context *ctx );

private:
	uint chemicalIndex;
	uint splitIndex;
};

// ===========================================================================
class AddToWaitListFromSplitBuffer : public Command {
public:
	AddToWaitListFromSplitBuffer( uint chemicalIndex, uint splitIndex, const RuntimeDistribution *when );
	virtual ~AddToWaitListFromSplitBuffer();

	virtual void execute( Context *ctx );

private:
	uint chemicalIndex;
	uint splitIndex;
	RuntimeDistribution when;
};

// ===========================================================================
class SplitCompartments : public Command {
public:
	SplitCompartments( uint splitIndex, uint compSplitIndex, const SplitFunction *fn );
	virtual ~SplitCompartments();

	virtual void execute( Context *ctx );

private:
	uint splitIndex;
	uint compSplitIndex; // Set to (unsigned)-1 to delete immediately
	SplitFunction split;
};

// ===========================================================================
class InsertSplitCompartments : public Command {
public:
	InsertSplitCompartments( uint splitIndex );
	virtual ~InsertSplitCompartments();

	virtual void execute( Context *ctx );

private:
	uint splitIndex;
};

// ===========================================================================
class DeleteCompartments : public Command {
public:
	DeleteCompartments();
	virtual ~DeleteCompartments();

	virtual void execute( Context *ctx );
};

// ===========================================================================
class UpdateSimulation : public Command {
public:
	UpdateSimulation();
	virtual ~UpdateSimulation();

	virtual void execute( Context *ctx );
};

} // namespace init

} // namespace sgns2

#endif // SIMULATIONINIT_H
