
// See simulationloader.h for a description of the contents of this file.

#include "stdafx.h"

#include <algorithm>
#include <cassert>
#include <new>
#include <stdint.h>

#include "lua.hpp"
extern "C" {
#include "pluto.h"
}

#include "simulationloader.h"
#include "sbmlreader.h"

using namespace sgns2;

// ---------------------------------------------------------------------------
SimulationLoader::SimulationLoader()
: parser(NULL)
, cmdsSinceAction(0)
, emptyAddress(true), specificAddress(true)
, reactantHead(NULL), reactantTail(NULL)
, productHead(NULL), productTail(NULL)
, extraCommands(NULL)
, rxnHEval(NULL)
, rxnCompSplitCount(0)
, startTime(0.0), stopTime(0.0)
, readoutInterval(1.0)
, saveInterval(0.0)
, saveIndex(0.0)
, batchCount(1.0), batchThreads(0.0)
, readoutFile("output.?")
, fileHeader("")
, saveFileTemplate("simulation_save%%.g")
, outputFormat(OUTPUT_CSV)
, outputTarget(OUTPUTTGT_FILE)
, chemicalCount(0), reactionCount(0)
, maxSplitCount(0)
, L_packed(NULL)
{
	show[SHOW_PROGRESS] = false;
	show[SHOW_PERFORMANCE] = false;
	show[SHOW_TIME] = true;
	show[SHOW_STEP_COUNT] = false;
	show[SHOW_WL_SIZE] = false;
	show[SHOW_STEP_DESC] = false;

	// Create the parser
	parser = new parse::Parser( this );
	parser->parse( "initialization", "seed", "" );

	// Create Env
	envType = new CompartmentType( "Env" );
	selectedType = envType;
	currentType = envType;
	compTypes.insert( CompTypeMap::value_type( envType->getName().c_str(), envType ) );
	// Initialize intermediate reaction stuff
	resetReaction();
}

// ---------------------------------------------------------------------------
SimulationLoader::~SimulationLoader() {
	free( L_packed );
	delete parser;

	for( CompTypeMap::iterator it = compTypes.begin(); it != compTypes.end(); ++it )
		delete it->second.type; // destroys all reactions too
	for( ChemicalMap::iterator it = chemicals.begin(); it != chemicals.end(); ++it )
		delete it->second;

	//for( InitCmdList::iterator it = initCommands.begin(); it != initCommands.end(); ++it )
	//	delete *it;
	// TODO: Free memory from Extra commands
}

// ---------------------------------------------------------------------------
bool SimulationLoader::parseExtra( const char *id, const char *data ) {
	switch( id[0] ) {
	case 'i':
		if( 0 == strcmp( id, "import" ) ) {
			char format[32];
			const char *fn = strchr( data, ':' );
			bool explicitFormat = !!fn;
			if( explicitFormat ) {
				if( fn - data >= 32 )
					parser->raiseError( "Unknown file format: %s", data );
				strncpy( format, data, std::min( sizeof(format)-1, (size_t)(fn - data) ) );
				format[fn - data] = '\0';
			} else {
				// Get the format from the filename
				fn = data;
				const char *fmt = strrchr( data, '.' );
				if( !fmt ) {
					fmt = "sgns";
				} else {
					fmt++;
				}
				strncpy( format, fmt, sizeof(format)-1 );
			}

			if( 0 == strcmp( format, "sgns" ) ) {
				parser->parse( "import", "include", fn );
			} else if( 0 == strcmp( format, "sbml" ) || 0 == strcmp( format, "xml" ) ) {
#ifdef ENABLE_SBML
				if( 0 == strcmp( data, "-" ) ) {
					importSBMLFromStream( std::cin, parser, this );
				} else {
					importSBMLFromFile( data, parser, this );
				}
#else
				parser->raiseError( "SGNS2 was not compiled with ENABLE_SBML. SBML support is disabled." );
#endif
			} else if( 0 == strcmp( format, "cmdl" ) || 0 == strcmp( format, "dizzy" ) ) {
				// TODO: Add CMDL
			} else if( explicitFormat ) {
				parser->raiseError( "Unknown file format: %s", format );
			} else {
				// Assume SGNS, treat as --include
				parser->parse( "import", "include", fn );
			}
			return true;
		}
		break;
	case 'o':
		if( 0 == strcmp( id, "output_format" ) ) {
			if( 0 == strcmp( data, "csv" ) ) {
				outputFormat = OUTPUT_CSV;
			} else if( 0 == strcmp( data, "tsv" ) ) {
				outputFormat = OUTPUT_TSV;
			} else if( 0 == strcmp( data, "bin32" ) ) {
				outputFormat = OUTPUT_BIN32;
			} else if( 0 == strcmp( data, "bin64" ) ) {
				outputFormat = OUTPUT_BIN64;
			} else if( 0 == strcmp( data, "null" ) || 0 == strcmp( data, "none" ) ) {
				outputFormat = OUTPUT_NULL;
			} else {
				parser->warning( "Unknown output format '%s', defaulting to 'csv'", data );
				outputFormat = OUTPUT_CSV;
			}
			return true;
		} else if( 0 == strcmp( id, "output" ) ) {
			const char *showhide = data;
			bool showme = false;
			if( 0 == strncmp( showhide, "show", 4 ) ) {
				showme = true;
			} else if( 0 != strncmp( showhide, "hide", 4 ) ) {
				parser->raiseError( "Expected 'show' or 'hide'" );
			}

			const char *what = strchr( data, ' ' );
			while( *(++what) == ' ' ) { }

			if( 0 == strcmp( what, "time" ) ) {
				show[SHOW_TIME] = showme;
			} else if( 0 == strcmp( what, "step_count" ) ) {
				show[SHOW_STEP_COUNT] = showme;
			} else if( 0 == strcmp( what, "waitlist_size" ) ) {
				show[SHOW_WL_SIZE] = showme;
			} else if( 0 == strcmp( what, "step_description" ) ) {
				show[SHOW_STEP_DESC] = showme;
			} else {
				parser->raiseError( "%s what?", showme ? "Show" : "Hide" );
			}

			// output {
			//   show time;
			//   hide step_count;
			//   show waitlist_size;
			//   show step_description;
			// }
			return true;
		}
		break;
	case 'p':
		if( 0 == strcmp( id, "progress" ) ) {
			if( 0 == strcmp( data, "off" ) ) {
				show[SHOW_PROGRESS] = false;
			} else if( 0 == strcmp( data, "on" ) ) {
				show[SHOW_PROGRESS] = true;
			} else {
				parser->raiseError( "Expected: 'on' or 'off'" );
			}
			return true;
		} else if( 0 == strcmp( id, "performance" ) ) {
			if( 0 == strcmp( data, "off" ) ) {
				show[SHOW_PERFORMANCE] = false;
			} else if( 0 == strcmp( data, "on" ) ) {
				show[SHOW_PERFORMANCE] = true;
			} else {
				parser->raiseError( "Expected: 'on' or 'off'" );
			}
			return true;
		}
		break;
	}

	return false;
}

