
// See simulationsampler.h for a description of the contents of this file.

#include "stdafx.h"

#include <stdint.h>
#include <cstdio>

#include "platform.h"
#include "simulationsampler.h"
#include "hiercompartment.h"
#include "compartmenttype.h"
#include "chemical.h"
#include "simulationloader.h"

// ===========================================================================
SimulationSampler::SimulationSampler()
{
}

// ---------------------------------------------------------------------------
SimulationSampler::~SimulationSampler() {
}

// ===========================================================================
NullSampler::NullSampler() {
}

// ---------------------------------------------------------------------------
NullSampler::~NullSampler() {
}

// ---------------------------------------------------------------------------
void NullSampler::sampleState( sgns2::SimulationInstance*, sgns2::HierCompartment* ) {
}

// ===========================================================================
template< typename T >
FileRecordSampler<T>::FileRecordSampler( SamplerTarget *target, sgns2::SimulationLoader *ld )
: target(target), ld(ld)
, recordSepLen(0)
, sampleSepLen(0)
{
}

// ---------------------------------------------------------------------------
template< typename T >
FileRecordSampler<T>::~FileRecordSampler() {
}

// ---------------------------------------------------------------------------
template< typename T >
void FileRecordSampler<T>::sampleState( sgns2::SimulationInstance *sim, sgns2::HierCompartment *env ) {
	(void)sim;
	sampleCompartment( env );
	target->endSample();
}

// ---------------------------------------------------------------------------
template< typename T >
void FileRecordSampler<T>::sampleCompartment( sgns2::HierCompartment *compartment ) {
	bool first_column = true;
	if( compartment->getType()->shouldOutput() ) {
		const sgns2::CompartmentType *type = compartment->getType();

		if( target->beginCompartment( compartment ) ) {
			// This is the first output for the compartment - output column headers
			if( ld->shouldShow( sgns2::SimulationLoader::SHOW_TIME ) ) {
				static_cast<T*>(this)->writeHeaderField( "Time" );
				first_column = false;
			}
			if( ld->shouldShow( sgns2::SimulationLoader::SHOW_STEP_COUNT ) ){
				if( !first_column) {
					target->writeData( &recordSeparator[0], recordSepLen );
				}
				static_cast<T*>(this)->writeHeaderField( "Step Count" );
				first_column = false;
			}
			if( ld->shouldShow( sgns2::SimulationLoader::SHOW_WL_SIZE ) ){
				if( !first_column ) {
					target->writeData( &recordSeparator[0], recordSepLen );
				}
				static_cast<T*>(this)->writeHeaderField( "Wait List Size" );
				first_column = false;
			}

			for( sgns2::uint i = 0; i < type->getChemicalCount(); i++ ) {
				if( type->getChemicalAtIndex(i)->shouldOutput() ) {
					if( !first_column ) {
						target->writeData( &recordSeparator[0], recordSepLen );
					}
					static_cast<T*>(this)->writeHeaderField( type->getChemicalAtIndex(i)->getName().c_str() );
					first_column = false;
				}
			}
		}

		// End the last row
		target->writeData( &sampleSeparator[0], sampleSepLen );
		first_column = true;

		// Output time
		if( ld->shouldShow( sgns2::SimulationLoader::SHOW_TIME ) ) {
			static_cast<T*>(this)->writeRecord( compartment->getSimulation()->getTime() );
			first_column = false;
		}

		// Output special columns
		if( ld->shouldShow( sgns2::SimulationLoader::SHOW_STEP_COUNT ) ){
			if( !first_column ) {
				target->writeData( &recordSeparator[0], recordSepLen );
			}
			static_cast<T*>(this)->writeRecord( (sgns2::int64)compartment->getSimulation()->getStepCount());
			first_column = false;
		}
		if( ld->shouldShow( sgns2::SimulationLoader::SHOW_WL_SIZE ) ){
			if( !first_column  ) {
				target->writeData( &recordSeparator[0], recordSepLen );
			}
			static_cast<T*>(this)->writeRecord( (sgns2::int64)compartment->getWaitList()->getSize());
			first_column = false;
		}

		// Output molecule populations
		for( sgns2::uint i = 0; i < type->getChemicalCount(); i++ ) {
			if( type->getChemicalAtIndex(i)->shouldOutput() ) {
				if( !first_column ) {
					target->writeData( &recordSeparator[0], recordSepLen );
				}
				static_cast<T*>(this)->writeRecord( (sgns2::int64)compartment->getPopulation( i ) );
				first_column = false;
			}
		}
	}

	// Output all subcompartments
	for( sgns2::HierCompartment *subComp = compartment->getFirstSubCompartment(); subComp; subComp = subComp->getNextInContainer() )
		sampleCompartment( subComp );
}

// ===========================================================================
Bin32Sampler::Bin32Sampler( SamplerTarget *target, sgns2::SimulationLoader *ld )
: FileRecordSampler<Bin32Sampler>( target , ld )
{
	target->setBinary( true );
}

// ---------------------------------------------------------------------------
void Bin32Sampler::writeRecord( double d ) {
	float f = (float)d;
	target->writeData( &f, 4 );
}

// ---------------------------------------------------------------------------
void Bin32Sampler::writeRecord( sgns2::int64 i ) {
	int32_t i32 = (int32_t)i;
	target->writeData( &i32, 4 );
}

// ===========================================================================
Bin64Sampler::Bin64Sampler( SamplerTarget *target, sgns2::SimulationLoader *ld  )
: FileRecordSampler<Bin64Sampler>( target, ld )
{
	target->setBinary( true );
}

// ---------------------------------------------------------------------------
void Bin64Sampler::writeRecord( double d ) {
	target->writeData( &d, 8 );
}

// ---------------------------------------------------------------------------
void Bin64Sampler::writeRecord( sgns2::int64 i ) {
	target->writeData( &i, 8 );
}

// ===========================================================================
DlmTextSampler::DlmTextSampler( SamplerTarget *target, sgns2::SimulationLoader *ld, char delimiter )
: FileRecordSampler<DlmTextSampler>( target , ld )
{
	target->setBinary( false );
	recordSeparator[0] = delimiter;
	recordSepLen = 1;
	sampleSeparator[0] = '\n';
	sampleSepLen = 1;
}

// ---------------------------------------------------------------------------
void DlmTextSampler::writeHeaderField( const char *title ) {
	target->writeData( title, strlen(title) );
}

// ---------------------------------------------------------------------------
void DlmTextSampler::writeRecord( double d ) {
	char text[64];
	size_t n = snprintf( &text[0], sizeof(text), "%.20g", d );
	target->writeData( text, n );
}

// ---------------------------------------------------------------------------
void DlmTextSampler::writeRecord( sgns2::int64 i ) {
	char text[64];
	size_t n = snprintf( &text[0], sizeof(text), "%.0f", (double)i );
	target->writeData( text, n );
}

