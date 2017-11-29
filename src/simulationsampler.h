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

/* simulationsampler.h/cpp

SimulationSampler class contents:
	- Abstract base class for all simulation samplers

NullSampler class contents:
	- Sampler that does nothing

FileRecordSampler class contents:
	- Base class for file-based samplers

Bin32Sampler class contents:
	- Fixed-width 32bit binary sampler
	- The records are in the same order as the DlmTextSampler, but are
	  output in binary

Bin64Sampler class contents:
	- Fixed-width 64bit binary sampler
	- The records are in the same order as the DlmTextSampler, but are
	  output in binary

DlmTextSampler class contents:
	- Delimited text-based sampler
	- Outputs a spreadsheet of samples, one time point per row

*/

#ifndef SIMULATIONSAMPLER_H
#define SIMULATIONSAMPLER_H

#include "simulation.h"
#include "hiercompartment.h"
#include "samplertarget.h"
#include "simulationloader.h"

// ===========================================================================
class SimulationSampler {
public:
	SimulationSampler();
	virtual ~SimulationSampler();

	virtual void sampleState( sgns2::SimulationInstance *sim, sgns2::HierCompartment *env ) = 0;
	//virtual void sampleStep( sgns2::Simulation *sim, sgns2::Compartment *env ) = 0;
};

// ===========================================================================
class NullSampler : public SimulationSampler {
public:
	NullSampler();
	virtual ~NullSampler();

	virtual void sampleState( sgns2::SimulationInstance *sim, sgns2::HierCompartment *env );
	//virtual void sampleStep( sgns2::Simulation *sim, sgns2::Compartment *env );
};

// ===========================================================================
template< typename T >
class FileRecordSampler : public SimulationSampler {
public:
	FileRecordSampler( SamplerTarget *target , sgns2::SimulationLoader *ld );
	virtual ~FileRecordSampler();

	virtual void sampleState( sgns2::SimulationInstance *sim, sgns2::HierCompartment *env );
	//virtual void sampleStep( sgns2::Simulation *sim, sgns2::Compartment *env );

protected:
	void sampleCompartment( sgns2::HierCompartment *compartment );

	// Helper for the curiously recursive template stuff
	T *thisT() { return (T*)this; }
	// The final destination of the samples
	SamplerTarget *target;
	// The loader, used to determine what to ouput
	sgns2::SimulationLoader *ld;
	// The string to insert between records (within the columns)
	char recordSeparator[4];
	unsigned recordSepLen;
	// The string to insert between samples (between rows)
	char sampleSeparator[4];
	unsigned sampleSepLen;
	
	inline void writeHeaderField( const char* ) { } // Default
	// void writeRecord( double d );
	// void writeRecord( sgns2::api::int64 i );
};

// ===========================================================================
class Bin32Sampler : public FileRecordSampler<Bin32Sampler> {
public:
	Bin32Sampler( SamplerTarget *target, sgns2::SimulationLoader *ld );

	void writeRecord( double d );
	void writeRecord( sgns2::int64 i );
};

// ===========================================================================
class Bin64Sampler : public FileRecordSampler<Bin64Sampler> {
public:
	Bin64Sampler( SamplerTarget *target, sgns2::SimulationLoader *ld  );

	void writeRecord( double d );
	void writeRecord( sgns2::int64 i );
};

// ===========================================================================
class DlmTextSampler : public FileRecordSampler<DlmTextSampler> {
public:
	DlmTextSampler( SamplerTarget *target, sgns2::SimulationLoader *ld, char delimiter );

	void writeHeaderField( const char *title );
	void writeRecord( double d );
	void writeRecord( sgns2::int64 i );
};

#endif //SAMPLERTARGET_H