// ---------------------------------------------------------------------------
void SimulationLoader::selectChemical( const char *name, bool defVisibility ) {
	ChemicalMap::const_iterator it = chemicals.find( name );
	if( it != chemicals.end() ) {
		currentChemical = it->second;
	} else {
		Chemical *chem = new Chemical( name );
		currentChemical = chem;
		chemicals[chem->getName().c_str()] = chem;
		chem->setOutput( defVisibility );
		chemicalCount++;
	}
}

// ---------------------------------------------------------------------------
void SimulationLoader::setChemicalVisible( bool visible ) {
	currentChemical->setOutput( visible );
}

// ---------------------------------------------------------------------------
void SimulationLoader::setPopulation( const sgns2::RuntimeDistribution *distr, bool add ) {
	initCommands.push_back( new init::SetPopulations( selectedType->getChemicalIndex( currentChemical, true ), distr, add ) );
	cmdsSinceAction = 0;
}

// ---------------------------------------------------------------------------
void SimulationLoader::createCompartmentType( const char *typestr ) {
	CompTypeMap::const_iterator it = compTypes.find( typestr );
	if( it != compTypes.end() )
		parser->raiseError( "Compartment type %s already exists", typestr );
	NamedCompMap::const_iterator nit = namedCompartments.find( typestr );
	if( nit != namedCompartments.end() )
		parser->raiseError( "Compartment %s already exists", typestr );
	if( currentType->getDepth() + 1 == MAX_COMPARTMENT_TYPE_DEPTH )
		parser->raiseError( "The compartment hierarchy is currently limited to %d types", MAX_COMPARTMENT_TYPE_DEPTH );

	CompartmentType *type = new CompartmentType( typestr, currentType );
	compTypes.insert( CompTypeMap::value_type( type->getName().c_str(), type ) );
}

// ---------------------------------------------------------------------------
void SimulationLoader::selectCompartmentType( const char *name ) {
	if( name == NULL ) {
		currentType = envType;
	} else {
		CompTypeMap::const_iterator it = compTypes.find( name );
		if( it == compTypes.end() )
			parser->raiseError( "Undeclared compartment type: %s", name );
		currentType = it->second.type;
	}
}

// ---------------------------------------------------------------------------
void SimulationLoader::selectCompartment( const char *name ) {
	if( name == NULL ) {
		// Clear trailing non-useful init commands
		while( cmdsSinceAction ) {
			initCommands.pop_back();
			cmdsSinceAction--;
		}

		// Select EnvType
		selectedType = envType;
		initCommands.push_back( &reloadEnvCmd );
		cmdsSinceAction++;
		emptyAddress = true;
		specificAddress = true;
	} else {
		// Select the compartment type or instance
		CompTypeMap::iterator it = compTypes.find( name );
		if( it == compTypes.end() ){
			NamedCompMap::iterator namedIt = namedCompartments.find( name );
			if( namedIt == namedCompartments.end() )
				parser->raiseError( "Undeclared compartment type or name: %s", name );
			if( !emptyAddress )
				parser->raiseError( "Compartment address is invalid" );
			
			while( cmdsSinceAction ) {
				initCommands.pop_back();
				cmdsSinceAction--;
			}

			selectedType = namedIt->second.type;
			initCommands.push_back( &namedIt->second.selectMe );
			cmdsSinceAction++;
			emptyAddress = false;
		} else {
			CompartmentType *newType = it->second.type;

			if( !newType->isSubtypeOf( selectedType ) )
				parser->raiseError( "Compartment address is invalid" );

			if( newType->getDepth() != 0 ) {
				// Move selectedType down the hierarchy to newType
				CompartmentType *hierarchy[MAX_COMPARTMENT_TYPE_DEPTH];
				sgns2::uint bottom = newType->getDepth();
				while( newType != selectedType ) {
					hierarchy[newType->getDepth()] = newType;
					newType = newType->getParentType();
				}
			
				for( sgns2::uint i = selectedType->getDepth() + 1; i <= bottom; i++ ) {
					initCommands.push_back( &compTypes.find( hierarchy[i]->getName().c_str() )->second.selectMe ); // TODO: Optimize?
					cmdsSinceAction++;
				}

				selectedType = it->second.type;
				specificAddress = false;
				emptyAddress = false;
			}
		}
	}
}

