
// See samplertarget.h for a description of the contents of this file.

#include "stdafx.h"

// iostream handles the file IO
#include <fstream>
// The stdout target is written with lower-level IO libraries
// so that it can properly output binary data
#include <cstdio>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include "platform.h"
#include "hiercompartment.h"
#include "samplertarget.h"
#include "compartmenttype.h"

// ---------------------------------------------------------------------------
SamplerTarget::SamplerTarget()
{
}

// ---------------------------------------------------------------------------
SamplerTarget::~SamplerTarget() {
}

// ---------------------------------------------------------------------------
FileSamplerTarget::FileSamplerTarget( const char *envName, const char *pattern )
: binary(false)
, envName(envName)
, fileNamePattern(pattern)
, currentList(NULL)
, lastSampleList(NULL)
, currentFile(NULL)
{
}

// ---------------------------------------------------------------------------
FileSamplerTarget::~FileSamplerTarget() {
	endSample();
	endSample();
}

// ---------------------------------------------------------------------------
void FileSamplerTarget::setBinary( bool isBinary ) {
	binary = isBinary;
}

// ---------------------------------------------------------------------------
bool FileSamplerTarget::beginCompartment( sgns2::HierCompartment *compartment ) {
	InstToDesc::iterator it = currentOutputs.find( compartment->getInstantiationIndex() );
	const char *openMode = binary ? "ab" : "a";
	CompartmentDesc *desc;
	bool ret = false;

	if( it == currentOutputs.end() ) {
		// New compartment!
		openMode = binary ? "wb" : "w";
		ret = true;
		
		// Add the CompartmentDesc
		currentOutputs.insert( InstToDesc::value_type( compartment->getInstantiationIndex(), CompartmentDesc( compartment->getInstantiationIndex(), NULL ) ) );
		it = currentOutputs.find( compartment->getInstantiationIndex() );
		desc = &it->second;
	} else {
		desc = &it->second;

		// Remove from the old list
		if( desc->next )
			desc->next->toMe = desc->toMe;
		*desc->toMe = desc->next;
	}

	if( !desc->file ) {
		// Re-open the file
		if( compartment->getContainer() ) {
			// Not Env - generate the filename
			char filename[PATH_MAX];
			snprintf( filename, PATH_MAX, fileNamePattern.c_str(), compartment->getType()->getName().c_str(), compartment->getInstantiationIndex() );
			desc->file = fopen( filename, openMode );
			if( !desc->file ) {
				dropFile();
				desc->file = fopen( filename, openMode );
				if( !desc->file )
					fprintf( stderr, "Warning: Failed to open %s for writing.\n", filename );
			}
		} else {
			// Env - use the envName
			desc->file = fopen( envName.c_str(), openMode );
			if( !desc->file ) {
				dropFile();
				desc->file = fopen( envName.c_str(), openMode );
				if( !desc->file )
					fprintf( stderr, "Warning: Failed to open %s for writing.\n", envName.c_str() );
			}
		}
	}

	// Insert into the current list
	desc->toMe = &currentList;
	desc->next = currentList;
	if( currentList )
		currentList->toMe = &desc->next;
	currentList = desc;

	currentFile = desc->file;
	return ret;
}

// ---------------------------------------------------------------------------
void FileSamplerTarget::writeData( const void *data, size_t size ) {
	if( currentFile )
		fwrite( data, size, 1, currentFile );
}

// ---------------------------------------------------------------------------
void FileSamplerTarget::endSample() {
	// Remove all descriptions for all compartments not sampled this sample
	while( lastSampleList ) {
		CompartmentDesc *desc = lastSampleList;
		lastSampleList = lastSampleList->next;

		if( desc->file )
			fclose( desc->file );
		currentOutputs.erase( desc->instIdx );
	}

	// Swap lists
	lastSampleList = currentList;
	if( lastSampleList )
		lastSampleList->toMe = &lastSampleList;
	currentList = NULL;
}

// ---------------------------------------------------------------------------
void FileSamplerTarget::dropFile() {
	// Find a file to close so that another may be opened
	// The order of file closing is chosen so that if N compartments need to
	// be output simultaneously and there is a limit of M files, then there
	// will be N-M re-opening operations per sample
	CompartmentDesc *toDrop = currentList;
	while( toDrop && !toDrop->file )
		toDrop = toDrop->next;

	if( !toDrop ) {
		toDrop = lastSampleList;
		while( toDrop && !toDrop->file )
			toDrop = toDrop->next;
	}

	if( toDrop ) {
		fclose( toDrop->file );
		toDrop->file = NULL;
	}
}

// ---------------------------------------------------------------------------
FileSamplerTarget::CompartmentDesc::CompartmentDesc( sgns2::uint idx, FILE *f )
: next(NULL), toMe(NULL)
, file(f), instIdx(idx)
{
}

// ---------------------------------------------------------------------------
StdoutSamplerTarget::StdoutSamplerTarget()
	: firstSample(true), squelchData(false)
{
}

// ---------------------------------------------------------------------------
StdoutSamplerTarget::~StdoutSamplerTarget() {
}

// ---------------------------------------------------------------------------
void StdoutSamplerTarget::setBinary( bool isBinary ) {
	(void)isBinary;
#ifdef _WIN32
	_setmode( _fileno(stdout), isBinary ? _O_BINARY : _O_TEXT );
#endif
}

// ---------------------------------------------------------------------------
bool StdoutSamplerTarget::beginCompartment( sgns2::HierCompartment *compartment ) {
	if( compartment->getContainer() ) {
		// Squelch all data from non-Env compartments
		squelchData = true;
		return false;
	}
	
	// Output headers for the first sample only
	bool ret = firstSample;
	firstSample = false;
	squelchData = false;
	return ret;
}

// ---------------------------------------------------------------------------
void StdoutSamplerTarget::writeData( const void *data, size_t size ) {
	if( !squelchData )
		fwrite( data, size, 1, stdout );
}

// ---------------------------------------------------------------------------
void StdoutSamplerTarget::endSample() {
}

