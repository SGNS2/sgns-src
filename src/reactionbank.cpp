
// See reactionbank.h for a description of the contents of this file.

#include "stdafx.h"

#include "reactionbank.h"

namespace sgns2 {
namespace reaction {

// ---------------------------------------------------------------------------
BankTemplate::BankTemplate()
	: instances((uint)-1)
{
}

// ---------------------------------------------------------------------------
BankTemplate::~BankTemplate() {
	assert( !isSealed() || instances == 0 ); // Check that all instances are destroyed
}

// ---------------------------------------------------------------------------
void BankTemplate::seal() {
	if( !isSealed() )
		instances = 0;
}

// ---------------------------------------------------------------------------
BankInstance::BankInstance()
{
}

// ---------------------------------------------------------------------------
BankInstance::~BankInstance() {
	tmplate->destroyInstance( this );
}

// ---------------------------------------------------------------------------
IntraBankTemplate::IntraBankTemplate()
{
}

// ---------------------------------------------------------------------------
IntraBankTemplate::~IntraBankTemplate() {
	assert( instances == 0 || isSealed() );
}

// ---------------------------------------------------------------------------
BankInstance *IntraBankTemplate::instantiate( Compartment *in, BankInstance **context ) {
	assert( isSealed() );

	BankInstance *bi = new BankInstance;
	bi->tmplate = this;
	bi->instances = new ReactionInstance*[getReactionCount()];

	for( uint i = 0; i < templates.size(); i++ ) {
		if( templates[i].umbrellaId == (uint)-1 ) {
			// Free reaction
			bi->instances[i] = templates[i].tmplate.instantiate( &in );
		} else {
			// sub-reaction
			bi->instances[i] = templates[i].tmplate.instantiate( &in, context[templates[i].parentBankId]->getReactionInstance( templates[i].umbrellaId ) );
		}
		bi->instances[i]->begin();
	}

	instances++;

	return bi;
}

// ---------------------------------------------------------------------------
void IntraBankTemplate::destroyInstance( BankInstance *bi ) {
	assert( bi->tmplate == this );

	for( uint i = 0; i < templates.size(); i++ )
		delete bi->instances[i];
	delete[] bi->instances;

	instances--;
}

// ---------------------------------------------------------------------------
uint IntraBankTemplate::createReaction( uint parentBank, uint umbrellaId, bool umbrella, bool fireOnce ) {
	assert( !isSealed() );

	uint id = static_cast<uint>(templates.size());
	templates.push_back( TargettedTemplate( umbrellaId, parentBank, umbrella, fireOnce ) );
	return id;
}

} // namespace reaction
} // namespace sgns2