// ---------------------------------------------------------------------------
void SimulationLoader::outputCompartment( bool output ) {
	if( !emptyAddress && specificAddress )
		parser->raiseError( "Cannot change the output of a specific compartment" );
	selectedType->setOutput( output );
}

// ---------------------------------------------------------------------------
void SimulationLoader::instantiateCompartment( const char *name ) {
	// Named instantiation
	if( currentType->getDepth() == 0 )
		parser->raiseError( "Cannot create Env" );
	CompTypeMap::const_iterator it = compTypes.find( name );
	if( it != compTypes.end() )
		parser->raiseError( "Compartment type %s already exists", name );
	NamedCompMap::const_iterator nit = namedCompartments.find( name );
	if( nit != namedCompartments.end() )
		parser->raiseError( "Compartment %s already exists", name );
	if( !specificAddress )
		parser->raiseError( "Compartment address is invalid for a named compartment" );

	uint idx = static_cast<uint>(namedCompartments.size());
	namedCompartments.insert( NamedCompMap::value_type( name, CompartmentNameAndInit( idx, currentType ) ) );
	initCommands.push_back( new init::InstantiateNamedCompartment( idx, currentType ) );
	cmdsSinceAction = 0;
}

// ---------------------------------------------------------------------------
void SimulationLoader::instantiateCompartment( int n ) {
	// Anonymous instantiation
	if( currentType->getDepth() == 0 )
		parser->raiseError( "Cannot create Env" );
	if( currentType->getParentType() != selectedType )
		selectCompartment( currentType->getParentType()->getName().c_str() );
	initCommands.push_back( new init::InstantiateCompartments( currentType, n ) );
	cmdsSinceAction = 0;
}

// ---------------------------------------------------------------------------
void SimulationLoader::newReaction( const char *name ) {
	resetReaction();
	rxnHasName = name != NULL;
	if( name )
		rxnName = name;
}

// ---------------------------------------------------------------------------
void SimulationLoader::finishReaction( double c ) {
	consolidateReactsIn();

	bool reactantsInExtra = !splits.empty();
	bool productsInExtra = rxnProducesCompartment || !splits.empty();

	reaction::Template *bottomTemplate = NULL;
	uint umbrellaIndex = (uint)-1, umbrellaBank = 0;
	uint8_t typeUsedMask = 3;
	uint maxDepth = static_cast<uint>(typeUsed.size());
	if( productsInExtra ) {
		// Reactions producing compartments should not create umbrellas to
		// produce chemicals since it's handled by the Extra object
		typeUsedMask = 1;
		maxDepth = static_cast<uint>(reactsIn.size());
	}
	if( rxnDestroysCompartment && rxnDestroysCompartment == maxDepth - 1 )
		// Only force destroy compartment reaction to have an empty ReactionInstance
		// when it's destroying a compartment below all reactants
		typeUsedMask |= 4;

	for( uint i = 0; i < maxDepth; i++ ) {
		if( typeUsed[i] & typeUsedMask || i == maxDepth-1 ) {
			uint rxnIndex;
			rxnIndex = reactsIn[i]->getBank()->createReaction( umbrellaBank, umbrellaIndex, i+1 < typeUsed.size(), rxnDestroysCompartment && i >= rxnDestroysCompartment );
			reaction::Template *tmplate = reactsIn[i]->getBank()->getReactionTemplate( rxnIndex );
			tmplate->setC( c );
			if( rxnHEval && reactantHead->compartment == i )
				tmplate->setHEvaluator( rxnHEval );
			
			TempChemical **prevR = &reactantHead;
			if( reactantsInExtra ) {
				// Populations will be changed in Extra, so only put the rate function here
				for( TempChemical *r = reactantHead; r; ) {
					if( r->compartment == i && !r->fn.isUnit() ) {
						reaction::Reactant *rct = tmplate->newReactant( r->chemicalIdx, 0, 0 );
						*rct->getRateFunction() = r->fn;
					}
					prevR = &r->next;
					r = r->next;
				}
			} else {
				// Normal reaction
				for( TempChemical *r = reactantHead; r; ) {
					if( r->compartment == i ) {
						if( !r->fn.isUnit() || r->n ) {
							reaction::Reactant *rct = tmplate->newReactant( r->chemicalIdx, r->n, 0 );
							*rct->getRateFunction() = r->fn;
						}

						TempChemical *next = r->next;
						*prevR = r->next;
						chemicalPool.free( r );
						r = next;
					} else {
						prevR = &r->next;
						r = r->next;
					}
				}
			}
			if( !productsInExtra ) {
				prevR = &productHead;
				for( TempChemical *r = productHead; r; ) {
					if( r->compartment == i ) {
						reaction::Product *prod = tmplate->newProduct( r->chemicalIdx, r->n, 0 );
						*prod->getTau() = r->tau;

						TempChemical *next = r->next;
						*prevR = r->next;
						chemicalPool.free( r );
						r = next;
					} else {
						prevR = &r->next;
						r = r->next;
					}
				}
			}

			tmplate->flipChemicalOrders();
			bottomTemplate = tmplate;
			umbrellaIndex = rxnIndex;
			umbrellaBank = i;
			c = 1.0;
		}
	}

	if( reactantsInExtra ) {
		// Use init commands for the stoichiometry of the reactant list for reactions that use splits
		if( !extraCommands )
			extraCommands = new InitCmdExtra;

		while( reactantHead ) {
			TempChemical *r = reactantHead;
			reactantHead = reactantHead->next;

			extraCommands->addCommand( new init::SelectCompartment( r->compartment ) );
			if( r->isSplit ) {
				if( r->chemical ) {
					extraCommands->addCommand( new init::SplitPopulation( r->chemicalIdx, r->n, &r->split ) );
				} else {
					extraCommands->addCommand( new init::SelectCompartmentType( r->createType ) );
					extraCommands->addCommand( new init::SplitCompartments( r->n, r->chemicalIdx == (uint)-1 ? (uint)-1 : static_cast<uint>(splits.size()) + r->chemicalIdx, &r->split ) );
				}
			} else if( r->n != 0 ) {
				RuntimeDistribution n = RuntimeDistribution::DeltaDistribution( -(double)r->n );
				extraCommands->addCommand( new init::SetPopulations( r->chemicalIdx, &n, true ) );
			}

			chemicalPool.free( r );
		}
	}

	if( rxnDestroysCompartment ) {
		// Annihiliate compartments! Muahaha
	 	if( !extraCommands )
			extraCommands = new InitCmdExtra;

		extraCommands->addCommand( new init::SelectCompartment( rxnDestroysCompartment ) );
		extraCommands->addCommand( new init::DeleteCompartments );
	}

	if( productsInExtra ) {
		// Use init commands for the product list for reactions that create compartments or use splits
		if( !extraCommands )
			extraCommands = new InitCmdExtra;

		while( productHead ) {
			TempChemical *r = productHead;
			productHead = productHead->next;

			extraCommands->addCommand( new init::SelectCompartment( r->compartment ) );
			if( r->isSplit ) {
				// Release a split
				if( r->chemical ) {
					// Population
					if( r->tau.isZero() ) {
						extraCommands->addCommand( new init::AddPopulationFromSplitBuffer( r->chemicalIdx, r->n ) );
					} else {
						extraCommands->addCommand( new init::AddToWaitListFromSplitBuffer( r->chemicalIdx, r->n, &r->tau ) );
					}
				} else {
					// Compartment
					extraCommands->addCommand( new init::InsertSplitCompartments( static_cast<uint>(splits.size()) + r->n ) );
				}
			} else if( r->chemical ) {
				// Basic release
				RuntimeDistribution n = RuntimeDistribution::DeltaDistribution( (double)r->n );
				if( r->tau.isZero() ) {
					extraCommands->addCommand( new init::SetPopulations( r->chemicalIdx, &n, true ) );
				} else {
					extraCommands->addCommand( new init::AddToWaitList( r->chemicalIdx, &n, &r->tau, true ) );
				}
			} else {
				// Create a compartment
				extraCommands->addCommand( new init::InstantiateNamedCompartment( r->compartment+1, r->createType ) );
			}

			chemicalPool.free( r );
		}
	}

	if( extraCommands )
		bottomTemplate->addExtra( extraCommands );

	reactionCount++;

	uint rxnSplitCount = rxnCompSplitCount + static_cast<uint>(splits.size());
	if( rxnSplitCount > maxSplitCount )
		maxSplitCount = rxnSplitCount;

	assert( reactantHead == NULL );
	assert( productHead == NULL );
}

