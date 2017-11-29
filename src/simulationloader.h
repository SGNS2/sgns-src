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

/* simulationloader.h/cpp

SimulationLoader class contents:


*/

#ifndef SIMULATIONLOADER_H
#define SIMULATIONLOADER_H

#include <list>
#include <map>
#include <vector>

#include "parser.h"
#include "simulationinit.h"
#include "mempool.h"
#include "chemical.h"
#include "compartmenttype.h"

namespace sgns2 {

class SimulationLoader : public parse::ParseListener {
public:
	SimulationLoader();
	~SimulationLoader();

	// See parser.h for explanations of these functions

	virtual bool parseExtra( const char *id, const char *data );

	virtual void selectChemical( const char *name, bool defVisibility );
	virtual void setChemicalVisible( bool visible );
	virtual void setPopulation( const sgns2::RuntimeDistribution *distr, bool add );

	virtual void createCompartmentType( const char *typestr );
	virtual void selectCompartmentType( const char *name );
	virtual void selectCompartment( const char *name );
	virtual void outputCompartment( bool output );
	virtual void instantiateCompartment( const char *name );
	virtual void instantiateCompartment( int n );

	virtual void newReaction( const char *name );
	virtual void finishReaction( double c );
	virtual void overrideH( const char *func, double *params, unsigned nParams );
	virtual void newReactant( int n );
	virtual void newSplitReactant( const sgns2::SplitFunction *split );
	virtual void newSplitCompartment( const sgns2::SplitFunction *split );
	virtual void setRate( const sgns2::RateFunction *rf );
	virtual void newProduct( int n );
	virtual void newSplitProduct( unsigned src, bool splitCompartments );
	virtual void setTau( const sgns2::RuntimeDistribution *tau );
	virtual void produceCompartment();
	virtual void eatCompartment();

	virtual void addWaitListRelease( int n, double time );

	double getParameterD( Parameter param );
	const char *getParameterS( Parameter param );
	virtual void setParameterD( Parameter param, double val );
	virtual void setParameterS( Parameter param, const char *val );

	virtual void saveAt( double time, const char *filename );

	virtual void issueWarning( const parse::Error *warning );


	// Access to the parser
	parse::Parser *getParser() { return parser; }
	// Include a file directly
	bool include( const char *file );

	// Finalize the simulation
	void loadingComplete();
	// Indicate that a batch run is starting - runtime components should be duplicated
	void beginBatchRun();
	// Create a new SimulationInstance
	void beginSimulation( SimulationInstance *&sim, HierCompartment *&env, unsigned seedOffset = 0 );

	// Model stats
	inline unsigned getReactionCount() const { return reactionCount; }
	inline unsigned getChemicalCount() const { return chemicalCount; }

	// Output
	enum Show {
		// Verbosity to stdout
		SHOW_PROGRESS,
		SHOW_PERFORMANCE,
		// Verbosity to output file
		SHOW_TIME,
		SHOW_STEP_COUNT,
		SHOW_WL_SIZE,
		SHOW_STEP_DESC,

		SHOW_COUNT
	};
	// Access to visilility settings
	inline bool shouldShow( Show what ) const { return show[what]; }
	enum OutputTarget {
		OUTPUTTGT_STDOUT,
		OUTPUTTGT_FILE
	};
	// Access to the desired output target
	inline OutputTarget getOutputTarget() const { return outputTarget; }
	enum OutputFormat {
		OUTPUT_CSV,
		OUTPUT_TSV,
		OUTPUT_BIN32,
		OUTPUT_BIN64,
		OUTPUT_NULL
	};
	// Access to the desired output format
	inline OutputFormat getOutputFormat() const { return outputFormat; }

	enum {
		MAX_COMPARTMENT_TYPE_DEPTH = 16
	};

protected:
	// Reset the temporary reaction storage
	void resetReaction();
	// If reactsIn has not yet been set up, sets it up
	void consolidateReactsIn();
	// Ensure that a compartment type is in the compartment type
	// stack for the current reaction
	bool ensureTypeInStack( CompartmentType *type );

	// The parser
	parse::Parser *parser;

	// The list of initcommands to run when creating a new simulation
	typedef std::vector< init::Command* > InitCmdList;
	InitCmdList initCommands;
	unsigned cmdsSinceAction; // Used to remove superfluous initcommands
	bool emptyAddress, specificAddress;
	init::SelectEnv reloadEnvCmd;
	init::UpdateSimulation updateSimCmd;

