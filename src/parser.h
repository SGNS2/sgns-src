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

/* parser.h/cpp

sgns2::parse::Error class contents:
	- Stores information about a parsing or loading error

sgns2::parse::ParseListener class contents:
	- Parser-Loader interface

sgns2::parse::Parser class contents:
	- Parsing functions for all SGNS format reaction files
	- Lua state management
*/

#ifndef PARSER_H
#define PARSER_H

#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <setjmp.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include "simtypes.h"
#include "rate.h"
#include "distribution.h"
#include "split.h"

class LuaRandom;

namespace sgns2 {

class Simulation;
class Reaction;
class Reactant;
class Product;

namespace parse {

class ParseStream;

class Error {
public:
	inline Error()
		: msg(NULL), context(NULL), line(NULL), lineNo(0), charNo(0) { }
	// context, line and msg's lifetimes are managed by Parser, which
	// has the lifetime of the Parser or until another error occurs.
	inline Error( const char *msg, const char *context, const char *line, unsigned lineNo, unsigned charNo )
		: msg(msg), context(context), line(line), lineNo(lineNo), charNo(charNo) { }
	~Error() { }

	inline const char *getMessage() const { return msg; }
	inline const char *getContext() const { return context; }
	inline const char *getLine() const { return line; }
	inline unsigned getLineNo() const { return lineNo; }
	inline unsigned getCharNo() const { return charNo; }

private:
	Error( const Error& ) { }

	const char *msg;
	const char *context;
	const char *line;
	unsigned lineNo;
	unsigned charNo;
};

class ParseListener {
public:
	virtual ~ParseListener() { }

	// Entry point to parse unknown identifiers
	virtual bool parseExtra( const char *id, const char *data ) = 0;

	// Select a chemical
	// If it does not exist yet, it should be created in the current context with
	// 0 population and with the given visibility
	virtual void selectChemical( const char *name, bool defVisibility ) = 0;
	// Set the current chemical's visibility
	virtual void setChemicalVisible( bool visible ) = 0;
	// Set the amount of the chemical
	// distr should be copied as it won't outlive this call
	virtual void setPopulation( const sgns2::RuntimeDistribution *distr, bool add ) = 0;

	// Creates a new compartment type
	// If the current compartment selection is a type, the new type is
	// 'inside' the selected type and interface banks should be instantiated
	// only between the parent and child compartments
	virtual void createCompartmentType( const char *typestr ) = 0;
	// Selects a compartment type
	virtual void selectCompartmentType( const char *name ) = 0;
	// The name could be a type
	// Repeated calls narrow down the selection
	// NULL resets the selection
	virtual void selectCompartment( const char *name ) = 0;
	// Flag the selected compartment/compartment type for output / hiding
	virtual void outputCompartment( bool output ) = 0;
	// Create a compartment of the selected type in the selected
	// compartment (expected to be unique, but not guaranteed)
	virtual void instantiateCompartment( const char *name ) = 0;
	// Creates n compartments of the selected type in each of the
	// selected set of compartments
	virtual void instantiateCompartment( int n ) = 0; // Anonymous instantiation

	// For reactants/products
	// If the current compartment selection is a type, the chemical
	// may be in any one of the compartments of that type. If reactants/products
	// share a compartment type, they are all consumed/produced in a single instance
	// of that compartment type for any execution of a given reaction.

	// Reactions with reactants in more than one compartment are called
	// Interface Reactions. 
		