// ---------------------------------------------------------------------------
void SimulationLoader::overrideH( const char *func, double *params, unsigned nParams ) {
	if( 0 == strcmp( func, "lua" ) ) {
		if( nParams != 1 )
			parser->raiseError( "Expected one parameter for h-function lua" );
		reactantHead->fn.p0.i = (int)params[0];
		rxnHEval = &hEval_lua;
	} else if( 0 == strcmp( func, "fa2a1r" ) ) {
		if( nParams != 8 )
			parser->raiseError( "Expected 8 parameters for h-function fa2a1r" );

		TempChemical *r = reactantHead;
		if( !r ) parser->raiseError( "H-function 'fa2a1r' requires 3 reactants in the same compartment" );
		r->fn.p0.d = params[0]; // k0
		r->fn.p1.d = params[7]; // k123
		if( !r->next || r->next->compartment != r->compartment )
			parser->raiseError( "H-function 'fa2a1r' requires 3 reactants in the same compartment" );
		r = r->next;
		r->fn.p0.d = params[1]; // k1
		r->fn.p1.d = params[2]; // k2
		r->fn.p2.d = params[3]; // k3
		if( !r->next || r->next->compartment != r->compartment )
			parser->raiseError( "H-function 'fa2a1r' requires 3 reactants in the same compartment" );
		r = r->next;
		r->fn.p0.d = params[4]; // k12
		r->fn.p1.d = params[5]; // k23
		r->fn.p2.d = params[6]; // k13
		rxnHEval = &hEval_fa2a1r;
	} else if( 0 == strcmp( func, "sshdimer" ) ) {
		if( nParams != 1 )
			parser->raiseError( "Expected 1 parameter for h-function sshdimer" );
		if( !reactantHead || !reactantHead->next || reactantHead->compartment != reactantHead->next->compartment )
			parser->raiseError( "H-function 'sshdimer' requires at least 2 reactants in the same compartment" );

		reactantHead->fn.p2.d = params[0]; // k
		rxnHEval = &hEval_sshdimer;
	} else {
		parser->raiseError( "Unknown h-function: %s", func );
	}
}

