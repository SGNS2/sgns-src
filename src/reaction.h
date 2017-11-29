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

/* reaction.h/cpp

Reactant class contents:
	- Represents a reactant in a reaction
	- Stores stoichiometry, species index and rate function

Product class contents:
	- Represents a product in a reaction
	- Stores stoichiometry, species index and delay distribution

Template class contents:
	- Represents a reaction (not bound to any specific compartment)
	- Stores Reactants, Products and stochastic constants

TemplateStoich class contents:
	- Stoichiometry object to insert into a ReactionInstance which redirects
	  all calls to a reaction::Template class
*/

#ifndef REACTION_H
#define REACTION_H

#include <climits>
#include <deque>

#include "simtypes.h"
#include "simulation.h"
#include "event.h"
#include "rng.h"
#include "rate.h"
#include "distribution.h"
#include "reactioninstance.h"
#include "compartment.h"

namespace sgns2 {
namespace reaction {

class Template;
template< uint C >
class TemplateStoich;

// ===========================================================================
class Reactant {
public:
	Reactant( int amount, uint speciesIndex, uint compartmentIndex, Reactant *next );
	~Reactant();

	// Get the population of this reactant in this compartment context
	inline Population getPopulationIn( Compartment **in ) throw() {
		return in[srcCompartment]->getPopulation( srcIndex ); }
	// Evaluate the rate function of this reactant
	inline double evaluate( Compartment **in ) throw() {
		return rate.evaluate( getPopulationIn(in) ); }
	// Get the next reactant in this reaction
	inline Reactant *getNext() throw() { return next; }
	inline void setNext( Reactant *next ) throw() { this->next = next; }
	// Consume some of this reactant
	inline void consume( Compartment **in ) throw() {
		in[srcCompartment]->modifyPopulation( srcIndex, consumes ); }
	// Access to the rate function
	inline RateFunction *getRateFunction() { return &rate; }
	// Access the amount of the reactant is consumed
	inline int getConsumes() { return -consumes; }
	// Set the stoichiometry of the reactant
	inline void setConsumes( int n ) { consumes = -n; }
	// Get the index in the compartment of the species of this reactant
	inline uint getIndex() { return srcIndex; }
	// Get the index of the compartment in the TemplateStoich
	inline uint getCompartmentIndex() { return srcCompartment; }
		
private:
	// Rate function of the reactant
	RateFunction rate;
	// Amount consumed
	int consumes;
	// Index of the population
	uint srcIndex;
	// Index of the compartment
	uint srcCompartment;
	// The next reactant in the reaction
	Reactant *next;
};

// ===========================================================================
class Product {
	friend class ProductQueue;
	friend class Template;
public:
	// Constructor to be called by Reaction
	Product( int amount, uint speciesIndex, uint compartmentIndex, Product *next );
	~Product();

	// Release this product into a compartment
	void release( Compartment **in ) throw();
	// Get the next product in this reaction
	inline Product *getNext() throw() { return next; }
	// Set the next product in this reaction
	inline void setNext( Product *next ) throw() { this->next = next; }
	// Access to the time delay
	inline RuntimeDistribution *getTau() { return &tau; }
	// Access to the target compartment index
	//inline uint getDestCompartmentIdx() const { return destCompartment; }
	// Get the numebr of this product produced when the reaction occurs
	inline int getProduces() const throw() { return produces; }
	// Set the number of this product produced when the reaction occurs
	inline void setProduces( int n ) throw() { produces = n; }
		
private:
	// Normal population product
	RuntimeDistribution tau;
	// Number of this molecule produced
	int produces;
	// Index in the TemplateStoich that this product is produced in
	uint destCompartment;
	// Index of the product's species in the compartment
	uint destIndex;
	// The next product in the reaction
	Product *next;
};

// ===========================================================================
// A general reaction template
class Template {
public:
	explicit Template( bool umbrella = false, bool fireOnce = false ) throw();
	~Template() throw();

	// Instances for NRM
	typedef ReactionStoichInstance<TemplateStoich<1> > Instance;
	typedef FireOnceReactionInstance<TemplateStoich<1> > FireOnceInstance;
	// Instances for two-compartment NRM reactions
	typedef ReactionStoichInstance<TemplateStoich<2> > InterfaceInstance;
	// Instances for three-compartment NRM reactions (interface + counter?)
	typedef ReactionStoichInstance<TemplateStoich<3> > ThreeWayInstance;
	// Instances for hierarchical NRM
	typedef MarkovUmbrellaReactionInstance<TemplateStoich<1> > UmbrellaInstance;