	// Begin a new reaction
	// If this is called before a finishReaction for a previous
	// newReaction, the old reaction is scrapped
	virtual void newReaction( const char *name ) = 0;
	// Complete the reaction
	virtual void finishReaction( double c ) = 0;
	// Override the H evaluator for the reaction
	virtual void overrideH( const char *func, double *params, unsigned nParams ) = 0;
	// Create a new reactant in the current reaction that
	// consumes n of the selected chemical
	virtual void newReactant( int n ) = 0;
	// Create a new split reactant in the current reaction
	// that splits the current chemical
	virtual void newSplitReactant( const sgns2::SplitFunction *split ) = 0;
	// Create a compartment split in the current reaction that
	// splits the current compartment type
	virtual void newSplitCompartment( const sgns2::SplitFunction *split ) = 0;
	// Set the rate function of the last reactant
	virtual void setRate( const sgns2::RateFunction *rf ) = 0;
	// Create a new product in the current reaction that
	// produces n of the selected chemical
	virtual void newProduct( int n ) = 0;
	// Create a new product that outputs the result of the src'th split
	// in the input list
	virtual void newSplitProduct( unsigned src, bool splitCompartments ) = 0;
	// Set the delay of the last product
	// tau should be copied as it won't outlive this call
	virtual void setTau( const sgns2::RuntimeDistribution *tau ) = 0;
	// When this reaction is executed, it produces a compartment
	// of the selected type 
	virtual void produceCompartment() = 0;
	// Destroys the compartment of the selected type when the
	// reaction goes through
	virtual void eatCompartment() = 0;

	// Adds the current chemical to the wait list of the
	// current selection of compartments
	virtual void addWaitListRelease( int n, double time ) = 0;

	// Basic parameters
	enum Parameter {
		SEED, // Real
		START_TIME, // Real
		READOUT_INTERVAL, // Real
		STOP_TIME, // Real

		READOUT_FILE_TEMPLATE, // String
		READOUT_FILE_HEADER, // String

		SAVE_INTERVAL, // Real
		SAVE_INDEX, // Integer, but set with setParameterD
		SAVE_FILE_TEMPLATE, // String
		SAVE_NOW, // Passed to setParameterS, should save immediately

		BATCH_COUNT, // Integer, but set with setParameterD
		BATCH_THREADS // Integer, but set with setParameterD
	};
	// Sets the value of a real-valued parameter
	virtual void setParameterD( Parameter param, double val ) = 0;
	// Sets the value of a string-valued parameter
	virtual void setParameterS( Parameter param, const char *val ) = 0;

	// Queues a save at the given time
	virtual void saveAt( double time, const char *filename ) = 0;

	// Issues a warning
	virtual void issueWarning( const Error *warning ) = 0;

protected:
	ParseListener() { }
};

class Parser {
public:
	Parser( ParseListener *target );
	virtual ~Parser();

	// Parses the input stream as though from a file
	// Context is passed on to any error message
	virtual bool parse( const char *context, std::istream &in );
	// Parses the input string as though from a file
	// Context is passed on to any error message
	virtual bool parse( const char *context, const char *buffer );
	// Parses an individual identifier-value pair
	// Context is passed on to any error message
	virtual bool parse( const char *context, const char *identifier, const char *data );

	// Access to the last error that occurred
	virtual const Error *getLastError();

	// Adds a C closure to the global Lua environment
	virtual void addLuaClosure( const char *name, lua_CFunction fn );
	// Access the Lua state
	virtual lua_State *getL();
	// Does the model have runtime Lua components?
	virtual bool hasRuntimeLua();
	// Signal that there are runtime lua components
	virtual void setHasRuntimeLua();
	// Compiles and runs a string in Lua
	virtual bool runLua( const char *context, const char *s );
	// Compiles a string in Lua and returns up to maxCount doubles
	virtual int getLuaReals( const char *src, const char *s, double *reals, int maxCount );
	// Compiles a lua function and returns its registry index
	virtual int getLuaFunction( const char *src, const char *s );

	// Raises an error in the current parsing context (one of the
	// parse functions must have been called
	// Parsing errors are flagged by throwing 1u
	virtual void raiseError( const char *format, ... );
	// Raises a warning in the current parsing context (one of the
	// parse functions must have been called
	virtual void warning( const char *format, ... );

private:
	// Top-level identifier parsers
	typedef void (Parser::*IdentifierReader)();

	void readIdInclude();
	void readIdWarn();
	void readIdProgress();
	void readIdPerformance();
	void readIdLua();
	void readIdParameter();

	void readIdSeed();
	void readIdTime();
	void readIdStopTime();

	void readIdCompartment();
	void readIdPopulation();
	void readIdReaction();
	void readIdQueue();