// ---------------------------------------------------------------------------
void SimulationLoader::newReactant( int n ) {
	if( !emptyAddress && specificAddress )
		parser->raiseError( "Reactions that occur in a named compartment only are not currently implemented" );
	if( !ensureTypeInStack( selectedType ) )
		parser->raiseError( "Reactions cannot span laterally across the compartment hierarchy" );

	typeUsed[selectedType->getDepth()] |= 1;

	TempChemical *r = chemicalPool.alloc();
	r->next = NULL;
	r->prev = reactantTail;
	if( reactantTail ) {
		reactantTail->next = r;
	} else {
		reactantHead = r;
	}
	reactantTail = r;
	r->compartment = selectedType->getDepth();
	r->n = n;
	r->chemical = currentChemical;
	r->chemicalIdx = selectedType->getChemicalIndex( currentChemical, true );
	r->fn = RateFunction::Linear();
	r->isSplit = false;
}

// ---------------------------------------------------------------------------
void SimulationLoader::newSplitReactant( const sgns2::SplitFunction *split ) {
	if( !emptyAddress && specificAddress )
		parser->raiseError( "Reactions that occur in a named compartment only are not currently implemented" );
	if( !ensureTypeInStack( selectedType ) )
		parser->raiseError( "Reactions cannot span laterally across the compartment hierarchy" );

	typeUsed[selectedType->getDepth()] |= 1;

	TempChemical *r = chemicalPool.alloc();
	r->next = NULL;
	r->prev = reactantTail;
	if( reactantTail ) {
		reactantTail->next = r;
	} else {
		reactantHead = r;
	}
	reactantTail = r;
	r->compartment = selectedType->getDepth();
	r->n = static_cast<uint>(splits.size());
	r->chemical = currentChemical;
	r->chemicalIdx = selectedType->getChemicalIndex( currentChemical, true );
	r->fn = RateFunction::Unit();
	r->isSplit = true;
	r->split = *split;
	splits.push_back( r );
}

// ---------------------------------------------------------------------------
void SimulationLoader::newSplitCompartment( const sgns2::SplitFunction *split ) {
	if( !emptyAddress && specificAddress )
		parser->raiseError( "Cannot split named compartments" );
	if( !selectedType->getParentType() )
		parser->raiseError( "Cannot split Env compartments" );
	if( !ensureTypeInStack( selectedType->getParentType() ) )
		parser->raiseError( "Reactions cannot span laterally across the compartment hierarchy" );

	typeUsed[selectedType->getDepth() - 1] |= 1;

	TempChemical *r = chemicalPool.alloc();
	r->next = NULL;
	r->prev = reactantTail;
	if( reactantTail ) {
		reactantTail->next = r;
	} else {
		reactantHead = r;
	}
	reactantTail = r;
	r->compartment = selectedType->getDepth() - 1;
	r->n = static_cast<uint>(splits.size());
	r->chemical = NULL;
	r->createType = selectedType;
	r->chemicalIdx = (uint)-1; // -1 = unused, >=0 = targets a corresponding compartment split
	r->fn = RateFunction::Unit();
	r->isSplit = true;
	r->split = *split;
	splits.push_back( r );
}

// ---------------------------------------------------------------------------
void SimulationLoader::setRate( const sgns2::RateFunction *rf ) {
	assert( reactantTail );
	reactantTail->fn = *rf;
}

// ---------------------------------------------------------------------------
void SimulationLoader::newProduct( int n ) {
	if( !emptyAddress && specificAddress )
		parser->raiseError( "Reactions that occur in a named compartment only are not currently implemented" );
	if( !ensureTypeInStack( selectedType ) )
		parser->raiseError( "Reactions cannot span laterally across the compartment hierarchy (do you need to reorder the product list?)" );

	if( !rxnProducesCompartment )
		typeUsed[selectedType->getDepth()] |= 2;

	TempChemical *r = chemicalPool.alloc();
	r->next = NULL;
	r->prev = productTail;
	if( productTail ) {
		productTail->next = r;
	} else {
		productHead = r;
	}
	productTail = r;
	r->compartment = selectedType->getDepth();
	r->n = n;
	r->chemical = currentChemical;
	r->chemicalIdx = selectedType->getChemicalIndex( currentChemical, true );
	r->tau = RuntimeDistribution::DeltaDistribution( 0.0 );
	r->isSplit = false;
}

// ---------------------------------------------------------------------------
void SimulationLoader::newSplitProduct( unsigned src, bool splitCompartments ) {
	if( !emptyAddress && specificAddress ) {
		if( splitCompartments ) {
			// --[]--> 1:@named;
			parser->raiseError( "Named compartments cannot be produced" );
		} else {
			parser->raiseError( "Reactions that occur in a named compartment only are not currently implemented" );
		}
	}
	if( !ensureTypeInStack( selectedType ) )
		parser->raiseError( "Reactions cannot span laterally across the compartment hierarchy (do you need to reorder the product list?)" );

	if( !rxnProducesCompartment )
		typeUsed[selectedType->getDepth()] |= 2;

	if( src == 0 ) {
		// An autosplit - search for the first split reactant with the same chemical
		if( splitCompartments ) {
			for( std::vector<TempChemical*>::const_iterator it = splits.begin(); it != splits.end(); ++it ) {
				if( (*it)->chemical == NULL && (*it)->createType == selectedType && (*it)->chemicalIdx == (unsigned)-1 ) {
					src = (*it)->n + 1;
					break;
				}
			}
			if( src == 0 )
				parser->raiseError( "This compartment type is not present as an unreleased split in the reactant list." );
		} else {
			for( std::vector<TempChemical*>::const_iterator it = splits.begin(); it != splits.end(); ++it ) {
				if( (*it)->chemical == currentChemical ) {
					src = (*it)->n + 1;
					break;
				}
			}
			if( src == 0 )
				parser->raiseError( "This chemical is not present as a split in the reactant list. Either remove the ':' or index the split directly." );
		}
	} else if( src > splits.size() ) {
		parser->raiseError( "Split index is greater than the number of splits in the reactant list." );
	}

	src--; // src is 1-based.. our indices are 0-based
	if( splitCompartments ) {
		if( splits[src]->chemical )
			parser->raiseError( "A compartment split product cannot be refer to a population split reactant." );
		if( splits[src]->createType != selectedType )
			parser->raiseError( "Compartment split type must match reactant split type." );
		if( splits[src]->chemicalIdx != (unsigned)-1 )
			parser->raiseError( "This compartment split index has already been released." );
	}

	TempChemical *r = chemicalPool.alloc();
	r->next = NULL;
	r->prev = productTail;
	if( productTail ) {
		productTail->next = r;
	} else {
		productHead = r;
	}
	productTail = r;
	r->tau = RuntimeDistribution::DeltaDistribution( 0.0 );
	r->isSplit = true;
	if( splitCompartments ) {
		r->compartment = selectedType->getDepth() - 1;
		r->n = rxnCompSplitCount;
		r->chemical = NULL;
		splits[src]->chemicalIdx = rxnCompSplitCount;
		rxnCompSplitCount++;
	} else {
		r->compartment = selectedType->getDepth();
		r->n = src;
		r->chemicalIdx = selectedType->getChemicalIndex( currentChemical, true );
		r->chemical = currentChemical;
	}
}

