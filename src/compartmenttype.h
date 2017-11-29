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

/* compartmenttype.h/cpp

CompartmentType class contents:
	- Instantates HierCompartment's and manages the reactions created therein
	- Stores the type name and relation to parent types
	- Stores the chemical types present in compartments of this type
	- Stores whether the compartment type should be output
*/

#ifndef COMPARTMENTTYPE_H
#define COMPARTMENTTYPE_H

#include <vector>
#include <string>
#include <map>

#include "reactionbank.h"

namespace sgns2 {

class Chemical;
class HierCompartment;
class SimulationInstance;

class CompartmentType {
	friend class HierCompartment; // Access to instantiateBankIn

public:
	// Create a new compartment type with the given name
	CompartmentType( const char *name, CompartmentType *parentType = NULL );
	CompartmentType( const std::string &name, CompartmentType *parentType = NULL );
	~CompartmentType();

	// Access to the BankTemplate with reactions that will be
	// instantiated in all compartments of this type
	reaction::IntraBankTemplate *getBank() const { return reactions; }

	// Instantiates a contained compartment in the given container
	HierCompartment *instantiate( HierCompartment *in ) const;
	// Instantiates an orphan compartment
	// (will contain no reactions unless this type has no parent)
	HierCompartment *instantiate( SimulationInstance *sim ) const;

	// Returns the indec of the given chemical in the compartment type
	uint getChemicalIndex( Chemical *chemical ) const;
	// Returns the indec of the given chemical in the compartment type
	// When add is true, if the chemical does not yet exist in the
	// compartment, it will be added
	uint getChemicalIndex( Chemical *chemical, bool add = false );
	// Get the depth of the type in the hierarchy (the number of ancestors
	// this compartment type has)
	inline uint getDepth() const { return depth; }
	// Returns the number of different chemicals in this compartment type
	inline uint getChemicalCount() const { return static_cast<uint>(chemicals.size()); }
	// Returns the chemical type at a given index in this compartment type
	inline Chemical *getChemicalAtIndex( uint idx ) const { return chemicals[idx]; }
	// Get the containing compartment type
	inline CompartmentType *getParentType() const { return superType; }

	// Tests whether this compartment is a subtype (or the same type) as type
	bool isSubtypeOf( const CompartmentType *type ) const;

	// Should output files be created for compartments of this type
	inline bool shouldOutput() const { return outputCompartment; }
	// Set whether compartments of this type should be output
	inline void setOutput( bool output ) { outputCompartment = output; }

	// Access to the compartment type's name
	inline const std::string &getName() const { return name; }

private:
	// Instantiates the reaction bank in the target compartment
	void instantiateBankIn( HierCompartment *in ) const;

	std::string name; // Type name
	uint depth; // Depth of the compartment type
	CompartmentType *superType; // Parent compartment type
	// Bank of reactions to instantiate in all compartments of this type
	reaction::IntraBankTemplate *reactions;
	std::vector< Chemical* > chemicals; // List of chemicals in this compartment type
	typedef std::map< Chemical*, uint > ChemicalMap;
	ChemicalMap chemicalIndices; // Chemical -> index map
	bool outputCompartment; // Should this compartment type be output?
};

} // namespace sgns2

#endif // COMPARTMENTTYPE_H
