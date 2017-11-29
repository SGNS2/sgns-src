
// See parser.h for a description of the contents of this file.

#include "stdafx.h"

#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <cfloat>
#include <cassert>

#include "platform.h"
#include "parser.h"
#include "parsestream.h"
#include "rng.h"
#include "luarandom.h"

namespace sgns2 {
namespace parse {

const uint MAX_IDENTIFIER_LEN = 32;
const uint MAX_ELEMENT_NAME_LEN = 64;
const uint MAX_COMP_NAME_LEN = 64;
const uint MAX_RXN_NAME_LEN = 64;

/* Deprecated error propagation mechanism - to be removed completely
#define PE_THROW longjmp( parseOrigin, 1 )
#define PE_BEGINFRAME { bool propErr_ = false; jmp_buf oldOrig_; memcpy( &oldOrig_, &parseOrigin, sizeof( jmp_buf ) ); {
#define PE_ENDFRAME } if( propErr_ ) PE_THROW; memcpy( &parseOrigin, &oldOrig_, sizeof( jmp_buf ) ); }
#define PE_BEGINFRAME_NP PE_BEGINFRAME
#define PE_ENDFRAME_NP(onErr) } memcpy( &parseOrigin, &oldOrig_, sizeof( jmp_buf ) ); if( propErr_ ) { onErr; } }
#define PE_TRY if( setjmp( parseOrigin ) ) propErr_ = true; else
*/

#define PE_THROW throw 1u;
#define PE_BEGINFRAME
#define PE_ENDFRAME
#define PE_BEGINFRAME_NP //try {
#define PE_ENDFRAME_NP(onErr) //} catch( unsigned ) { onErr; }
#define PE_TRY


// ---------------------------------------------------------------------------
Parser::Parser( sgns2::parse::ParseListener *target )
: showWarnings(true)
, moleculeReadout(true)
, runtimeLua(false)
, in(NULL)
, includeDepth(0)
, target(target)
{
	// Initialize Lua
	L = luaL_newstate();
	luaL_openlibs( L );

	// Lua random number generator
	LuaRandom::init_lua(L);
	LuaRandom::l_create(L);
	luaGlobalRandom = static_cast<LuaRandom*>(luaL_checkudata(L, -1, LUARANDOM_META));
	lua_setglobal(L, "random");

	// Lua parse function
	lua_pushlightuserdata( L, this );
	lua_pushcclosure( L, Parser::lua_parse, 1 );
	lua_setglobal( L, "parse" );

	// Add all identifier parsers
	idReaders["include"] = &Parser::readIdInclude;
	idReaders["warn"] = &Parser::readIdWarn;
	idReaders["lua"] = &Parser::readIdLua;
	idReaders["parameter"] = &Parser::readIdParameter;

	idReaders["seed"] = &Parser::readIdSeed;
	idReaders["time"] = &Parser::readIdTime;
	idReaders["stop_time"] = &Parser::readIdStopTime;

	idReaders["compartment"] = &Parser::readIdCompartment;
	idReaders["population"] = &Parser::readIdPopulation;
	idReaders["reaction"] = &Parser::readIdReaction;
	idReaders["queue"] = &Parser::readIdQueue;

	idReaders["molecule_readout"] = &Parser::readIdMoleculeReadout;
	idReaders["readout_interval"] = &Parser::readIdReadoutInterval;
	idReaders["output_file"] = &Parser::readIdOutputFile;
	idReaders["output_file_header"] = &Parser::readIdOutputFileHeader;

	//idReaders["save_interval"] = &Parser::readIdSaveInterval;
	//idReaders["save_file"] = &Parser::readIdSaveFile;
	//idReaders["save_index"] = &Parser::readIdSaveIndex;
	//idReaders["save_now"] = &Parser::readIdSaveNow;
	//idReaders["save_at"] = &Parser::readIdSaveAt;

	idReaders["batch_count"] = &Parser::readIdBatchCount;
	//idReaders["batch_init"] = &Parser::readIdBatchInit;
	idReaders["batch_threads"] = &Parser::readIdBatchThreads;
}

// ---------------------------------------------------------------------------
Parser::~Parser() {
}

// ---------------------------------------------------------------------------
bool Parser::parse( const char *context, std::istream &in ) {
	PE_BEGINFRAME_NP
		ParseStream pin( in, context );
		PE_TRY readIdentifiers( &pin );
	PE_ENDFRAME_NP( return false );
	return true;
}

// ---------------------------------------------------------------------------
bool Parser::parse( const char *context, const char *buffer ) {
	PE_BEGINFRAME_NP
		std::stringstream ss;
		ss << buffer;
		ParseStream pin( ss, context );
		PE_TRY readIdentifiers( &pin );
	PE_ENDFRAME_NP( return false );
	return true;
}

// ---------------------------------------------------------------------------
bool Parser::parse( const char *context, const char *identifier, const char *data ) {
	PE_BEGINFRAME_NP
		std::stringstream ss;
		ss << data;
		ParseStream pin( ss, context );
		PE_TRY readIdData( identifier, &pin );
		if( pin.sget() >= 0 ) {
			this->in = &pin;
			error( "Unexpected symbols" );
		}
	PE_ENDFRAME_NP( return false );
	return true;
}

// ---------------------------------------------------------------------------
const sgns2::parse::Error *Parser::getLastError() {
	return &curError;
}

// ---------------------------------------------------------------------------
void Parser::addLuaClosure( const char *name, lua_CFunction fn ) {
	lua_register( L, name, fn );
}

// ---------------------------------------------------------------------------
::lua_State *Parser::getL() {
	return L;
}

// ---------------------------------------------------------------------------
bool Parser::hasRuntimeLua() {
	return runtimeLua;
}

// ---------------------------------------------------------------------------
void Parser::setHasRuntimeLua() {
	runtimeLua = true;
}

// ---------------------------------------------------------------------------
bool Parser::runLua( const char *context, const char *s ) {
	if( luaL_loadstring( L, s ) || lua_pcall( L, 0, 0, 0 ) ) {
		errorMsg = lua_tostring( L, -1 );
		errorCtx = context;
		new( &curError ) Error( errorMsg.c_str(), errorCtx.c_str(), "", 0, 0 );
		lua_pop( L, 1 );
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
int Parser::getLuaReals( const char *src, const char *s, double *reals, int maxCount ) {
	std::stringstream ss;
	ss << s;
	ParseStream in( ss, src );
	ParseStream *oldIn = this->in;
	this->in = &in;
	int n = readLuaReals( reals, maxCount, src );
	this->in = oldIn;
	return n;
}

// ---------------------------------------------------------------------------
int Parser::getLuaFunction( const char *src, const char *s ) {
	if( 0 == luaL_loadstring( L, s ) ) {
		if( 0 == lua_pcall( L, 0, 1, 0 ) ) {
			if( !lua_isfunction( L, -1 ) )
				error( "Error in %s: %s", src, "Expected function" );
			return luaL_ref( L, LUA_REGISTRYINDEX );
		} else {
			error( "Error in %s: %s", src, lua_tostring( L, -1 ) );
		}
	} else {
		error( "Error in %s: %s", src, lua_tostring( L, -1 ) );
	}
	return 0;
}

/*
// ---------------------------------------------------------------------------
void Parser::raiseError( const char *format, ... ) {
	char msg[256];
	va_list args;
	va_start(args, format);
	if( vsnprintf( msg, 256, format, args ) < 0 )
		strcpy( msg, "[INTERNAL] Failed to create the error message (double fail!)" );

	//errorMsg = msg;
	//errorCtx = context;
	//new( &curError ) Error( errorMsg.c_str(), errorCtx.c_str(), "", 0, 0 );
	throwError( 0, msg );
	PE_THROW;
}
*/
// ---------------------------------------------------------------------------
/*
Parser::ParseException::ParseException( ParseStream *at, const char *message, int charPos )
: source(at->getSource()), lineNo(at->getLineNo()), msg(message)
, charPos(charPos >= 0 ? charPos : at->getLineChar()), line(at->getCurLine())
{
}
*/

// ---------------------------------------------------------------------------
void Parser::readIdInclude() {
	// include <filename>

	// Crude check for recursive includes
	if( includeDepth > 16 ) {
		error( "Includes nested over 16 levels deep" );
		return;
	}

	// Read the rest of the input
	char filename[256];
	int len = 0;
	int ch;
	in->strip();
	while( (ch = in->get()) >= 0 ) {
		if( len == 255 )
			error( 255, "Included filename is too long (max 255 characters)" );
		filename[len++] = (char)ch;
	}
	if( len == 0 )
		error( "Expected filename" );

	filename[len] = '\0';

	PE_BEGINFRAME
	if( len == 1 && filename[0] == '-' ) {
		// Read from stdin
		includeDepth++;
		ParseStream pin( std::cin, "stdin" );
		PE_TRY readIdentifiers( &pin );
		includeDepth--;
	} else {
		// Open the file and read it in
		std::ifstream fin( filename );
		if( fin ) {
			includeDepth++;
			ParseStream pin( fin, filename );
			PE_TRY readIdentifiers( &pin );
			includeDepth--;
		} else {
			error( len - 1, "Failed to open %s for reading", filename );
		}
	}
	PE_ENDFRAME
}

// ---------------------------------------------------------------------------
void Parser::readIdWarn() {
	// warn [(all|off)]
	// Ommitted implies all
	//  all - Shows all warnings
	//  off - Disable all warnings

	int ch = in->sget();
	bool good = false;
	if( ch < 0 ) {
		showWarnings = true;
		return;
	} else if( ch == 'a' ) {
		if( in->get() == 'l' && in->get() == 'l' ) {
			showWarnings = true;
			good = true;
		}
	} else if( ch == 'o' ) {
		if( in->get() == 'f' && in->get() == 'f' ) {
			showWarnings = false;
			good = true;
		}
	}
	if( !good )
		error( "Expected \'all\' or \'off\'" );
}

/*
// ---------------------------------------------------------------------------
void Parser::readIdProgress() {
	params.silent = false;
}

// ---------------------------------------------------------------------------
void Parser::readIdPerformance() {
	params.printperf = true;
}
*/

// ---------------------------------------------------------------------------
void Parser::readIdLua() {
	// lua <lua block>

	PE_BEGINFRAME
		std::stringstream ss;
		int ch;

		// Make Lua give correct line numbers
		for( uint i = in->getLineNo()-1; i > 0; i-- )
			ss << '\n';

		while( (ch = in->get()) >= 0 )
			ss << (char)ch;

		PE_TRY {
			if( luaL_loadbuffer( L, ss.str().c_str(), ss.str().length(), in->getSource() ) == 0 ) {
				if( lua_pcall ( L, 0, 0, 0 ) != 0 ) {
					error( "Lua run-time error: %s", lua_tostring( L, -1 ) );
					lua_pop( L, 1 );
				}
			} else {
				error( "Lua error: %s", lua_tostring( L, -1 ) );
				lua_pop( L, 1 );
			}
		}
	PE_ENDFRAME
}

// ---------------------------------------------------------------------------
void Parser::readIdParameter() {
	// parameter block

	char paramName[MAX_IDENTIFIER_LEN];
	int r = readCID( paramName, MAX_IDENTIFIER_LEN );

	if( r <= 0 ) {
		if( r == PARSE_EOS || r == PARSE_INVALID ) {
			error( "Expected parameter name" );
		} else if( r == PARSE_TOO_LONG ) {
			error( MAX_COMP_NAME_LEN, "Parameter name is too long" );
		}
		return;
	}

	if( in->sget() != '=' )
		error( "Expected value of %s", paramName );

	lua_getglobal( L, paramName );
	if( lua_isnil( L, -1 ) ) {
		lua_pop( L, 1 );

		double value;
		r = readLuaReals( &value, 1, paramName );
		if( r <= 0 )
			error( "Expected value of %s", paramName );
		lua_pushnumber( L, value );
		lua_setglobal( L, paramName );
	} else {
		while( in->get() >= 0 ) { }
		lua_pop( L, 1 );
	}
}

// ---------------------------------------------------------------------------
void Parser::readIdSeed() {
	// seed [int]
	double newSeed;
	uint newSeedInt;
	int r = readLuaReals( &newSeed, 1, "random seed" );
	if (r <= 0) {
		if (r == PARSE_EOS) {
			// Ensure different seeds between runs
#ifdef _WIN32
			newSeedInt = timeGetTime() ^ (GetCurrentProcessId() << 7);
#else
			newSeedInt = time( NULL ) ^ (getpid() << 7);
#endif
			// Ensure different seeds between invocations
			static uint skew = 1234;
			skew = (skew >> 3) + (skew << 2) + (skew << 5) + 0x1803;
			newSeedInt ^= skew;
			newSeed = (double)newSeedInt;
		} else {
			error( "Expected seed" );
		}
	}

	target->setParameterD( sgns2::parse::ParseListener::SEED, newSeed );
	luaGlobalRandom->getRNG()->seed(uint64(newSeed));
}

// ---------------------------------------------------------------------------
void Parser::readIdTime() {
	// time <real>
	int r;
	double time;
	if( (r = readLuaReals( &time, 1, "start time" )) <= 0) {
		error( "Expected start time" );
	} else if( time < 0.0 ) {
		// Warn for negative start time
		warning( "Negative start time" );
	}
	target->setParameterD( sgns2::parse::ParseListener::START_TIME, time );
}

// ---------------------------------------------------------------------------
void Parser::readIdStopTime() {
	// stop_time <real>
	int r;
	double time;
	if( (r = readLuaReals( &time, 1, "stop time" )) <= 0) {
		error( "Expected stop time" );
	} else if( time < 0.0 ) {
		// Warn for negative stop time
		warning( "Negative stop time." );
	}
	target->setParameterD( sgns2::parse::ParseListener::STOP_TIME, time );
}

// ---------------------------------------------------------------------------
void Parser::readIdCompartment() {
	// compartment <type> <name>[@<where>]
	// compartment type <name>[@<supertype>]
	// compartment instance <name>[\[<count>\]]{@<address>}

	char compType[MAX_COMP_NAME_LEN];
	int r = readCID( compType, MAX_COMP_NAME_LEN );
	if( r < 0 ) {
		if( r == PARSE_EOS || r == PARSE_INVALID ) {
			error( "Expected compartment type, 'type' or 'instance'" );
		} else if( r == PARSE_TOO_LONG ) {
			error( MAX_COMP_NAME_LEN, "Compartment type name is too long" );
		}
		return;
	}

	if( strcmp( compType, "type" ) == 0 ) {
		bool hideCompartment = false;
		if( in->speek() == '#' ) {
			in->get();
			in->strip();
			hideCompartment = true;
		}

		int r = readCID( compType, MAX_COMP_NAME_LEN );
		if( r < 0 ) {
			if( r == PARSE_EOS || r == PARSE_INVALID ) {
				error( "Expected compartment type name" );
			} else if( r == PARSE_TOO_LONG ) {
				error( MAX_COMP_NAME_LEN, "Compartment name is too long" );
			}
			return;
		}
		if( strcmp( compType, "type" ) == 0 || strcmp( compType, "instance" ) == 0 )
			error( "Reserved compartment type name" );

		if( in->speek() == '@' ) {
			in->get();
			in->strip();
			// The type is in a compartment
			char superCompName[MAX_COMP_NAME_LEN];
			r = readCID( superCompName, MAX_COMP_NAME_LEN );
			if( r < 0 ) {
				if( r == PARSE_EOS || r == PARSE_INVALID ) {
					error( "Expected supertype name" );
				} else if( r == PARSE_TOO_LONG ) {
					error( MAX_COMP_NAME_LEN, "Compartment type name is too long" );
				}
				return;
			}
			target->selectCompartmentType( superCompName );
		} else {
			target->selectCompartmentType( NULL );
		}
		
		target->createCompartmentType( compType );
		target->selectCompartment( NULL );
		target->selectCompartment( compType );
		target->outputCompartment( !hideCompartment );
		return;
	}
	
	if( strcmp( compType, "instance" ) == 0 ) {
		// Anonymous compartment instantiation
		uint n = 1;
		in->strip();

		r = readCID( compType, MAX_COMP_NAME_LEN );
		if( r < 0 ) {
			if( r == PARSE_EOS || r == PARSE_INVALID ) {
				error( "Expected compartment type" );
			} else if( r == PARSE_TOO_LONG ) {
				error( MAX_COMP_NAME_LEN, "Compartment type name is too long" );
			}
			return;
		}

		target->selectCompartmentType( compType );

		if( in->speek() == '[' ) {
			// Lua number
			in->get();
			double count;
			r = readLuaReals( &count, 1, "instance count", ']' );
			if( r < 0 )
				error( "Expected anonymous compartment instance count" );
			in->get();
			n = (uint)floor( count );
		}

		target->selectCompartment( NULL );
		if( in->speek() == '@' ) {
			in->get();
			in->strip();
			char compWhere[MAX_COMP_NAME_LEN];
			r = readCID( compWhere, MAX_COMP_NAME_LEN );
			if( r < 0 ) {
				if( r == PARSE_EOS || r == PARSE_INVALID ) {
					error( "Expected compartment name" );
				} else if( r == PARSE_TOO_LONG ) {
					error( MAX_COMP_NAME_LEN, "Compartment name is too long" );
				}
				return;
			}
			target->selectCompartment( compWhere );
		}

		target->instantiateCompartment( n );
		return;
	}

	// compartment <type> <name>[@<where>]

	target->selectCompartmentType( compType );

	in->strip();
	char compName[MAX_COMP_NAME_LEN];
	r = readCID( compName, MAX_COMP_NAME_LEN );
	if( r < 0 ) {
		if( r == PARSE_EOS || r == PARSE_INVALID ) {
			error( "Expected compartment name" );
		} else if( r == PARSE_TOO_LONG ) {
			error( MAX_COMP_NAME_LEN, "Compartment name is too long" );
		}
		return;
	}
	if( strcmp( compName, "type" ) == 0 || strcmp( compName, "instance" ) == 0 )
		error( "Reserved compartment name" );

	target->selectCompartment( NULL );
	if( in->speek() == '@' ) {
		in->get();
		in->strip();
		char compWhere[MAX_COMP_NAME_LEN];
		r = readCID( compWhere, MAX_COMP_NAME_LEN );
		if( r < 0 ) {
			if( r == PARSE_EOS || r == PARSE_INVALID ) {
				error( "Expected compartment name" );
			} else if( r == PARSE_TOO_LONG ) {
				error( MAX_COMP_NAME_LEN, "Compartment name is too long" );
			}
			return;
		}
		target->selectCompartment( compWhere );
	}

	target->instantiateCompartment( compName );
}

// ---------------------------------------------------------------------------
void Parser::readCompartmentAddress() {
	// Compartment instantiation
	target->selectCompartment( NULL );

	if( in->speek() == '@' ) {
		in->get();
		in->strip();
		char compType[MAX_COMP_NAME_LEN];
		int r = readCID( compType, MAX_COMP_NAME_LEN );
		if( r == PARSE_EOS || r == PARSE_INVALID ) {
			error( "Expected compartment type or name" );
		} else if( r == PARSE_TOO_LONG ) {
			error( MAX_COMP_NAME_LEN, "Compartment name is too long" );
		}

		if( in->speek() == '@' ) {
			in->get();
			in->strip();
			char compName[MAX_COMP_NAME_LEN];
			r = readCID( compName, MAX_COMP_NAME_LEN );
			if( r == PARSE_EOS || r == PARSE_INVALID ) {
				error( "Expected compartment name" );
			} else if( r == PARSE_TOO_LONG ) {
				error( MAX_COMP_NAME_LEN, "Compartment name is too long" );
			}

			target->selectCompartment( compName );
		}

		target->selectCompartment( compType );
	}
}

// ---------------------------------------------------------------------------
void Parser::readIdPopulation() {
	// population [#|!] s-id [@c-id] [(+=,-=,=) int]
	char sid[MAX_ELEMENT_NAME_LEN];
	bool setVisible = false;
	bool visible = moleculeReadout;
	
	int ch = in->sget();
	if( ch < 0 ) {
		error( "Expected species name, !, or #" );
		return;
	} else if (ch == '!') {
		visible = true;
		setVisible = true;
	} else if (ch == '#') {
		visible = false;
		setVisible = true;
	} else {
		in->putback( (char)ch );
	}
	in->strip();

	int len = readSID( sid, MAX_ELEMENT_NAME_LEN );
	if( len < 0 ) {
		if( len == PARSE_EOS || len == PARSE_INVALID ) {
			error( "Expected species name" );
		} else if( len == PARSE_TOO_LONG ) {
			error( MAX_ELEMENT_NAME_LEN, "Species name is too long" );
		}
		return;
	}

	target->selectChemical( sid, moleculeReadout );

	readCompartmentAddress();

	ch = in->sget();
	bool neg = false;
	bool addOld = false;
	bool add0 = false;
	if( ch < 0 ) {
		// Simply has the s-id
		add0 = true;
		addOld = true;
	} else if( ch == '-' || ch == '+' ) { // += and -=
		neg = ch == '-';
		addOld = true;

		ch = in->get();
	}
	if( !add0 && ch != '=' ) { // =
		error( addOld ? 1 : 0, "Expected +=, -=, or =" );
		return;
	}

	// Get the Element
	target->selectChemical( sid, visible );
	if( setVisible )
		target->setChemicalVisible( visible );

	// Get the new amounts of the element
	double val = 0.0;
	if( !add0 ) {
		int r = readLuaReals( &val, 1, "population" );
		if( r < 0 )
			error( "Expected population" );
		if( neg )
			val = -val;
	}

	// Change the population
	sgns2::RuntimeDistribution valDistr = sgns2::RuntimeDistribution::DeltaDistribution( val );
	target->setPopulation( &valDistr, addOld );
}

// ---------------------------------------------------------------------------
void Parser::readIdReaction() {
	// rxnname localization reaction reactant-list --\[luacode\]--> product-list
	// reactant-list ::= [reactant (+ reactant)*]
	// reactant ::= [\*][int|\[luacode\]]s-id[\(rate-function\)]
	// rate-function ::= c-id:luacode(,luacode)*
	// product-list ::= [product (+ product)*]
	// product ::= [int|\[luacode\]]s-id[\(delay\)]
	// delay ::= [c-id:]luacode(,luacode)*

	double c = 1.0;

	// Reaction Name
	if( in->speek() == '\"' ) {
		in->get();
		int ch;
		unsigned len = 0;
		char name[MAX_RXN_NAME_LEN];
		while( (ch = in->get()) >= 0 && ch != '\"' ) {
			name[len++] = (char)ch;
			if( len >= MAX_RXN_NAME_LEN )
				error( len, "Reaction name is too long" );
		}
		name[len] = '\0';
		target->newReaction( name );
	} else {
		// New anonymous reaction
		target->newReaction( NULL );
	}

	// Reactants
	if( in->speek() != '-' ) {
		while( true ) {
			// Read one reactant
			readReactant( c );
			if( in->speek() != '+' )
				break;
			in->get();
		}
	}

	// Reaction arrow --[
	if( !(in->get() == '-' && in->get() == '-') )
		error( "Expected reaction arrow '--['" );
	if( in->peek() == '(' ) {
		// H-function
		in->get();
		int lineStart = in->getLineNo();
		std::stringstream ss;
		if( !in->readLua( ss, ')' ) )
			error( "Unterminated h-function specification from line %d. Missing ')'?", lineStart );
		in->get();
		in->strip();
		uint nParams;
		double params[16];
		char hFuncName[32];
		readLabeledLuaReals( ss, hFuncName, sizeof(hFuncName), "h-function", true );

		nParams = (uint)lua_gettop(L);
		if( 0 == strcmp( hFuncName, "lua" ) ) {
			if( nParams != 1 || !lua_isfunction( L, 1 ) )
				error( "lua h-function expects one function as a parameter" );
			params[0] = (double)luaL_ref( L, 1 );
		} else {
			for( uint i = 0; i < nParams; i++ ) {
				if( !lua_isnumber( L, i + 1 ) )
					error( "Expected H-function parameter. Got %s", lua_typename( L, lua_type( L, i + 1 ) ) );
				params[i] = lua_tonumber( L, i + 1 );
			}
			lua_pop( L, (int)nParams );
		}
		target->overrideH( hFuncName, &params[0], nParams );
	}
	if( in->get() == '[' ) {
		double givenC;
		int r = readLuaReals( &givenC, 1, "stochastic constant", ']' );
		if( r < 0 )
			error( "Expected reaction stochastic constant" );
		c *= givenC;
		if( c < 0.0 )
			error( "Reaction's stochastic constant is negative" );

		in->get();
	} else {
		error( "Expected reaction arrow '--['" );
	}
	// -->
	if( !(in->get() == '-' && in->get() == '-' && in->get() == '>') )
		error( "Expected reaction arrow ']-->'" );

	// Products
	if( in->speek() >= 0 && in->speek() != '!' ) {
		while( true ) {
			// Read one product
			readProduct();
			if( in->speek() != '+' )
				break;
			in->get();
		}
	}

	// Script function
	if( in->speek() == '!' ) {
		in->get();
		// TODO
		error( 1, "Lua callback NYI" );
	}

	target->finishReaction( c );
}

void Parser::readReactant( double &c ) {
	PE_BEGINFRAME
	
	int r;
	if( in->speek() == '@' ) {
		// Compartment destruction
		in->get();
		in->strip();
		
		char compName[MAX_COMP_NAME_LEN];
		r = readCID( compName, MAX_COMP_NAME_LEN );
		if( r < 0 ) {
			if( r == PARSE_TOO_LONG )
				error( MAX_COMP_NAME_LEN, "Compartment name is too long" );
			error( "Expected compartment type" );
		}

		target->selectCompartment( NULL );
		target->selectCompartment( compName );
		target->eatCompartment();
		return;
	}

	int n = 1;
	bool isVirtual = false;
	int ch = in->sget();
	if( charIsAlphaC( (char)ch ) ) {
		// Either the reactant alone OR a split
		in->putback( (char)ch );
	} else if( ch == '*' ) {
		// Virtual reaction
		isVirtual = true;
		n = 0;
	} else if( ch == '[' ) {
		// Lua number
		double count;
		r = readLuaReals( &count, 1, "reactant stoichiometry", ']' );
		if( r < 0 )
			error( "Expected reactant stoichiometry" );
		n = (int)floor( count );
		in->get();
	} else if( charIsDigit( (char)ch ) ) {
		// Straight number
		in->putback( (char)ch );
		int count = 0;
		r = readUInt( count );
		n = (int)count;
	} else {
		error( "Expected reactant" );
	}

	char split[MAX_ELEMENT_NAME_LEN];
	bool hasSplit = false;
	bool hasSplitParams = false;
	
	std::stringstream splitSS;

	char sid[MAX_ELEMENT_NAME_LEN];
	bool hasRate = false;
	std::stringstream rateSS;

	bool hasLoc = false;
	bool isCompartmentSplit = false;

	PE_TRY {
		target->selectCompartment( NULL );
		while( true ) {
			// Get the SID
			in->strip();
			r = readSID( sid, MAX_ELEMENT_NAME_LEN );
			if( r < 0 ) {
				if( r == PARSE_TOO_LONG )
					error( MAX_ELEMENT_NAME_LEN, "Reactant name is too long" );
				if( r == PARSE_INVALID && hasSplit && in->speek() == '@' ) {
					isCompartmentSplit = true;
				} else {
					error( "Expected reactant" );
				}
			}



			// @Compartment
			if( in->speek() == '@' ) {
				in->get();
				in->strip();
				
				char compName[MAX_COMP_NAME_LEN];
				r = readCID( compName, MAX_COMP_NAME_LEN );
				if( r < 0 ) {
					if( r == PARSE_TOO_LONG )
						error( MAX_COMP_NAME_LEN, "Compartment name is too long" );
					error( "Expected compartment type" );
				}

				target->selectCompartment( compName );
			}

			// Is there a rate?
			if( in->speek() == '(' ) {
				in->get();
				int lineStart = in->getLineNo();
				if( !in->readLua( rateSS, ')' ) )
					error( "Unterminated reactant function from line %d. Missing ')'?", lineStart );
				in->get();
				hasRate = true;
			}

			// Is the sid and rate actually a split?
			if( hasLoc || hasSplit || in->speek() != ':' ) {
				// Nope.. done!
				break;
			}

			// This is a split.. copy the sid and rate into the split parameters
			in->get();
			hasSplit = true;
			hasSplitParams = hasRate;
			hasRate = false;
			strcpy( split, sid );
			splitSS << rateSS.str();
			rateSS.str("");
			rateSS.clear();
		}
		/*
		// check for is biased
		unsigned index_biased = (unsigned)(strlen(split)-1);
		if(split[index_biased] == '2'  ){
			isbiased = true;
			split[index_biased] = '\0';
		}
		*/
		// Get the element
		if( !isCompartmentSplit )
			target->selectChemical( sid, moleculeReadout );
		
		bool isUnBiased = false;
		// Interpret the split
		if( hasSplit ) {
			enum {
				SPLIT_ALL_OR_NONE = 0, SPLIT_BETA_BINO, SPLIT_BINO, SPLIT_PAIR, SPLIT_TAKE,
				SPLIT_TAKE_ROUND, SPLIT_COPY, SPLIT_RANGE, SPLIT_BINO_P, SPLIT_COUNT
			} splitType = SPLIT_COUNT;
			if( 0 == strcmp( split, "allornothing" ) ) {
				splitType = SPLIT_ALL_OR_NONE;
			} else if( 0 == strcmp( split, "betapart" ) ) {
				splitType = SPLIT_BETA_BINO;
			} else if( 0 == strcmp( split, "betapart2" ) ) {
				splitType = SPLIT_BETA_BINO;
				isUnBiased = true;
			} else if( 0 == strcmp( split, "split" ) ) {
				splitType = SPLIT_BINO;
			} else if( 0 == strcmp( split, "split2" ) ) {
				splitType = SPLIT_BINO;
				isUnBiased = true;
			} else if( 0 == strcmp( split, "split_P" ) ) {
				splitType = SPLIT_BINO_P;
			} else if( 0 == strcmp( split, "pairpart" ) ) {
				splitType = SPLIT_PAIR;
			} else if( 0 == strcmp( split, "take_round" ) ) {
				splitType = SPLIT_TAKE_ROUND;
			} else if( 0 == strcmp( split, "take" ) ) {
				splitType = SPLIT_TAKE;
			} else if( 0 == strcmp( split, "copy" ) ) {
				splitType = SPLIT_COPY;
				isVirtual = true;
			} else if( 0 == strcmp( split, "range" ) ) {
				splitType = SPLIT_RANGE;
			} else if( 0 == strcmp( split, "steal" ) ) {
				error( "Stealing is illegal" );
			} else {
				error( "Unknown split distribution" );
			}
			const double paramRestrictions[SPLIT_COUNT][4] = {
				{ 0.5, 1.0, 0.0, 1.0 }, // SPLIT_ALL
				{ 0.5, 1.0, DBL_MIN, DBL_MAX }, // SPLIT_BETABINO
				{ 0.5, 1.0, 0.0, 1.0 }, // SPLIT_BINO
				{ 0.5, 1.0, 0.0, 1.0 }, // SPLIT_PAIR
				{ 1.0, 1.0, 0.0, 1.0 }, // SPLIT_TAKE
				{ 1.0, 1.0, 0.0, 1.0 }, // SPLIT_TAKE_ROUND
				{ 1.0, 1.0, -(DBL_MAX), DBL_MAX }, // SPLIT_COPY
				{ 1.0, 1.0, 0.0, DBL_MAX },  // SPLIT_RANGE
				{ 1.0, 1.0, 1.0, DBL_MAX }  // SPLIT_BINO_P
			};
			double param[2];
			if( hasSplitParams ) {
				ParseStream paramStream( splitSS, "split parameters" );
				ParseStream *oldIn = in;
				in = &paramStream;
				r = readLuaReals( &param[0],2, "split parameters" );
				in = oldIn;
				if( r < 0 )
					error( "Expected split parameters" );
				if( splitType == SPLIT_BETA_BINO && r < 2){
					param[1] = param[0];
					param[0] = 1;
				}
				if( (splitType == SPLIT_PAIR || splitType == SPLIT_RANGE) && r < 2){
					param[1] = 1;
				}

				double leftBound = paramRestrictions[splitType][2];
				double rightBound = paramRestrictions[splitType][3];
				if( splitType == SPLIT_TAKE && isVirtual ){
					leftBound = -(DBL_MAX);
					rightBound = DBL_MAX ;
				}
				if( param[0] < leftBound || param[0] > rightBound )
					error( "%s split distribution's parameter must be in the range [%f %f]", split, leftBound, rightBound );
				if( r > 1 && (param[1] < paramRestrictions[splitType][2] || param[1] > paramRestrictions[splitType][3] ))
					error( "%s split distribution's parameter must be in the range [%f %f]", split, paramRestrictions[splitType][2], paramRestrictions[splitType][3] );
			} else {
				param[0] = paramRestrictions[splitType][0];
				param[1] = paramRestrictions[splitType][0];
			}

			
			SplitFunction split;
			switch( splitType ) {
				case SPLIT_ALL_OR_NONE:
					split = SplitFunction::AllOrNothing( param[0], isVirtual );
					break;
				case SPLIT_BETA_BINO:
					split = SplitFunction::BetaBinomialSplit( param[0], param[1], isVirtual, isUnBiased );
					break;	
				case SPLIT_BINO:
					split = SplitFunction::BinomialSplit( param[0], isVirtual, isUnBiased );
					break;
				case SPLIT_PAIR:
					split = SplitFunction::PairSplit( param[0], param[1], isVirtual );
					break;
				case SPLIT_TAKE:
					split = SplitFunction::Take( param[0], isVirtual );
					break;
				case SPLIT_TAKE_ROUND:
					split = SplitFunction::TakeRound( param[0], isVirtual );
					break;
				case SPLIT_COPY:
					split = SplitFunction::Take( param[0], isVirtual );
					break;
				case SPLIT_RANGE:{
					if (param[0] > param[1]){
						double temp = param[0];
						param[0] = param[1];
						param[1] = temp;
					}
					split = SplitFunction::Range( param[0], param[1], isVirtual );
					break;
					}
				case SPLIT_BINO_P:
					split = SplitFunction::BinomialSplit_P( (int)param[0], (int)param[1], isVirtual, isUnBiased );
					break;
				default: assert( false );
			}

			if( isCompartmentSplit ) {
				target->newSplitCompartment( &split );
			} else {
				target->newSplitReactant( &split );
			}
		} else {
			// Create the normal reactant
			target->newReactant( n );
		}

		// Interpret the reaction rate
		sgns2::RateFunction rf;
		if( hasRate ) {
			rf = readRate( rateSS, c, n, hasSplit );
		} else {
			// Default rates
			if( hasSplit ) {
				// const
				rf = RateFunction::Unit();
			} else if( n <= 1 ) {
				// linear
				rf = RateFunction::Linear();
			} else {
				// gilh:n
				rf = BasicRateFunction::GilH( n );
			}
		}
		target->setRate( &rf );
	}

	PE_ENDFRAME
}

sgns2::RateFunction Parser::readRate( std::stringstream &in, double &c, int consumes, bool isSplit ) {
	char distr[64];
	uint paramBase = (uint)lua_gettop(L);
	double params[3];
	for( uint i = 0; i < 3; i++ )
		params[i] = 1.0;
	readLabeledLuaReals( in, distr, sizeof(distr), "reactant function", true );

	uint nParams = (uint)lua_gettop(L) - paramBase;
	for( uint i = 0; i < nParams; i++ ) {
		if( !lua_isnumber( L, paramBase + i + 1 ) )
			error( "Expected reactant function parameter. Got %s", lua_typename( L, lua_type( L, i + 1 ) ) );
		params[i] = lua_tonumber( L, paramBase + i + 1 );
	}
	lua_pop( L, (int)nParams );

	sgns2::RateFunction rf;
	uint usedParams = 0;
	if( 0 == strcmp( distr, "gilh" ) || 0 == strcmp( distr, "h" ) ) {
		// H-function
		usedParams = 1;
		if( nParams >= 1 ) {
			if( params[0] < 0 )
				error( "Invalid parameter for gilh function" );
			rf = BasicRateFunction::GilH( (int)floor( params[0] ) );
		} else if( isSplit ) {
			error( "Unspecified gilh function given for reactant with split" );
		} else {
			rf = BasicRateFunction::GilH( consumes );
		}
	} else if( 0 == strcmp( distr, "const" ) || 0 == strcmp( distr, "linear" ) ||
		       0 == strcmp( distr, "square" ) || 0 == strcmp( distr, "sqr" ) ||
			   0 == strcmp( distr, "cube" ) ) {
	    // Simple functions for basic powers
		usedParams = 1;
		double power = 1.0;
		switch( distr[1] ) {
			case 'o': power = 0.0; break;
			case 'i': power = 1.0; break;
			case 'q': power = 2.0; break;
			case 'u': power = 3.0; break;
		}
		rf = BasicRateFunction::Pow( power );
		if( nParams >= 1 )
			c *= params[0];
	} else if( 0 == strcmp( distr, "pow" ) ||
		       0 == strcmp( distr, "min" ) || 0 == strcmp( distr, "max" ) ) {
	    // General power, min and max
		usedParams = 2;
		if( nParams < 1 )
			error( "Reactant function '%s' expects at least one parameter", distr );
		if( distr[0] != 'p' && params[0] < 0.0 )
			error( "The first parameter for the %s reactant function must be positive", distr );
		if( nParams > 1 && params[1] < 0.0 )
			error( "The second parameter for the %s reactant function must be positive", distr );
		switch( distr[2] ) {
			case 'w': rf = BasicRateFunction::Pow( params[0] ); break;
			case 'x': rf = BasicRateFunction::Max( params[0] ); break;
			case 'n': rf = BasicRateFunction::Min( params[0] ); break;
		}
		if( nParams >= 2 )
			c *= params[0];
	} else if( 0 == strcmp( distr, "hill" ) || 0 == strcmp( distr, "invhill" ) ) {
		// Hill functions
		usedParams = 2;
		if( params[0] <= 0.0 )
			error( "The first parameter for the %s reactant function must be strictly positive", distr );
		if( distr[0] == 'h' )
			rf = BasicRateFunction::Hill( pow( params[0], params[1] ), params[1] );
		else
			rf = BasicRateFunction::Invhill( pow( params[0], params[1] ), params[1] );
	} else if( 0 == strcmp( distr, "step" ) ) {
		// General step function
		usedParams = 3;
		if( nParams < 1 )
			error( "Reactant function '%s' expects at least one parameter", distr );
		if( params[0] < 0.0 )
			error( "The first parameter for the %s reactant function must be positive", distr );
		if( nParams >= 3 ) {
			// step:a,b,c
		} else {
			// step:a,0,b or 1
			params[2] = nParams == 2 ? params[1] : 1.0;
			params[1] = 0.0;
		}
		// h = X < p1 ? p2 : p3
		double cnorm = 1.0;
		if( fabs(params[1]) <= 0.0 ) {
			if( fabs(params[2]) <= 0.0 ) {
				// step: p1, 0, 0 -> Zero rate function
				cnorm = 0.0;
				rf = RateFunction::Unit();
			} else {
				// step: p1, 0, p3 -> Uses BasicRateFunction::Step with c *= p3
				cnorm = params[2];
				rf = BasicRateFunction::Step( (Population)floor(params[0]), 0.0 );
			}
		} else {
			// step: p1, p2, p3 -> Uses BasicRateFunction::Step2 with c *= p2
			cnorm = params[1];
			rf = BasicRateFunction::Step2( (Population)floor(params[0]), params[2] / params[1] );
		}
		if( fabs( cnorm - 1.0 ) > DBL_EPSILON )
			c *= cnorm;
	} else {
		error( "Unknown reactant function: %s", distr );
	}
	if( nParams > usedParams )
		error( "Extra parameters given to reactant function %s", distr );
	
	return rf;
}

void Parser::readProduct() {
	PE_BEGINFRAME

	int r;
	if( in->speek() == '@' ) {
		// Compartment construction
		in->get();
		in->strip();
		
		char compName[MAX_COMP_NAME_LEN];
		r = readCID( compName, MAX_COMP_NAME_LEN );
		if( r < 0 ) {
			if( r == PARSE_TOO_LONG )
				error( MAX_COMP_NAME_LEN, "Compartment type name is too long" );
			error( "Expected compartment type" );
		}

		target->selectCompartment( NULL );
		target->selectCompartment( compName );
		target->produceCompartment();
		return;
	}

	int n = 1;
	bool isSplit = false;
	bool isCompartmentSplit = false;

	int ch = in->sget();
	if( charIsAlphaC( (char)ch ) ) {
		// Product alone
		in->putback( (char)ch );
	} else if( ch == '[' ) {
		// Lua number
		double count;
		r = readLuaReals( &count, 1, "product stoichiometry", ']' );
		if( r < 0 )
			error( "Expected product stoichiometry" );
		n = (int)floor( count );
		in->get();
	} else if( charIsDigit( (char)ch ) ) {
		// Straight number OR a split
		in->putback( (char)ch );
		int count = 0;
		r = readUInt( count );
		n = (int)count;

		if( in->speek() == ':' ) {
			// A split!
			in->get();
			isSplit = true;
		}
	} else if( ch == ':' ) {
		// An auto-split!
		n = 0;
		isSplit = true;
	} else {
		error( "Expected product" );
	}

	char sid[MAX_ELEMENT_NAME_LEN];
	bool hasDelay = false;
	std::stringstream delaySS;

	PE_TRY {
		// Get the SID
		in->strip();
		r = readSID( sid, MAX_ELEMENT_NAME_LEN );
		if( r < 0 ) {
			if( r == PARSE_TOO_LONG )
				error( MAX_ELEMENT_NAME_LEN, "Product name is too long" );
			if( r == PARSE_INVALID && isSplit && in->speek() == '@' ) {
				isCompartmentSplit = true;
			} else {
				error( "Expected product name" );
			}
		}

		// @Compartment
		target->selectCompartment( NULL );
		if( in->speek() == '@' ) {
			in->get();
			in->strip();
			
			char compName[MAX_COMP_NAME_LEN];
			r = readCID( compName, MAX_COMP_NAME_LEN );
			if( r < 0 ) {
				if( r == PARSE_TOO_LONG )
					error( MAX_COMP_NAME_LEN, "Compartment name is too long" );
				error( "Expected compartment type" );
			}

			target->selectCompartment( compName );
		}

		// Is there a delay?
		if( in->speek() == '(' ) {
			if( isCompartmentSplit )
				error( "Compartment splits cannot be delayed" );
			in->get();
			int lineStart = in->getLineNo();
			if( !in->readLua( delaySS, ')' ) )
				error( "Unterminated product delay from line %d. Missing ')'?", lineStart );
			in->get();
			hasDelay = true;
		}

		if( isCompartmentSplit ) {
			target->newSplitProduct( n, true );
		} else {
			// Get the element
			target->selectChemical( sid, moleculeReadout );

			if( isSplit ) {
				target->newSplitProduct( n, false );
			} else {
				target->newProduct( n );
			}

			if( hasDelay ) {
				sgns2::RuntimeDistribution tau = readDelay( delaySS );
				target->setTau( &tau );
			}
		}
	}

	PE_ENDFRAME
}

sgns2::RuntimeDistribution Parser::readDelay( std::stringstream &in/*, Reaction *rxn, Product *prod*/ ) {
	char distr[64];
	uint paramBase = (uint)lua_gettop(L);
	double params[3];
	for( uint i = 0; i < 3; i++ )
		params[i] = 1.0;
	if( !readLabeledLuaReals( in, distr, sizeof(distr), "delay distribution" ) )
		strcpy( distr, "delta" );

	uint nParams = (uint)lua_gettop(L) - paramBase;
	for( uint i = 0; i < nParams; i++ ) {
		if( !lua_isnumber( L, paramBase + i + 1 ) )
			error( "Expected delay parameter. Got %s", lua_typename( L, lua_type( L, i + 1 ) ) );
		params[i] = lua_tonumber( L, paramBase + i + 1 );
	}
	lua_pop( L, (int)nParams );

	// Preprocessing of parameters for some basic optimization
	uint usedParams = 0;
	if( 0 == strcmp( distr, "delta" ) || 0 == strcmp( distr, "const" ) ) {
		usedParams = 1;
		strcpy( distr, "delta" );
	} else if( 0 == strcmp( distr, "gaussian" ) || 0 == strcmp( distr, "gaus" ) || 0 == strcmp( distr, "normal" ) ) {
		usedParams = 2;
		if( params[1] < 0.0 )
			error( "Gaussian delay distribution's sigma must be >= 0" );
		if( params[1] < DBL_EPSILON ) {
			if( params[0] < 0 )
				error( "Gaussian delay distribution with sigma = 0 has negative mean" );
			else
				strcpy( distr, "delta" );
		} else {
			strcpy( distr, "gaussian" );
		}
	} else if( 0 == strcmp( distr, "gaussian.trunc" ) || 0 == strcmp( distr, "gaus.trunc" ) || 0 == strcmp( distr, "normal.trunc" ) ) {
		usedParams = 2;
		if( params[1] < 0.0 )
			error( "Gaussian delay distribution's sigma must be >= 0" );
		if( params[1] < DBL_EPSILON ) {
			if( params[0] < 0 )
				error( "Gaussian delay distribution with sigma = 0 has negative mean" );
			else
				strcpy( distr, "delta" );
		} else {
			strcpy( distr, "truncgaussian" );
		}
	} else if( 0 == strcmp( distr, "exponential" ) || 0 == strcmp( distr, "exp" ) ) {
		usedParams = 1;
		if( params[0] < DBL_EPSILON )
			error( "Exponential delay distribution's lambda must be > 0" );
		strcpy( distr, "exp" );
	} else if( 0 == strcmp( distr, "gamma" ) || 0 == strcmp( distr, "erlang" ) ) {
		usedParams = 2;
		if( params[0] < 0 || params[1] < 0 )
			error( "Gamma delay distribution's shape and scale parameters must both be >= 0" );
		if( params[0] < DBL_EPSILON || params[1] < DBL_EPSILON ) {
			strcpy( distr, "delta" );
			params[0] = 0.0;
		} else if( fabs( params[0] - 1.0 ) < DBL_EPSILON ) {
			strcpy( distr, "exp" );
			params[0] = 1.0 / params[1];
		} else {
			strcpy( distr, "gamma" );
		}
	} else if( 0 == strcmp( distr, "uniform" ) ) {
		usedParams = 2;
		if( params[0] == params[1] ) {
			strcpy( distr, "delta" );
		} else if( params[0] > params[1] ) {
			std::swap( params[0], params[1] );
		}
	} else if( 0 == strcmp( distr, "chisquare" ) ) {
		usedParams = 1;
		if( params[0] < DBL_EPSILON )
			error( "Chi-Square delay distribution's degrees of freedom parameter must be > 0" );
		params[0] /= 2.0;
		params[1] = 2;
	} else {
		error( "Unknown delay distribution: %s", distr );
	}

	if( nParams < usedParams )
		error( "Too few parameters given to delay distribution %s", distr );
	if( nParams > usedParams )
		error( "Too many parameters given to delay distribution %s", distr );

	if( 0 == strcmp( distr, "delta" ) && params[0] <= 0.0 )
		return sgns2::RuntimeDistribution::DeltaDistribution( 0.0 );

	if( 0 == strcmp( distr, "delta" ) ) {
		// Non-zero Delta
		return sgns2::RuntimeDistribution::DeltaDistribution( params[0] );
	} else if( 0 == strcmp( distr, "gaussian" ) ) {
		// Gaussian
		return sgns2::BasicRuntimeDistribution::NonNegGaussianDistribution( params[0], params[1] );
	} else if( 0 == strcmp( distr, "gaussian.trunc" ) ) {
		// Truncated Gaussian
		return sgns2::BasicRuntimeDistribution::TruncGaussianDistribution( params[0], params[1] );
	} else if( 0 == strcmp( distr, "exp" ) ) {
		// Exponential
		return sgns2::BasicRuntimeDistribution::ExponentialDistribution( params[0] );
	} else if( 0 == strcmp( distr, "gamma" ) ) {
		// Gamma
		return sgns2::BasicRuntimeDistribution::GammaDistribution( params[0], params[1] );
	} else if( 0 == strcmp( distr, "uniform" ) ) {
		// Uniform
		return sgns2::BasicRuntimeDistribution::UniformDistribution( params[0], params[1] );
	}
	return sgns2::RuntimeDistribution::DeltaDistribution( 0.0 );
}

bool Parser::readLabeledLuaReals( std::stringstream &in, char *label, uint labelMaxLen, const char *context, bool forceLabel ) {
	bool ret = false;

	PE_BEGINFRAME
		
	ParseStream block( in, context );
	block.strip();

	std::stringstream luaSS;
	luaSS << "return ";

	bool readLabel = false;

	PE_TRY {
		if( (readLabel = block.readLua( luaSS, ':' )) || forceLabel ) {
			if( readLabel )
				block.get();
			std::string labelStr = luaSS.str().substr(7);
			uint strip = 0;
			while( labelStr.length() > strip && charIsWhitespace( label[labelStr.length() - 1 - strip] ) )
				strip++;
			if( labelStr.length() == strip )
				error( static_cast<int>(in.str().length()), "Label for %s is empty", context );
			if( labelStr.length() - strip >= labelMaxLen )
				error( static_cast<int>(in.str().length()), "Label for %s is too long", context );
			
			strcpy( label, labelStr.c_str() );

			if( !readLabel )
				return true;

			luaSS.str("");
			luaSS.clear();
			luaSS << "return ";
			block.strip();
			block.readLua( luaSS );
			ret = true;
		}

		if( luaSS.str().length() == 7 ) {
			return ret;
		} else {
			if( luaL_loadstring( L, luaSS.str().c_str() ) == 0 ) {
				if( lua_pcall( L, 0, LUA_MULTRET, 0 ) != 0 ) 
					error( "Error in %s: %s", context, lua_tostring( L, -1 ) );
			} else {
				error( "Error in %s: %s", context, lua_tostring( L, -1 ) );
			}
		}
	}

	PE_ENDFRAME

	return ret;
}

void Parser::readIdQueue() {
	// queue [int]s-id\(luacode\)
	
	// Read the lua block
	int ch = in->sget();
	int r;
	int n;
	if( ch < 0 )
		error( "Expected wait list entry" );
	if( ch == '[' ) {
		double d;
		r = readLuaReals( &d, 1, "wait list molecule count", ']' );
		if( r <= 0 )
			error( "Expected wait list molecule count" );
		in->get();
		n = (int)floor(d);
	} else {
		in->putback( (char)ch );
		// Read the [int]
		int r = readInt( n );
		if (r < 0)
			n = 1;
	}

	// Read the s-id
	char sid[MAX_ELEMENT_NAME_LEN];
	r = readSID( sid, MAX_ELEMENT_NAME_LEN );
	if (r < 0) {
		if( r == PARSE_TOO_LONG )
			error( MAX_ELEMENT_NAME_LEN, "Wait list element name is too long" );
		error( "Expected wait list element name" );
	}

	// @Compartment
	readCompartmentAddress();

	// Read the release time
	double t;
	if( (ch = in->sget()) != '(' )
		error( "Expected \'(\'" );
	
	// Read the real as a lua block
	r = readLuaReals( &t, 1, "molecule release time", ')' );
	if (r <= 0)
		error( "Expected molecule release time");
	in->get(); // Clear the ')'

	// Get the element
	target->selectChemical( sid, moleculeReadout );

	// Add it to the waiting list
	target->addWaitListRelease( n, t );
}

void Parser::readIdMoleculeReadout() {
	// Valid values are "show" and "hide"
	int ch = in->sget();
	bool good = false;
	if (ch == 's') {
		if (in->get() == 'h' && in->get() == 'o' && in->get() == 'w') {
			moleculeReadout = true;
			good = true;
		}
	} else if (ch == 'h') {
		if (in->get() == 'i' && in->get() == 'd' && in->get() == 'e') {
			moleculeReadout = false;
			good = true;
		}
	}
	if (!good)
		error( "Expected \'show\' or \'hide\'" );
}

void Parser::readIdReadoutInterval() {
	// readout_interval [real]
	// Ommited readout inverval implies no readout
	// Interval of 0 implies readout every gillespie step

	int r;
	double interval;
	if( (r = readLuaReals( &interval, 1, "readout interval" )) <= 0) {
		target->setParameterD( sgns2::parse::ParseListener::READOUT_INTERVAL, -1.0 );
	} else if( interval <= 0.0 ) {
		target->setParameterD( sgns2::parse::ParseListener::READOUT_INTERVAL, 0.0 );
	} else {
		target->setParameterD( sgns2::parse::ParseListener::READOUT_INTERVAL, interval );
	}
}

void Parser::readIdOutputFile() {
	// output_file string
	
	PE_BEGINFRAME

	std::stringstream ss;

	PE_TRY {
		int ch;
		while( (ch = in->get()) >= 0 )
			ss.put( (char)ch );

		if( ss.str().length() == 0 )
			error( "Expected filename" );

		target->setParameterS( sgns2::parse::ParseListener::READOUT_FILE_TEMPLATE, ss.str().c_str() );
	}

	PE_ENDFRAME
}

void Parser::readIdOutputFileHeader() {
	// output_file_header [string]

	PE_BEGINFRAME

	std::stringstream ss;
	int ch;
	while( (ch = in->get()) >= 0 )
		ss.put( (char)ch );

	PE_TRY {
		target->setParameterS( sgns2::parse::ParseListener::READOUT_FILE_HEADER, ss.str().c_str() );
	}

	PE_ENDFRAME
}

// ---------------------------------------------------------------------------

void Parser::readIdBatchCount(){
	// batch_count <real>
	int r;
	double count;
	if ( (r = readLuaReals( &count, 1, "batch count" )) <= 0) {
		error( "Expected batch count" );
	} else if ( count < 0.0 ) {
		// negative batch count
		error( "Batch count cannot be negative" );
	}
	target->setParameterD( sgns2::parse::ParseListener::BATCH_COUNT, count );

}

void Parser::readIdBatchThreads(){
	// batch_max_thread <real>
	int r;
	double count;
	if ( (r = readLuaReals( &count, 1, "batch count" )) <= 0) {
		error( "Expected thread count" );
	} else if ( count < 0.0 ) {
		error( "Thread count cannot be negative" );
	}
	target->setParameterD( sgns2::parse::ParseListener::BATCH_THREADS, count );
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


// ---------------------------------------------------------------------------
void Parser::readIdData( const char *id, ParseStream *in ) {
	ParseStream *oldIn = this->in;
	if( in )
		this->in = in;

	IDReaderMap::const_iterator it = idReaders.find( id );
	if( it != idReaders.end() ) {
		// Known identifier - read it in
		(this->*(it->second))();
	} else {
		PE_BEGINFRAME

		std::stringstream ss;
		int ch;
		while( (ch = this->in->get()) >= 0 )
			ss.put( (char)ch );

		PE_TRY {
			if( !target->parseExtra( id, ss.str().c_str() ) )
				warning( "Unrecognized identifier \'%s\'.", id );
		}

		PE_ENDFRAME
	}

	this->in = oldIn;
}

// ---------------------------------------------------------------------------
void Parser::readIdData( const char *id, char end, char end2 ) {
	if( in->strip() < 0 ) {
		error( "Unexpected EOF after identifier %s", id );
		return;
	}

	in->setEOFOn( end );
	if( end2 )
		in->setSecondEOF( end2 );

	readIdData( id );

	if( in->clearEOF() < 0 ) {
		error( "Unexpected EOF in data" );
	} else if( in->sget() != end ) {
		if( end == ';' ) {
			error( "Unexpected symbols. Missing a \';\'?" );
		} else {
			error( "Unexpected symbols" );
		}
	} else if( end2 && in->get() != end2 ) {
		error( "Unexpected symbols" );
	}
}

// ---------------------------------------------------------------------------
void Parser::readIdDataBlock( const char *id ) {
	int ch = in->sget();
	if( ch >= 0 ) {
		if( ch == '{' ) { // block
			while( true ) {
				if( (ch = in->sget()) >= 0 ) {
					if( ch == '}' )
						return; // done
					in->putback( (char)ch );
					readIdData( id, ';' );
				} else {
					error( "Unexpected EOF in block data" );
				}
			}
		} else if( ch == '!' ) {
			if( in->get() == '{' ) { // super block
				readIdData( id, '}', '!' );
			} else { // single line, starting with !
				in->putback( '!' );
				readIdData( id, ';' );
			}
		} else { // single line
			in->putback( (char)ch );
			readIdData( id, ';' );
		}
	} else {
		error( "Unexpected EOF" );
	}
}

// ---------------------------------------------------------------------------
bool Parser::readIdentifier() {
	if( in->strip() >= 0 ) {
		char identifier[MAX_IDENTIFIER_LEN];
		int len = readCID( identifier, MAX_IDENTIFIER_LEN );
		if( len >= 0 ) {
			readIdDataBlock( identifier );
			return true;
		} else if( len == -2 ) {
			int ch = in->get();
			if( ch == ';' ) {
				// Empty ; - ignore it
				return true;
			} else {
				error( "Expected identifier" );
			}
		} else {
			error( MAX_IDENTIFIER_LEN, "Identifier too long" );
		}
	}

	return false;
}

// ---------------------------------------------------------------------------
void Parser::readIdentifiers( ParseStream *in ) {
	ParseStream *oldIn = this->in;
	if( in )
		this->in = in;
	while( readIdentifier() ) { }
	in = oldIn;
}

// ---------------------------------------------------------------------------
void Parser::error( int backStep, const char *format, ... ) {
	char msg[256];
	va_list args;
	va_start(args, format);
	if( vsnprintf( msg, 256, format, args ) < 0 )
		strcpy( msg, "[INTERNAL] Failed to create the error message (double fail!)" );

	throwError( -backStep, msg );
}

// ---------------------------------------------------------------------------
void Parser::error( const char *format, ... ) {
	char msg[256];
	va_list args;
	va_start(args, format);
	if( vsnprintf( msg, 256, format, args ) < 0 )
		strcpy( msg, "[INTERNAL] Failed to create the error message (double fail!)" );

	throwError( 0, msg );
}

// ---------------------------------------------------------------------------
void Parser::raiseError( const char *format, ... ) {
	char msg[256];
	va_list args;
	va_start(args, format);
	if( vsnprintf( msg, 256, format, args ) < 0 )
		strcpy( msg, "[INTERNAL] Failed to create the error message (double fail!)" );

	throwError( 0, msg );
}

// ---------------------------------------------------------------------------
void Parser::throwError( int posDifference, const char *error ) {
	int pos = in->getLineChar() + posDifference; // Store the position since in->getCurLine() will modify in->getLineChar()
	if( pos < 0 )
		pos = 0;
	errorMsg = error;
	errorCtx = in->getSource();
	errorLineSoFar = in->getCurLine();
	new( &curError ) Error( errorMsg.c_str(), errorCtx.c_str(), errorLineSoFar.c_str(), in->getLineNo(), pos );
	//longjmp( parseOrigin, 1 );
	PE_THROW;
	//throw ParseException( in, error, std::max( 0, in->getLineChar() + backStep ) );
}

// ---------------------------------------------------------------------------
void Parser::warning( const char *format, ... ) {
	if( showWarnings ) {
		char msg[256];
		va_list args;
		va_start(args, format);
		if( vsnprintf( msg, 256, format, args ) < 0 )
			strcpy( msg, "[INTERNAL] Failed to create the warning message" );

		Error err( msg, errorCtx.c_str(), "", in->getLineNo(), in->getLineChar() );
		target->issueWarning( &err );
		//std::cerr << "Warning " << in->getSource() << "(" << in->getLineNo() << "): " << msg << std::endl;
	}
}

// ---------------------------------------------------------------------------
void Parser::stripWhitespace( char *s ) {
	size_t l = strlen( s );
	while( l > 0 && charIsWhitespace( s[l - 1] ) )
		l--;
	s[l] = '\0';
}

// ---------------------------------------------------------------------------
int Parser::readCID( char *identifier, int maxLen ) {
	int ch = in->get();
	maxLen--;
	if( ch < 0 ) {
		return PARSE_EOS;
	} else if( charIsAlphaC( (char)ch ) ) {
		int idlen = 1;
		identifier[0] = (char)ch;
		while( true ) {
			ch = in->get();
			if( ch < 0 ) {
				identifier[idlen] = '\0';
				return idlen;
			} else if( charIsAlnumC( (char)ch ) ) {
				if( idlen < maxLen ) {
					identifier[idlen++] = (char)ch;
				} else {
					identifier[idlen] = '\0';
					return PARSE_TOO_LONG;
				}
			} else {
				identifier[idlen] = '\0';
				in->putback( (char)ch );
				return idlen;
			}
		}
	} else {
		in->putback( (char)ch );
		return PARSE_INVALID;
	}
}

// ---------------------------------------------------------------------------
int Parser::readSID( char *sid, int maxLen ) {
	int len = readCID( sid, maxLen );
	if( len < 0 ) {
		return len;
	} else {
		while( true ) {
			int ch = in->get();
			if( ch < 0 ) {
				return len;
			} else if( ch == '.' ) {
				// Read another c-id
				sid[len] = '.';
				int len2 = readCID( sid + len + 1, maxLen - len - 1 );
				if( len2 < 0 ) {
					return len2;
				} else {
					len += len2 + 1;
				}
			} else {
				// Done
				in->putback( (char)ch );
				return len;
			}
		}
	}
}

// ---------------------------------------------------------------------------
int Parser::readInt( int &i ) {
	bool neg = false;
	i = 0;
	int ch = in->peek();
	if( ch < 0 ) {
		return PARSE_EOS;
	} else if( ch == '-' ) {
		neg = true;
		in->get(); // Remove the - from the stream
		if( (ch = in->peek()) < 0 ) {
			in->putback( '-' );
			return PARSE_INVALID;
		}
	}
	if( ch < '0' || ch > '9' ) {
		// Not a digit
		if( neg )
			in->putback( '-' );
		return PARSE_INVALID;
	}

	while( true ) {
		ch = in->get();
		if( ch >= '0' && ch <= '9' ) {
			i *= 10;
			i += ch - '0';
		} else {
			if( ch >= 0 )
				in->putback( (char)ch );
			if( neg )
				i = -i;
			return PARSE_OK;
		}
	}
}

// ---------------------------------------------------------------------------
int Parser::readUInt( int &i ) {
	int ch = in->get();
	if( ch < 0 )
		return PARSE_EOS;

	if( ch < '0' || ch > '9' ) {
		// Not a digit
		return PARSE_INVALID;
	}

	i = ch - '0';
	while( true ) {
		ch = in->get();
		if( ch >= '0' && ch <= '9' ) {
			i *= 10;
			i += ch - '0';
		} else {
			if( ch >= 0 )
				in->putback( (char)ch );
			return PARSE_OK;
		}
	}
}

// ---------------------------------------------------------------------------
int Parser::readLuaReals( double *d, int n, const char *blockname, char delimiter ) {
	std::stringstream ss;

	in->strip();
	ss << "return ";
	in->readLua( ss, delimiter );

	int startTop = lua_gettop(L);
	int nReals = 0;

	if( ss.str().length() == 7 ) {
		return PARSE_EOS;
	} else {
		if( luaL_loadstring( L, ss.str().c_str() ) == 0 ) {
			if( lua_pcall( L, 0, LUA_MULTRET, 0 ) == 0 ) {
				nReals = lua_gettop(L) - startTop;
				if( nReals > n ) {
					// Pop return values until n values are on the stack
					warning( "Too many lua expressions in %s.", blockname );
					lua_pop( L, nReals - n );
					nReals = n;
				}

				for( int i = nReals - 1; i >= 0; i-- ) {
					if( lua_isnumber( L, -1 ) ) {
						d[i] = lua_tonumber( L, -1 );
						lua_pop( L, 1 );
					} else {
						error( "Expected %s. Lua returned '%s'", blockname, lua_typename( L, lua_type( L, -1 ) ) );
					}
				}
			} else {
				error( "Error in %s: %s", blockname, lua_tostring( L, -1 ) );
			}
		} else {
			error( "Error in %s: %s", blockname, lua_tostring( L, -1 ) );
		}
	}

	return nReals;
}

// ---------------------------------------------------------------------------
int Parser::readLuaRealBatch( double *d, int n, char *blockname, char delimiter ) {
	std::stringstream ss;

	in->strip();
	ss << "return ";
	in->readLua( ss, delimiter );

	int startTop = lua_gettop(L);

	if( ss.str().length() == 7 ) {
		return PARSE_EOS;
	} else {
		std::string s = ss.str().c_str();
		if( luaL_loadstring( L, ss.str().c_str() ) == 0 ) {
			for( int i = 0; i < n; i++ ) {
				lua_pushvalue( L, -1 );
				if( lua_pcall( L, 0, LUA_MULTRET, 0 ) == 0 ) {
					if( lua_gettop(L) > startTop + 2 ) {
						warning( "Too many lua expressions in %s", blockname );
						lua_pop( L, lua_gettop(L) - (startTop + 2) );
					}

					if( lua_isnumber( L, -1 ) ) {
						d[i] = lua_tonumber( L, -1 );
						lua_pop( L, 1 );
					} else {
						error( "Expected %s. Lua returned '%s'", blockname, lua_typename( L, lua_type( L, -1 ) ) );
					}
				} else {
					error( "Error in %s: %s", blockname, lua_tostring( L, -1 ) );
				}
			}
			lua_pop( L, 1 ); // Pop the function
		} else {
			error( "Error in %s: %s", blockname, lua_tostring( L, -1 ) );
		}
	}

	return 1;
}

// ---------------------------------------------------------------------------
int Parser::lua_parse( lua_State *L ) {
	Parser *parser = static_cast<Parser*>(lua_touserdata( L, lua_upvalueindex(1) ));

	const char *what = luaL_checkstring( L, 1 );
	bool ret;
	if( lua_isstring( L, 2 ) ) {
		ret = parser->parse( "Lua parse", what, lua_tostring( L, 2 ) );
	} else {
		ret = parser->parse( "Lua parse", what );
	}
	lua_pushboolean( L, ret );
	if( ret )
		return 1;

	lua_pushstring( L, parser->getLastError()->getMessage() );
	return 2;
}



// ---------------------------------------------------------------------------
//   Setting parsers

#if 0
// save_interval [real]
// Ommited save interval implies no save
// Interval of 0 is not allowed
void ReadSaveInterval (ProgramParameters *pp, const char*, ParseStream &in) {
	int r;
	if ((r = ReadLuaReal (pp, in, pp->snapshotInterval, "save interval")) < 0) {
		if (r == -1) {
			// No readout
			pp->save = false;
		}
	} else if (pp->snapshotInterval < 0) {
		ParseError (pp, in, "Negative save interval.");
	} else if (pp->snapshotInterval < EPSILON) {
		ParseError (pp, in, "Save interval of 0 is not allowed.");
	} else {
		pp->save = true;
	}
}

// save_file string
void ReadSaveFile (ProgramParameters *pp, const char *setting, ParseStream &in) {
	int ch, len = 0, unblen = 0;
	bool lastpercent = false, donepd = false;
	while ((ch = in->get()) >= 0) {
		if (len == 127) {
			ParseError (pp, in, "Filename provided to %s is too long (max 127 characters).", setting);
			return;
		}
		pp->unbutcheredSnapshotFile[unblen++] = (char)ch;
		if (ch == '%') {
			if (lastpercent) {
				// Make %% into %d
				ch = 'd';
				lastpercent = false;
			} else {
				lastpercent = true;
			}
		} else if (lastpercent) {
			// Make % into %% otherwise so that when it is passed through sprintf, it becomes % again
			pp->snapshotFile[len++] = '%';
			lastpercent = false;
		}
		pp->snapshotFile[len++] = (char)ch;
	}
	if (len == 0) {
		ParseError (pp, in, "Expected filename after %s.", setting);
		return;
	}

	pp->snapshotFile[len] = '\0';
	pp->unbutcheredSnapshotFile[unblen] = '\0';
}

// save_index int
void ReadSaveIndex (ProgramParameters *pp, const char *setting, ParseStream &in) {
	if (ReadInt (in, pp->snapshotIndex) < 0) {
		ParseError (pp, in, "Expected int after %s.", setting);
	} else if (pp->snapshotIndex < 0) {
		ParseError (pp, in, "Negative save index.");
	}
}


// save_now string
void SaveProgramParameters (ProgramParameters *pp, ostream &out);
void ReadSaveNow (ProgramParameters *pp, const char *setting, ParseStream &in) {
	char filename[128];
	int ch, len = 0;
	while ((ch = in->get()) >= 0) {
		if (len == 127) {
			ParseError (pp, in, "Filename provided to %s is too long (max 127 characters).", setting);
			return;
		}
		filename[len++] = (char)ch;
	}
	if (len == 0) {
		ParseError (pp, in, "Expected filename after %s.", setting);
		return;
	}

	filename[len] = '\0';
	ofstream fout (filename);
	if (fout) {
		SaveProgramParameters (pp, fout);
	} else {
		ParseError (pp, in, "Failed to open %s for writing for %s.", filename, setting);
	}
}



void InitSettingParsers() {
	g_settingParsers["time"] = ReadTime;
	g_settingParsers["seed"] = ReadSeed;
	g_settingParsers["include"] = ReadInclude;
	g_settingParsers["population"] = ReadPopulation;
	g_settingParsers["reaction"] = ReadReaction;
	g_settingParsers["queue"] = ReadQueue;
	g_settingParsers["stop_time"] = ReadStopTime;
	g_settingParsers["readout_interval"] = ReadReadoutInterval;
	g_settingParsers["output_file"] = ReadOutputFile;
	g_settingParsers["save_interval"] = ReadSaveInterval;
	g_settingParsers["save_file"] = ReadSaveFile;
	g_settingParsers["save_index"] = ReadSaveIndex;
	g_settingParsers["output_file_header"] = ReadOutputFileHeader;
	g_settingParsers["warn"] = ReadWarn;
	g_settingParsers["save_now"] = ReadSaveNow;
	g_settingParsers["progress"] = ReadProgress;
	g_settingParsers["lua"] = ReadLua;
	g_settingParsers["performance"] = ReadPerformance;
	g_settingParsers["molecule_readout"] = ReadMoleculeReadout;

	//compartment
	//parameter
}
#endif

} // namespace parse
} // namespace sgns2