// ---------------------------------------------------------------------------
void SimulationLoader::setTau( const sgns2::RuntimeDistribution *tau ) {
	assert( reactantTail );
	if( productTail->isSplit && productTail->chemical == NULL )
		parser->raiseError( "Compartment split releases cannot be delayed" );
	productTail->tau = *tau;
}

// ---------------------------------------------------------------------------
void SimulationLoader::produceCompartment() {
	if( selectedType->getDepth() == 0 )
		parser->raiseError( "Cannot create Env" );
	if( specificAddress )
		parser->raiseError( "Named compartments cannot be produced" );

	// Put the parent compartment in the type stack
	if( !ensureTypeInStack( selectedType->getParentType() ) )
		parser->raiseError( "Cannot produce this compartment at this point in this reaction (do you need to reorder the product list?)" );
	if( !rxnProducesCompartment ) {
		// This is the first created compartment - ensure that the reaction
		// gets instantiated at the level immediately above it
		typeUsed[selectedType->getDepth()-1] |= 1;
		consolidateReactsIn();
		rxnProducesCompartment = true;
	}
	ensureTypeInStack( selectedType );
	
	// Remove types below the compartment that will have been just created
	curTypeStack.resize( selectedType->getDepth() + 1 );

	// Add the compartment creation to the product sequence
	TempChemical *r = chemicalPool.alloc();
	r->next = NULL;
	r->prev = productTail;
	if( productTail ) {
		productTail->next = r;
	} else {
		productHead = r;
	}
	productTail = r;
	r->compartment = selectedType->getDepth() - 1;
	r->n = 0;
	r->chemical = NULL; // Signals compartment creation
	r->chemicalIdx = 0;
	r->createType = selectedType;
	r->isSplit = false;
}

// ---------------------------------------------------------------------------
void SimulationLoader::eatCompartment() {
	if( selectedType->getDepth() == 0 )
		parser->raiseError( "Cannot destroy Env" );
	if( !emptyAddress && specificAddress ) {
		// @named --[]-->
		parser->raiseError( "Named compartments cannot be destroyed by name" );
	}
	if( rxnDestroysCompartment )
		parser->raiseError( "Cannot destroy two compartments - compartment destruction removes all compartments below a given level in the compartment hierarchy. Destroy the parent compartment." );

	if( !ensureTypeInStack( selectedType ) )
		parser->raiseError( "Reactions cannot span laterally across the compartment hierarchy" );
	typeUsed[selectedType->getDepth()] |= 4;

	rxnDestroysCompartment = selectedType->getDepth();
}

// ---------------------------------------------------------------------------
void SimulationLoader::addWaitListRelease( int n, double time ) {
	sgns2::RuntimeDistribution amt = sgns2::RuntimeDistribution::DeltaDistribution( n );
	sgns2::RuntimeDistribution when = sgns2::RuntimeDistribution::DeltaDistribution( time );

	initCommands.push_back( new init::AddToWaitList( selectedType->getChemicalIndex( currentChemical, true ), &amt, &when ) );
	cmdsSinceAction = 0;
}

// ---------------------------------------------------------------------------
double SimulationLoader::getParameterD( Parameter param ) {
	switch( param ) {
	case SEED: return seed;
	case START_TIME: return startTime;
	case READOUT_INTERVAL: return readoutInterval;
	case STOP_TIME: return stopTime;
	case SAVE_INTERVAL: return saveInterval;
	case SAVE_INDEX: return saveIndex;
	case BATCH_COUNT: return batchCount;
	case BATCH_THREADS: return batchThreads;
	default:
		parser->raiseError( "[internal] Invalid parameter given to getParameterD" );
	}
	return 0.0;
}

// ---------------------------------------------------------------------------
const char *SimulationLoader::getParameterS( Parameter param ) {
	switch( param ) {
	case READOUT_FILE_TEMPLATE: return readoutFile.c_str();
	case READOUT_FILE_HEADER: return fileHeader.c_str();
	case SAVE_FILE_TEMPLATE: return saveFileTemplate.c_str();
	default:
		parser->raiseError( "[internal] Invalid parameter given to getParameterS" );
	}
	return NULL;
}

