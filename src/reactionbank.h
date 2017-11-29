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

/* reactionbank.h/cpp

BankTemplate class contents:
	- Base class for reaction bank templates
	- Keeps track of how many instances of that bank there are
	- Must be sealed (no new reactions can be added) before instantiation

BankInstance class contents:
	- Base class for an instance of a BankTemplate
	- Stores the list of ReactionInstances in the bank

IntraBankTemplate class contents:
	- Contains the collection of reactions that can occur within a
	  single compartment (some may be Umbrella reactions and therefore can
	  span multiple compartments)
*/

#ifndef REACTIONBANK_H
#define REACTIONBANK_H

#include "reactioninstance.h"
#include "reaction.h"
#include "rng.h"
#include "simplesll.h"

namespace sgns2 {
namespace reaction {

// ===========================================================================
class BankTemplate {
	friend class BankInstance;
public:
	virtual ~BankTemplate();

	// Stops any new reactions from being created in the bank template
	void seal();
	inline bool isSealed() const { return instances != (uint)-1; }

	// Instance destruction
	virtual void destroyInstance( BankInstance *inst ) = 0;

protected:
	BankTemplate();

	// Number of active instances of the template
	uint instances;
};

// ===========================================================================
class BankInstance {
	friend class IntraBankTemplate;
public:
	~BankInstance();

	// Access to an individual ReactionInstance
	ReactionInstance *getReactionInstance( uint index ) { return instances[index]; }

private:
	// To be called by one of the friend classes
	BankInstance();

	// The template upon which this instance is based
	BankTemplate *tmplate;
	// The list of ReactionInstances in this bank
	ReactionInstance **instances;
};

// ===========================================================================
class IntraBankTemplate : public BankTemplate {
public:
	IntraBankTemplate();
	virtual ~IntraBankTemplate();
	
	// Creates a new instance of the bank template in the given compartment
	// Context is used for umbrellaed reactions and can be NULL if there are none
	BankInstance *instantiate( Compartment *in, BankInstance **context = NULL );
	// Destroys all reactions in the instance. inst must have been created by instantiate
	virtual void destroyInstance( BankInstance *inst );

	// Access to the number of reactions in this bank
	inline uint getReactionCount() { return (uint)templates.size(); }
	// Create a new reaction in this bank
	// Returns the new reaction's index
	uint createReaction( uint parentBank = 0, uint umbrellaId = (uint)-1, bool umbrella = false, bool fireOnce = false );
	// Access to a given reaction's template
	inline Template *getReactionTemplate( uint id ) { return &templates[id].tmplate; }

private:
	struct TargettedTemplate {
		TargettedTemplate( uint umbrellaId, uint parentBankId, bool isUmbrella, bool fireOnce )
			: parentBankId(parentBankId), umbrellaId(umbrellaId), tmplate(isUmbrella, fireOnce)
		{ }

		// Index in the instantiation context for the bank from which
		// the parent umbrella reaction is taken
		uint parentBankId;
		// Index of the umbrella reaction in the parent bank
		uint umbrellaId;
		// The reaction's template
		reaction::Template tmplate;
	};

	// The reactions in this bank
	typedef std::vector< TargettedTemplate > Templates;
	Templates templates;
};

} // namespace reaction
} // namespace sgns2

#endif // REACTIONBANK_H
