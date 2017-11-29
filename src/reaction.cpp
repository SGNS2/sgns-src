
// See reaction.h for a description of the contents of this file.

#include "stdafx.h"

#include <cassert>
#include <algorithm>

#include "compartment.h"
#include "reaction.h"

namespace sgns2 {
namespace reaction {

// ---------------------------------------------------------------------------
Reactant::Reactant( int amount, uint speciesIndex, uint compartmentIndex, Reactant *next )
: rate(RateFunction::Linear())
, consumes(-amount)
, srcIndex(speciesIndex)
, srcCompartment(compartmentIndex)
, next(next)
{
}

// ---------------------------------------------------------------------------
Reactant::~Reactant() {
}

// ---------------------------------------------------------------------------
Product::Product( int amount, uint speciesIndex, uint compartmentIndex, Product *next )
: tau(RuntimeDistribution::DeltaDistribution(0.0))
, produces(amount)
, destCompartment(compartmentIndex)
, destIndex(speciesIndex)
, next(next)
{
}

// ---------------------------------------------------------------------------
Product::~Product() {
}

// ---------------------------------------------------------------------------
void Product::release( Compartment **context ) throw() {
	if( !tau.isZero() ) {
		// Delayed release
		Compartment *dest = context[destCompartment];
		SimulationInstance *sim = dest->getSimulation();
		double dt = tau.sample( sim->distrCtx() );
		dest->getWaitList()->releaseAt( sim->getTime() + dt, destIndex, produces );
	} else {
		// Delay-less release
		context[destCompartment]->modifyPopulation( destIndex, produces );
	}
}

// ---------------------------------------------------------------------------
Template::Template( bool umbrella, bool fireOnce ) throw()
: c(1.0)
, firstReactant(NULL)
, firstProduct(NULL)
, firstExtra(NULL)
, isUmbrella(umbrella)
, firesOnce(fireOnce)
, nCompartments(0)
, hEval(&default_hEval)
{ }

// ---------------------------------------------------------------------------
Template::~Template() throw() {
}

// ---------------------------------------------------------------------------
void Template::execute( Compartment **context ) const throw() {
	for( Reactant *s = firstReactant; s; s = s->getNext() )
		s->consume( context );
	for( Product *p = firstProduct; p; p = p->getNext() )
		p->release( context );
}

// ---------------------------------------------------------------------------
void Template::executeExtra( Compartment **context ) const throw() {
	if( firstExtra ) {
		Extra *extra = firstExtra;
		do {
			extra->execute( this, context );
			extra = extra->getNextExtra();
		} while( extra );
	}
}

// ---------------------------------------------------------------------------
ReactionInstance *Template::instantiate( Compartment **in, ReactionInstance *umbrellaInst ) const throw() {
	ReactionInstance *inst = NULL;
	EventQueue *q = in[0];
	if( umbrellaInst )
		q = static_cast<UmbrellaInstance*>(umbrellaInst);
	if( isUmbrella ) {
		if( nCompartments <= 1 ) {
			// NOTE: Umbrella reactions are naturally compatible with firesOnce
			TemplateStoich<1> stoich( this, in );
			inst = new UmbrellaInstance( q, stoich );
		} else {
			assert( false ); // >= 2 Compartment Umbrella reactions NYI
		}
	} else if( firesOnce ) {
		switch( nCompartments ) {
		case 0: // Fallthrough
		case 1: {
				TemplateStoich<1> stoich( this, in );
				inst = new FireOnceInstance( q, stoich );
			} break;
		default:
			assert( false ); // >= 2 Compartment reactions NYI
		}
	} else {
		switch( nCompartments ) {
		case 0: // Fallthrough
		case 1: {
				TemplateStoich<1> stoich( this, in );
				inst = new Instance( q, stoich );
			} break;
		case 2: {
				TemplateStoich<2> stoich( this, in );
				inst = new InterfaceInstance( q, stoich );
			} break;
		case 3: {
				TemplateStoich<3> stoich( this, in );
				inst = new ThreeWayInstance( q, stoich );
			} break;
		default:
			assert( false ); // >= 4 Compartment reactions NYI
		}
	}

	addDependencies( in, inst );
	inst->begin();

	return inst;
}

// ---------------------------------------------------------------------------
void Template::addDependencies( Compartment **context, ReactionInstance *inst ) const throw() {
	for( Reactant *s = firstReactant; s; s = s->getNext() )
		context[s->getCompartmentIndex()]->addDependency( s->getIndex(), inst );
}

// ---------------------------------------------------------------------------
void Template::removeDependencies( Compartment **context, ReactionInstance *inst ) const throw() {
	for( Reactant *s = firstReactant; s; s = s->getNext() )
		context[s->getCompartmentIndex()]->removeDependency( s->getIndex(), inst );
}

// ---------------------------------------------------------------------------
Reactant *Template::newReactant( uint idx, int pop, uint compartment ) {
	nCompartments = std::max( compartment + 1, nCompartments );
	return firstReactant = new Reactant( pop, idx, compartment, firstReactant );
}

// ---------------------------------------------------------------------------
Product *Template::newProduct( uint idx, int pop, uint compartment ) {
	nCompartments = std::max( compartment + 1, nCompartments );
	return firstProduct = new Product( pop, idx, compartment, firstProduct );
}

// ---------------------------------------------------------------------------
void Template::flipChemicalOrders() {
	Reactant *r = firstReactant;
	firstReactant = NULL;
	while( r ) {
		Reactant *next = r->getNext();
		r->setNext( firstReactant );
		firstReactant = r;
		r = next;
	}
	Product *p = firstProduct;
	firstProduct = NULL;
	while( p ) {
		Product *next = p->getNext();
		p->setNext( firstProduct );
		firstProduct = p;
		p = next;
	}
}

// ---------------------------------------------------------------------------
void Template::addExtra( Template::Extra *extra ) {
	extra->setNextExtra( firstExtra );
	firstExtra = extra;
}

// ---------------------------------------------------------------------------
double SGNS_FASTCALL Template::default_hEval( Compartment **context, Reactant *r ) {
	double h = 1.0;
	for( ; r; r = r->getNext() )
		h *= r->evaluate( context );
	return h;
}

} // namespace
} // namespace