	void readIdMoleculeReadout();
	void readIdReadoutInterval();
	void readIdOutputFile();
	void readIdOutputFileHeader();

	void readIdSaveInterval();
	void readIdSaveFile();
	void readIdSaveIndex();
	void readIdSaveNow();

	void readIdBatchCount();
	void readIdBatchInit();
	void readIdBatchThreads();

	// Top-level parsing commands
	void readIdData( const char *id, ParseStream *in = NULL );
	void readIdentifiers( ParseStream *in = NULL );

	// Error conditions
	void error( int backStep, const char *format, ... );
	void error( const char *format, ... );

private:
	enum ParseMsg {
		PARSE_OK = 0,
		PARSE_INVALID = -2,
		PARSE_EOS = -3,
		PARSE_TOO_LONG = -4
	};

	// Low-level parsing functions

	// Strips the whitespace from the end of s
	static void stripWhitespace( char *s );

	// Read a CID from the stream
	// Regexp: [a-z_][0-9a-z_]*
	// Fills up the identifier array as it reads it
	// maxLen should be the capacity of the identifier array
	// Returns:
	//   >0 the length of the identifier read
	//   PARSE_EOS if we are already at the end of the stream
	//   PARSE_INVALID if the first character is non-alpha
	//   PARSE_TOO_LONG if the maximum length is reached ('\0' is automatically appended to the portion that was read)
	int readCID( char *identifier, int maxLen );

	// Reads a SID from the stream
	// Regexp: CID(\.CID)*
	// Returns the same as ReadCID
	int readSID( char *sid, int maxLen );

	// Reads an int from the stream
	// Regexp: -?[0-9]+ OR [0-9]+
	// Returns:
	//   PARSE_OK on success
	//   PARSE_EOS if the end of the stream was reached
	//   PARSE_INVALID if the first character is not a digit or a negative
	//   PARSE_INVALID if the first character is -, but the second is not a digit
	int readInt( int &i );
	int readUInt( int &i );

	// Reads a lua expression from the stream and evaluates it
	// Returns:
	//   >0 on success.. returns the number of reals extracted
	//   PARSE_EOS if the end of the stream was reached
	int readLuaReals( double *d, int n, const char *blockname, char delimiter = 0 );
	// Reads a lua expression from the streams and evaluates it repeatedly
	int readLuaRealBatch( double *d, int n, char *blockname, char delimiter = 0 );

	// Helper for readIdCompartment
	void readCompartmentAddress();

	// Helpers for readIdReaction
	void readReactant( double &c );
	sgns2::RateFunction readRate( std::stringstream &in, double &c, int consumes, bool isSplit );
	void readProduct();
	sgns2::RuntimeDistribution readDelay( std::stringstream &in );
	bool readLabeledLuaReals( std::stringstream &in, char *label, uint labelMaxLen,
		const char *context, bool forceLabel = false );

	// Top-level parsing helpers
	void readIdData( const char *id, char end, char end2 = '\0' );
	void readIdDataBlock( const char *id );
	bool readIdentifier();

	// Low-level error function
	void throwError( int backStep, const char *error );

	// Parse function for Lua
	static int lua_parse( lua_State *L );

	bool showWarnings; // Squelch warnings?
	bool moleculeReadout; // Default molecule readout

	lua_State *L; // The global Lua state
	LuaRandom *luaGlobalRandom; // Lua's RNG
	bool runtimeLua; // Are there runtime Lua components?
	ParseStream *in; // Current parsing source
	
	typedef std::map< const char *, IdentifierReader, StringCmpLt_Functor > IDReaderMap;
	IDReaderMap idReaders; // identifier(string)->IdentifierReader map

	uint includeDepth; // Current number of nested includes

	//jmp_buf parseOrigin;
	Error curError; // The last error thrown
	std::string errorMsg; // Storage for error strings
	std::string errorCtx;
	std::string errorLineSoFar;

	ParseListener *target; // The target ParseListener
};

} // namespace parse
} // namespace sgns2

#endif // PARSER_H
