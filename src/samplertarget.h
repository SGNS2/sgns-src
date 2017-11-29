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

/* samplertarget.h/cpp

SamplerTarget class contents:
	- Interface for all sampler targets

FileSamplerTarget class contents:
	- SamplerTarget implementation targetting multiple output files
	- Opens/closes files as necessary
	- Manages the compartment -> file mapping

StdoutSamplerTarget class contents:
	- SamplerTarget implementation targetting stdout
	- Squelches all output for compartments other than Env
	- Sends Env info to the stdout

*/

#ifndef SAMPLERTARGET_H
#define SAMPLERTARGET_H

#include <string>
#include <map>

class SamplerTarget {
public:
	virtual ~SamplerTarget();

	// Called before the first beginCompartment
	// Set whether the data is binary or text
	virtual void setBinary( bool isBinary ) = 0;
	// Called before writing the data for a compartment
	// Returns true when the header should be output (it is the
	// first time that the compartment is sampled)
	virtual bool beginCompartment( sgns2::HierCompartment *compartment ) = 0;
	// Write sample data for a compartment
	virtual void writeData( const void *data, size_t size ) = 0;
	// Called when ALL compartments for this sample have been completed
	virtual void endSample() = 0;

protected:
	SamplerTarget();
};

class FileSamplerTarget : public SamplerTarget {
public:
	// Creates a file sampler with envName as the environment compartment's
	// output filename and pattern as the compartment output pattern
	FileSamplerTarget( const char *envName, const char *pattern );
	virtual ~FileSamplerTarget();

	// SamplerTarget functions
	virtual void setBinary( bool isBinary );
	virtual bool beginCompartment( sgns2::HierCompartment *compartment );
	virtual void writeData( const void *data, size_t size );
	virtual void endSample();

private:
	// Close a file so that others may be opened
	void dropFile();

	bool binary; // Open files in binary mode
	std::string envName; // The environment's output filename
	std::string fileNamePattern; // Compartment output filename pattern

	struct CompartmentDesc {
		// Describes a compartment with a currently opened output file

		CompartmentDesc( sgns2::uint idx, FILE *f );
		CompartmentDesc *next, **toMe; // SLL of CompartmentDesc's
		FILE *file; // Opened output file of this compartment (NULL if file is closed)
		sgns2::uint instIdx; // Instantiation index of this compartment
	};
	// Map of compartment instantiation index -> CompartmentDesc
	typedef std::map< sgns2::uint, CompartmentDesc > InstToDesc;
	InstToDesc currentOutputs;

	CompartmentDesc *currentList; // SLL of compartments sampled this sample
	CompartmentDesc *lastSampleList; // SLL of compartments sampled last sample
	FILE *currentFile; // Output file of the currently sampling compartment
};

class StdoutSamplerTarget : public SamplerTarget {
public:
	// Creates a new stdout sampler
	StdoutSamplerTarget();
	virtual ~StdoutSamplerTarget();

	// SamplerTarget functions
	virtual void setBinary( bool isBinary );
	virtual bool beginCompartment( sgns2::HierCompartment *compartment );
	virtual void writeData( const void *data, size_t size );
	virtual void endSample();

private:
	bool firstSample; // Is this the first sample?
	bool squelchData; // Don't output data
};

#endif //SAMPLERTARGET_H