// ---------------------------------------------------------------------------
void SimulationLoader::setParameterD( Parameter param, double val ) {
	switch( param ) {
	case SEED: 
		seed = (unsigned)val;
	    break;
	case START_TIME:
		startTime = val;
		break;
	case READOUT_INTERVAL:
		readoutInterval = val;
		break;
	case STOP_TIME:
		stopTime = val;
		break;
	case SAVE_INTERVAL:
		saveInterval = val;
		break;
	case SAVE_INDEX:
		saveIndex = val;
		break;
	case BATCH_COUNT:
		batchCount = val;
		break;
	case BATCH_THREADS:
		batchThreads = val;
		break;
	default:
		parser->raiseError( "[internal] Invalid parameter set with setParameterD" );
	}
}

// ---------------------------------------------------------------------------
void SimulationLoader::setParameterS( Parameter param, const char *val ) {
	switch( param ) {
	case READOUT_FILE_TEMPLATE:
		readoutFile = val;
		outputTarget = 0 == strcmp( readoutFile.c_str(), "-" ) ? OUTPUTTGT_STDOUT : OUTPUTTGT_FILE;
		break;
	case READOUT_FILE_HEADER:
		fileHeader = val;
		break;
	case SAVE_FILE_TEMPLATE:
		saveFileTemplate = val;
		break;
	case SAVE_NOW:
		// TODO: Immediate save
		assert(false);
		break;
	default:
		parser->raiseError( "[internal] Invalid parameter set with setParameterS" );
	}
}

// ---------------------------------------------------------------------------
void SimulationLoader::saveAt( double time, const char *filename ) {
	(void)time;
	(void)filename;
	assert( false );
}

// ---------------------------------------------------------------------------
void SimulationLoader::issueWarning( const parse::Error *warning ) {
	std::cerr << "Warning at " << warning->getContext() << '(' << warning->getLineNo() << "): " << warning->getMessage() << std::endl;
}

// ---------------------------------------------------------------------------
void SimulationLoader::loadingComplete() {
	// Clear trailing non-useful init commands
	while( cmdsSinceAction ) {
		initCommands.pop_back();
		cmdsSinceAction--;
	}

	// Slap the appropriate file extension on if necessary
	if( readoutFile[readoutFile.length()-1] == '?' ) {
		readoutFile = readoutFile.substr(0, readoutFile.length() - 1);
		switch( outputFormat ) {
		case OUTPUT_CSV:
			readoutFile.append( "csv" );
			break;
		case OUTPUT_TSV:
			readoutFile.append( "tsv" );
			break;
		case OUTPUT_BIN32:
		case OUTPUT_BIN64:
			readoutFile.append( "bin" );
			break;
		default:
			readoutFile.append( "txt" );
			break;
		}
	}

	// Seal all reaction banks
	for( CompTypeMap::iterator it = compTypes.begin(); it != compTypes.end(); ++it )
		(*it).second.type->getBank()->seal();

	// Clear intermediate reaction memory
	resetReaction();
	chemicalPool.reset();
}

// ---------------------------------------------------------------------------
void SimulationLoader::beginBatchRun() {
	if( parser->hasRuntimeLua() ) {
		L_packedcapacity = 128;
		L_packed = malloc( L_packedcapacity );
		L_packedsize = 0;

		lua_State *L = parser->getL();
		lua_newtable( L );
		lua_newtable( L );
		lua_pushvalue( L, LUA_GLOBALSINDEX );
		lua_rawseti( L, -2, 1 );
		lua_pushvalue( L, LUA_REGISTRYINDEX );
		lua_rawseti( L, -2, 2 );
		pluto_persist( L, &SimulationLoader::L_saver, this );
	}
}

// ---------------------------------------------------------------------------
const char *SimulationLoader::L_loader( lua_State *L, void *ud, size_t *sz ) {
	(void)L;
	SimulationLoader *ld = (SimulationLoader*)ud;
	*sz = ld->L_packedsize;
	return (const char*)ld->L_packed;
}

// ---------------------------------------------------------------------------
int SimulationLoader::L_saver( lua_State *L, const void *p, size_t sz, void *ud ) {
	(void)L;
	SimulationLoader *ld = (SimulationLoader*)ud;
	if( ld->L_packedsize + sz > ld->L_packedcapacity ) {
		do {
			ld->L_packedcapacity <<= 1;
		} while( ld->L_packedcapacity < ld->L_packedsize + sz );
		void *newDat = malloc( ld->L_packedcapacity );
		memcpy( newDat, ld->L_packed, ld->L_packedsize );
		free( ld->L_packed );
		ld->L_packed = newDat;
	}

	memcpy( (char*)ld->L_packed + ld->L_packedsize, p, sz );
	ld->L_packedsize += static_cast<uint>(sz);
	return 0;
}

// ---------------------------------------------------------------------------
void SimulationLoader::beginSimulation( SimulationInstance *&sim, HierCompartment *&env, unsigned seedOffset ) {
	// Get or build an appropriate lua state
	lua_State *L;
	if( L_packed ) {
		L = luaL_newstate();
		pluto_unpersist( L, &SimulationLoader::L_loader, this );
		lua_rawgeti( L, -1, 1 );
		lua_replace( L, LUA_GLOBALSINDEX );
		lua_rawgeti( L, -1, 2 );
		lua_replace( L, LUA_REGISTRYINDEX );
	} else {
		L = parser->getL();
	}

	// Set up the new instance and init context
	sim = new SimulationInstance( seed + seedOffset, L );
	sim->distrCtx()->allocateSplitBuffer( maxSplitCount );
	init::Context initCtx( sim, envType );
	env = initCtx.env;

	// Run all init commands
	for( InitCmdList::const_iterator it = initCommands.begin(); it != initCommands.end(); ++it )
		(*it)->execute( &initCtx );

	// Make sure the sim is up to date before the first step
	sim->update();
}