	// Type Name -> CompartmentType
	struct CompartmentTypeAndInit {
		CompartmentTypeAndInit( CompartmentType *type )
			: selectMe(type), type(type) { }
		init::SelectCompartmentType selectMe;
		CompartmentType *type;
	};
	typedef std::map< const char*, CompartmentTypeAndInit, StringCmpLt_Functor > CompTypeMap;
	CompTypeMap compTypes;

	// Chemical Name -> Chemical
	typedef std::map< const char*, Chemical*, StringCmpLt_Functor > ChemicalMap;
	ChemicalMap chemicals;

	// Compartment Name -> named index
	struct CompartmentNameAndInit {
		CompartmentNameAndInit( sgns2::uint idx, CompartmentType *type )
			: index(idx), selectMe(idx), type(type) { }
		CompartmentNameAndInit( const CompartmentNameAndInit &other )
			: index(other.index), selectMe(other.index), type(other.type) { }
		unsigned index;
		init::SelectCompartment selectMe;
		CompartmentType *type;
	};
	typedef std::map< std::string, CompartmentNameAndInit > NamedCompMap;
	NamedCompMap namedCompartments;
	
	// Env
	CompartmentType *envType;

	// The currently selected compartment type
	// E.g. new compartments are created using this type
	CompartmentType *currentType;
	// The currently selected chemical
	// E.g. new reactants and products are created targetting this chemical
	Chemical *currentChemical;
	// The type of the currently selected compartment set
	// E.g. new compartments will be created inside this compartment type
	CompartmentType *selectedType;


	// Intermediate reaction construction
	struct TempChemical {
		TempChemical *next, *prev;
		Chemical *chemical;
		uint chemicalIdx;
		uint n;
		uint compartment;
		CompartmentType *createType;
		RateFunction fn;
		RuntimeDistribution tau;
		bool isSplit;
		SplitFunction split;
	};
	MemoryPool< TempChemical > chemicalPool;

	// An Extra action which simply runs a list of initcommands
	class InitCmdExtra : public reaction::Template::Extra {
	public:
		InitCmdExtra();
		virtual ~InitCmdExtra();

		inline void addCommand( init::Command *cmd ) { commands.push_back( cmd ); }
		virtual void execute( const reaction::Template *tmplate, Compartment **context );

	private:
		InitCmdList commands;
	};
	// Does the reaction have a name?
	bool rxnHasName;
	// Does this reaction produce a compartment?
	bool rxnProducesCompartment;
	// Does this reaction destroy a compartment?
	// If non-zero, this is the depth of the destroyed compartment
	sgns2::uint rxnDestroysCompartment;
	// Name of the current reactions
	std::string rxnName;
	// Linked list of reactants
	TempChemical *reactantHead, *reactantTail;
	// Linked list of products
	TempChemical *productHead, *productTail;
	// The Extra command to attach to this reaction
	InitCmdExtra *extraCommands;
	// The current compartment type stack
	std::vector< CompartmentType* > curTypeStack;
	// The compartment type hierarchy which this reaction reacts in
	std::vector< CompartmentType* > reactsIn;
	// The reaction's use of each type in the hierarchy
	// Bit 1: reactant; Bit 2: product; Bit 3: compartment destruction
	std::vector< uint8_t > typeUsed;
	// The current reaction's H-function
	reaction::Template::HEvaluator rxnHEval;
	// The Split reactant-product linking
	std::vector< TempChemical* > splits;
	// Number of compartment splits in the current reaction
	uint rxnCompSplitCount;

	// Simulation Parameters
	unsigned seed;
	double startTime;
	double stopTime;
	double readoutInterval;
	double saveInterval;
	double saveIndex;
	double batchCount;
	double batchThreads;

	// Output Parameters
	std::string readoutFile;
	std::string fileHeader;
	std::string saveFileTemplate;
	bool show[SHOW_COUNT];
	OutputFormat outputFormat;
	OutputTarget outputTarget;

	// Model stats
	uint chemicalCount;
	uint reactionCount;
	uint maxSplitCount;

	// Lua state backup for batch runs
	void *L_packed;
	uint L_packedsize;
	uint L_packedcapacity;
	static const char *L_loader( lua_State *L, void *ud, size_t *sz );
	static int L_saver( lua_State *L, const void *p, size_t sz, void *ud );

	// Special H-functions
	static double SGNS_FASTCALL hEval_fa2a1r( Compartment **context, reaction::Reactant *firstReactant );
	static double SGNS_FASTCALL hEval_sshdimer( Compartment **context, reaction::Reactant *firstReactant );
	static double SGNS_FASTCALL hEval_lua( Compartment **context, reaction::Reactant *firstReactant );
};

} // namespace sgns2

#endif // SIMULATIONLOADER_H