	class Extra {
		// Extra actions that a template can do.. Could include:
		//    Compartment construction/destruction
		//    Sample-on-reaction (NYI)
		//    Lua callbacks (NYI)
	public:
		Extra() throw() { }
		virtual ~Extra() { }

		virtual void execute( const Template *tmplate, Compartment **context ) = 0;

		inline void setNextExtra( Extra *next ) throw() { nextExtra = next; }
		inline Extra *getNextExtra() const throw() { return nextExtra; }

	private:
		Extra *nextExtra;
	};

	// Get the reaction's stochastic constant
	double getC() const { return c; }
	// Manage the stochastic constant of this reaction
	// Only affects instances which are not yet created
	void setC( double newC ) { c = newC; }

	// Calculates the H-function for this reaction
	inline double calcH( Compartment **context ) const throw() {
		return hEval( context, firstReactant ); }
	// Executes this reaction
	void execute( Compartment **context ) const throw();
	// Execures the Extra actions associated with this reaction
	void executeExtra( Compartment **context ) const throw();

	// Make a new instance of this reaction
	ReactionInstance *instantiate( Compartment **in, ReactionInstance *umbrella = NULL ) const throw();
	// Add this reaction's dependencies to the compartments
	void addDependencies( Compartment **context, ReactionInstance *inst ) const throw();
	// Remove this reaction's dependencies to the compartments
	void removeDependencies( Compartment **context, ReactionInstance *inst ) const throw();

	// Create a new reactant for this reaction
	Reactant *newReactant( uint idx, int pop, uint compartment );
	// Create a new product for this reaction
	Product *newProduct( uint idx, int pop, uint compartment );

	// Get the list of reactants in this reaction
	inline Reactant *getFirstReactant() const { return firstReactant; }
	// Get the list of products in this reaction
	inline Product *getFirstProduct() const { return firstProduct; }
	// Reverse the orders of the reactants and products
	void flipChemicalOrders();

	// Add a new Extra action to be executed upon execution of this reaction
	void addExtra( Extra *extra );

	// Is this an umbrella reaction?
	inline bool isUmbrellaReaction() const { return isUmbrella; }

	// Overriding the H evaluator
	typedef double (SGNS_FASTCALL *HEvaluator)( Compartment **context, Reactant *firstReactant );
	inline void setHEvaluator( HEvaluator eval ) { hEval = eval; }

protected:
	// The reaction's stochastic constant
	double c;
	// The first reactant in the SLL of reactants
	Reactant *firstReactant;
	// The first product in the SLL of products
	Product *firstProduct;
	// The first Extra in the list of Extras
	Extra *firstExtra;
	// Umbrella reaction flag (some term is factored out) and fire-once
	// flag (the reaction destroys the compartment)
	bool isUmbrella, firesOnce;
	uint nCompartments;

	// The default H-function is just the product of each reactant's rate
	// functions applied to their molecule populations
	static double SGNS_FASTCALL default_hEval( Compartment **context, Reactant *firstReactant );
	// The H-function of the reaction
	HEvaluator hEval;
};
	
// ===========================================================================
// General stochiometry classes which forward propensity calculations to
// the template
template< uint C >
class TemplateStoich {
public:
	TemplateStoich( const Template *rxn, Compartment **ctx ) throw()
		: c(rxn->getC()), tmplate(rxn) {
			for( uint i = 0; i < C; i++ )
				space[i] = ctx[i];
	}
	~TemplateStoich() throw() { }

	inline void destroy( ReactionInstance *inst ) {
		tmplate->removeDependencies( getContext(), inst );
	}

	inline double calcMarkovA() {
		return c * tmplate->calcH( getContext() );
	}

	inline void doReaction() {
		tmplate->execute( getContext() );
	}

	inline void doReactionExtra() {
		tmplate->executeExtra( getContext() );
	}

	inline Compartment **getContext() {
		return &space[0];
	}

	inline RNG::RNG *getRNG() {
		return space[0]->getSimulation()->getRNG();
	}

private:
	// These are the reaction's run-time variables
	double c; // The reaction rate
	const Template *tmplate;
		
	Compartment *space[C];
};

} // namespace reaction
} // namespace sgns2

#endif