// ---------------------------------------------------------------------------
void SimulationLoader::resetReaction() {
	for( TempChemical *r = reactantHead; r; ) {
		TempChemical *nextR = r->next;
		chemicalPool.free( r );
		r = nextR;
	}
	for( TempChemical *r = productHead; r; ) {
		TempChemical *nextR = r->next;
		chemicalPool.free( r );
		r = nextR;
	}

	rxnProducesCompartment = false;
	rxnDestroysCompartment = 0;
	
	reactantHead = reactantTail = NULL;
	productHead = productTail = NULL;
	extraCommands = NULL;
	curTypeStack.clear();
	reactsIn.clear();
	typeUsed.clear();
	rxnHEval = NULL;
	splits.clear();
	rxnCompSplitCount = 0;
}

// ---------------------------------------------------------------------------
void SimulationLoader::consolidateReactsIn() {
	if( reactsIn.empty() ) {
		reactsIn = curTypeStack;
		if( rxnDestroysCompartment )
			curTypeStack.resize( rxnDestroysCompartment );
	}
}

// ---------------------------------------------------------------------------
bool SimulationLoader::ensureTypeInStack( CompartmentType *type ) {
	uint depth = type->getDepth();
	if( curTypeStack.size() <= depth ) {
		curTypeStack.resize( depth + 1, NULL );
		if( !rxnProducesCompartment )
			typeUsed.resize( depth + 1, 0 );
	}

	while( true ) {
		if( curTypeStack[depth] ) {
			if( curTypeStack[depth] == type )
				break;
			return false; // Lateral compartment span
		} else {
			curTypeStack[depth] = type;
		}

		if( depth == 0 )
			break;

		type = type->getParentType();
		depth--;
	}

	return true;
}

// ---------------------------------------------------------------------------
SimulationLoader::InitCmdExtra::InitCmdExtra() {
}

// ---------------------------------------------------------------------------
SimulationLoader::InitCmdExtra::~InitCmdExtra() {
	for( InitCmdList::const_iterator it = commands.begin(); it != commands.end(); ++it )
		delete *it;
}

// ---------------------------------------------------------------------------
void SimulationLoader::InitCmdExtra::execute( const reaction::Template *tmplate, Compartment **context ) {
	(void)tmplate;
	init::Context ctx(static_cast<HierCompartment*>(context[0]));
	for( InitCmdList::const_iterator it = commands.begin(); it != commands.end(); ++it )
		(*it)->execute( &ctx );
}


// ---------------------------------------------------------------------------
double SGNS_FASTCALL SimulationLoader::hEval_fa2a1r( Compartment **context, reaction::Reactant *r ) {
	// Fractional Activation, Two Activators, One Repressor

	// (k0+k1*x1+ k2*x2+k12*x1*x2)/(1+k1*x1+ k2*x2+ k12*x1*x2+k3*x3+k13*x1*x3+k23*x2*x3+k123*x1*x2*x3)
	double k0 = r->getRateFunction()->p0.d;
	double k123 = r->getRateFunction()->p1.d;
	double x1 = (double)r->getPopulationIn( context );
	r = r->getNext();

	double k1 = r->getRateFunction()->p0.d;
	double k2 = r->getRateFunction()->p1.d;
	double k3 = r->getRateFunction()->p2.d;
	double x2 = (double)r->getPopulationIn( context );
	r = r->getNext();

	double k12 = r->getRateFunction()->p0.d;
	double k23 = r->getRateFunction()->p1.d;
	double k13 = r->getRateFunction()->p2.d;
	double x3 = (double)r->getPopulationIn( context );
	r = r->getNext();

	double h = (k0+k1*x1+ k2*x2+k12*x1*x2)/(1+k1*x1+ k2*x2+ k12*x1*x2+k3*x3+k13*x1*x3+k23*x2*x3+k123*x1*x2*x3);
	for( ; r; r = r->getNext() )
		h *= r->evaluate( context );
	return h;
}

// ---------------------------------------------------------------------------
double SGNS_FASTCALL SimulationLoader::hEval_sshdimer( Compartment **context, reaction::Reactant *r ) {
	// Steady state heterodimer

	// 0.5*   k*(1+(x1+x2)/k-((1+(x1+x2)/k)^2-4*x1*x2/k^2)^0.5)
	double k = r->getRateFunction()->p2.d;
	double x1 = (double)r->getPopulationIn( context );
	r = r->getNext();
	double x2 = (double)r->getPopulationIn( context );
	r = r->getNext();
	double x1x2_k = 1+(x1+x2)/k;
	double h = k * (1 + (x1+x2)/k - sqrt(x1x2_k*x1x2_k - 4*x1*x2/(k*k)));
	for( ; r; r = r->getNext() )
		h *= r->evaluate( context );
	return h;
}

// ---------------------------------------------------------------------------
double SGNS_FASTCALL SimulationLoader::hEval_lua( Compartment **context, reaction::Reactant *reactants ) {
	// General Lua evaluation function

	lua_State *L = context[0]->getSimulation()->getL();
	lua_rawgeti( L, LUA_REGISTRYINDEX, reactants->getRateFunction()->p0.i );
	int n = 0;
	for( reaction::Reactant *r = reactants; r; r = r->getNext() ) {
		lua_pushnumber( L, (lua_Number)r->getPopulationIn( context ) );
		n++;
	}

	if( lua_pcall( L, n, 1, 0 ) || !lua_isnumber( L, 1 ) ) {
		lua_pop( L, 1 );
		return 1.0;
	}

	double h = (double)lua_tonumber( L, 1 );
	lua_pop( L, 1 );
	return h;
}
