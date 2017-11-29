/*
Copyright (c) 2011, Jason Lloyd-Price, Abhishekh Gupta
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

/* main.cpp

Contents:
	- Reading the command line
	- Running simulation batches
	- Sampling the simulation
	- Progress reporting
	- Performance output
*/

#include "stdafx.h"

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <ctime>
#include <algorithm>
#include <clocale>

#include "platform.h"
#include "simulationloader.h"
#include "multithread.h"
#include "simulationsampler.h"
#include "samplertarget.h"

// Program information
#define PROGNAME "SGNS"
#define VERSION "2.1"
#define BUG_EMAIL "jason.lloyd-price@tut.fi"

// Performance output information
sgns2::uint64 g_stepCount = 0;
clock_t g_startClock;
clock_t g_initClock;
clock_t g_finishClock;

// ---------------------------------------------------------------------------
static void printVersion() {
	unsigned version =
#include "../version"
		+ 0;

	std::cout << PROGNAME << " " VERSION "." << version;
#ifndef DEPLOY
#if defined(DEBUG) || defined(_DEBUG)
	std::cout << " DEBUG BUILD";
#else
	std::cout << " PROTOTYPE";
#endif
	std::cout << ", built " << __DATE__ << " at " << __TIME__;
#endif
	std::cout << std::endl << " by Jason Lloyd-Price, Abhishekh Gupta, and Andre S. Ribeiro" << std::endl;
}

// ---------------------------------------------------------------------------
static void printHelp( const char *cmd ) {
	printVersion();
	std::cout << std::endl;
	std::cout << cmd << " [options] <simfile> [options]" << std::endl;
	std::cout << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  -p                 Output progress at each sample point" << std::endl;
	std::cout << "  -P                 Output performance and model information" << std::endl;
	std::cout << "  -b<batch count>    Runs <batch count> independent simulations" << std::endl;
	std::cout << "  -T<threads>        Limits the number of threads used in batch mode" << std::endl;
	std::cout << "  -i<filename>       Equivalent to --import <filename>" << std::endl;
	std::cout << "                     Use -i- to read from stdin" << std::endl;
	std::cout << "  -o<filename>       Equivalent to --output_file <filename>" << std::endl;
	std::cout << "                     Use -o- to output to stdout" << std::endl;
	std::cout << "  -f<format>         Equivalent to --output_format <format>" << std::endl;
	std::cout << "                     Formats: csv (default), tsv, bin32, bin64, none" << std::endl;
	std::cout << "  !<lua-code>        Executes the given Lua code immediately" << std::endl;
	std::cout << "  -t[<start>-]<stop>[:<interval>]" << std::endl;
	std::cout << "                     Set the simulation time to <start> (or 0 if <start> is" << std::endl;
	std::cout << "                     ommitted), the stop_time to <stop> and the readout" << std::endl;
	std::cout << "                     interval to <interval> (or 1 if omitted)" << std::endl;
	std::cout << "                     E.g. -t200:1 or -t50-100" << std::endl;
	std::cout << "  +<param>=<value>   Equivalent to --parameter \"<param> = <value>\"" << std::endl;
	std::cout << "  -?  or  --help     Display this message" << std::endl;
	std::cout << "  --<id> <data>      Parses <data> with identifier <id>" << std::endl;
	std::cout << "                     E.g. --reaction \"2A --[k_react]--> C\"" << std::endl;
	std::cout << "  --                 Interpret the remaining arguments as filenames" << std::endl;
	std::cout << std::endl;
	std::cout << "A command line argument that does not start with -, +, / or ! is interpreted" << std::endl;
	std::cout << "as a simulation file. It is interpreted so that the following are equivalent:" << std::endl;
	std::cout << "    " << cmd << " sim.g" << std::endl;
	std::cout << "    " << cmd << " -o sim.csv -i sim.g" << std::endl;
	std::cout << std::endl;
	std::cout << "The command line is read in the order it is given. If a simfile changes" << std::endl;
	std::cout << "settings, the corresponding options must be given *after* the simfile." << std::endl;
	std::cout << "E.g. if stop_time is given in sim.g, -t must be set after sim.g is included:" << std::endl;
	std::cout << "    " << cmd << " sim.g -t1000" << std::endl;
	std::cout << std::endl;
	std::cout << "Similarly, parameters used in the simfile must be specified *before*, as in:" << std::endl;
	std::cout << "    " << cmd << " +k=2.8 sim.g" << std::endl;
	std::cout << std::endl;
	std::cout << "Send bug reports to: " << BUG_EMAIL << std::endl;
	std::cout << std::endl;
}

// ---------------------------------------------------------------------------
static void parseTimeSwitch( const char *here, const char *arg, sgns2::SimulationLoader *ld ) {
	// -t[start-]<end>[:interval]
	const char *endP, *startP = arg;
	double startTime = 0.0;
	double t = strtod( startP, const_cast<char**>(&endP) );
	if( *endP == '-' ) {
		startTime = t;
		t = strtod( startP = endP + 1, const_cast<char**>(&endP) );
	}

	if( endP == startP ) {
		std::cerr << here << ": Expected simulation time" << std::endl;
		exit(1);
	} else if( t < startTime ) {
		std::cerr << here << ": Stop time cannot be before start time" << std::endl;
		if( startTime < 0.0 )
			std::cerr << "Set negative stop times as: -t-20--10" << std::endl;
		exit(1);
	}
	ld->setParameterD( sgns2::parse::ParseListener::START_TIME, startTime );
	ld->setParameterD( sgns2::parse::ParseListener::STOP_TIME, t );

	if( *endP == ':' ) {
		t = strtod( startP = endP + 1, const_cast<char**>(&endP) );
		if( endP == startP ) {
			std::cerr << here << ": Expected readout interval" << std::endl;
			exit(1);
		}
		ld->setParameterD( sgns2::parse::ParseListener::READOUT_INTERVAL, t );
	}

	if( *endP ) {
		std::cerr << here << ": Unexpected symbols after time" << std::endl;
		exit(1);
	}
}

// ---------------------------------------------------------------------------
static void parseSimFileArg( const char *here, const char *arg, sgns2::SimulationLoader *ld ) {
	char output_filename[PATH_MAX];
	strcpy( output_filename, arg );
	char *ext = strrchr( output_filename, '.' );
	if( !ext ) ext = output_filename + strlen( output_filename );
	strcpy( ext, ".?" );

	ld->getParser()->parse( here, "output_file", output_filename );
	ld->getParser()->parse( here, "import", arg );
}

// ---------------------------------------------------------------------------
static void parseCommandLine( int argc, const char **argv, sgns2::SimulationLoader *ld ) {
	if( argc == 1 ) {
		// No arguments - output usage
		printHelp( argv[0] );
		exit(0);
	}

	try {
		char here[32];
		strcpy( here, "cmdline(" );
		for( int arg = 1; arg < argc; arg++ ) {
			sprintf( here + 8, "%d", arg );
			strcat( here + 8, ")" );

			switch( argv[arg][0] ) {
			case '-':
			case '/':
				switch( argv[arg][1] ) {
				case 'p':
					// Progress
					ld->getParser()->parse( here, "progress", "on" );
					break;
				case 'P':
					// Performance
					ld->getParser()->parse( here, "performance", "on" );
					break;
				case 't': {
					// -t<sim time>
					const char *src = argv[arg] + 2;
					if( !*src ) {
						if( ++arg >= argc ) {
							std::cerr << here << ": Expected Simulation Time" << std::endl;
							exit(1);
						}
						src = argv[arg];
					}
					parseTimeSwitch( here, src, ld );
				} break;
				case 'b': {
					// -b<batch count>
					const char *src = argv[arg] + 2;
					if( !*src ) {
						if( ++arg >= argc ) {
							std::cerr << here << ": Expected Batch Count" << std::endl;
							exit(1);
						}
						src = argv[arg];
					}
					ld->getParser()->parse( here, "batch_count", src );
				} break;
				case 'T': {
					// -T<threads>
					const char *src = argv[arg] + 2;
					if( !*src ) {
						if( ++arg >= argc ) {
							std::cerr << here << ": Expected Thread Count" << std::endl;
							exit(1);
						}
						src = argv[arg];
					}
					ld->getParser()->parse( here, "batch_threads", src );
				} break;
				case 'i': {
					// -i<input file>
					const char *src = argv[arg] + 2;
					if( !*src ) {
						if( ++arg >= argc ) {
							std::cerr << here << ": Expected input filename" << std::endl;
							exit(1);
						}
						src = argv[arg];
					}
					ld->getParser()->parse( here, "import", src );
				} break;
				case 'o': {
					// -o<output file>
					const char *src = argv[arg] + 2;
					if( !*src ) {
						if( ++arg >= argc ) {
							std::cerr << here << ": Expected output filename" << std::endl;
							exit(1);
						}
						src = argv[arg];
					}
					ld->getParser()->parse( here, "output_file", src );
				} break;
				case 'f': {
					// -f<format>
					const char *src = argv[arg] + 2;
					if( !*src ) {
						if( ++arg >= argc ) {
							std::cerr << here << ": Expected format name" << std::endl;
							exit(1);
						}
						src = argv[arg];
					}
					ld->getParser()->parse( here, "output_format", src );
				} break;
				case '?':
					printHelp( argv[0] );
					exit(0);
				case '-':
					if( argv[arg][0] == '-' ) {
						// --
						switch( argv[arg][2] ) {
						case '\0': {
							// Read all the remaining arguments as simulation filenames
							for( arg++; arg < argc; arg++ ) {
								sprintf( here + 8, "%d", arg );
								strcat( here + 8, ")" );
								parseSimFileArg( here, argv[arg], ld );
							}
						} break;
						case 'v': 
							if( strcmp( argv[arg] + 2, "version" ) == 0 ) {
								printVersion();
								exit(0);
							}
							break;
						case 'h':
							if( strcmp( argv[arg] + 2, "help" ) == 0 ) {
								printHelp( argv[0] );
								exit(0);
							}
							break;
						}

						const char *id = argv[arg] + 2;
						const char *data = "";
						if( ++arg < argc )
							data = argv[arg];
						ld->getParser()->parse( here, id, data );
					} break;
				default:
					std::cerr << here << ": Unknown switch '" << argv[arg][1] << "'." << std::endl;
					exit(1);
				}
				break;
			case '+':
				// +<param>=<value> or +<param> <value>
				if( strchr( argv[arg], '=' ) ) {
					ld->getParser()->parse( here, "parameter", argv[arg] + 1 );
				} else if( arg + 1 < argc ) {
					char paramCmd[1024];
					strncpy( paramCmd, argv[arg] + 1, 1024 );
					strncat( paramCmd, "=", 1024 );
					strncat( paramCmd, argv[++arg], 1024 );
					ld->getParser()->parse( here, "parameter", paramCmd );
				} else {
					std::cerr << "Expected parameter value";
				}
				break;
			case '!':
				// !<lua-code>
				ld->getParser()->parse( here, "lua", &argv[arg][1] );
				break;
			default:
				// Simulation file
				parseSimFileArg( here, argv[arg], ld );
				break;
			}
		}
	} catch( unsigned ) {
		// TODO: Move this somewhere else..
		const sgns2::parse::Error *err = ld->getParser()->getLastError();
		std::cerr << std::endl;
		std::cerr << "Error in " << err->getContext() << ":" << err->getLineNo() << ": " << err->getMessage() << std::endl;
		std::cerr << err->getLine() << std::endl;
		std::cerr << std::string(std::max(err->getCharNo(),1u)-1,' ') << '^' << std::endl << std::endl;
		exit(1);
	}
}

// ---------------------------------------------------------------------------
// SimulationSamplerEvent is a small EventStream used to call the
// SimulationSampler ar fixed intervals
class SimulationSamplerEvent : public sgns2::EventStream {
public:
	SimulationSamplerEvent( SimulationSampler *sampler, double interval, sgns2::SimulationInstance *sim, sgns2::HierCompartment *env, bool showProgress = false )
		: sgns2::EventStream(sim->getParallelQueue()), batchIndex((unsigned)-1), lastStepCount(0), showProgress(showProgress)
		, sampler(sampler), interval(interval), sim(sim), env(env)
	{ }

	inline void setBatchIndex( unsigned idx ) {
		batchIndex = idx;
	}

	virtual ~SimulationSamplerEvent() throw() {
	}

	virtual void begin() {
		enqueue( sim->getTime() );
	}

	virtual void trigger() {
		// Sample the system now
		if( showProgress ) {
			sgns2::uint64 steps = sim->getStepCount();
			if( batchIndex != (sgns2::uint)-1 )
				std::cout << "Sim " << batchIndex << ": ";
			std::cout << "Time = " << sim->getTime() << "; Step Delta = " << (steps - lastStepCount) << std::endl;
			lastStepCount = steps;
		}
		sampler->sampleState( sim, env );
		enqueue( sim->getTime() + interval );
	}

	virtual void update() { }

private:
	sgns2::uint batchIndex; // Index of the current batch
	sgns2::uint64 lastStepCount; // Step count for progress reporting
	bool showProgress; // Should we output progress at each sample?

	SimulationSampler *sampler; // The target sampler
	double interval; // Sample interval
	sgns2::SimulationInstance *sim; // The simulation instance
	sgns2::HierCompartment *env; // The root compartment to sample
};

// ---------------------------------------------------------------------------
static sgns2::uint64 runSim( sgns2::SimulationLoader *ld, SimulationSampler *samp, unsigned idx ) {
	// Instantiate and run the simulation with the given sampler
	// idx is the index of the simulation within the batch or -1 if
	// not running in batch mode

	sgns2::SimulationInstance *sim;
	sgns2::HierCompartment *env;
	ld->beginSimulation( sim, env, idx == (unsigned)-1 ? 0 : idx );

	sim->setTime( ld->getParameterD( sgns2::parse::ParseListener::START_TIME ) );

	if( ld->getParameterD( sgns2::parse::ParseListener::READOUT_INTERVAL ) <= 0.0 ) {
		// Sample every step
		double stopTime = ld->getParameterD( sgns2::parse::ParseListener::STOP_TIME );
		while( true ) {
			sgns2::EventStream *evt = sim->runStep();
			if( !evt || sim->getTime() > stopTime )
				break;
			samp->sampleState( sim, env );
		}
	} else {
		// Sample periodically
		double sampInterval = ld->getParameterD( sgns2::parse::ParseListener::READOUT_INTERVAL );
		SimulationSamplerEvent sampEvt( samp, sampInterval, sim, env, ld->shouldShow( sgns2::SimulationLoader::SHOW_PROGRESS ) );
		sampEvt.setBatchIndex( idx );
		sampEvt.begin();
		sim->runUntil( ld->getParameterD( sgns2::parse::ParseListener::STOP_TIME ) );
	}

	sgns2::uint64 steps = sim->getStepCount();

	delete env; // Frees all memory associated with the simulation
	delete sim;

	return steps;
}

// ---------------------------------------------------------------------------
static void cleanFileNamePatternInto( const char *fn, const char *beforeExt, char *fnTgt, bool clean = false ) {
	// Cleans the filename so that it can be safely sprintf'd and inserts a
	// string before the last '.' in the filename

	const char *ext = NULL;
	char *extTgt = NULL;
	while( *fn ) {
		if( *fn == '%' && clean ) {
			*(fnTgt++) = '%';
		} else if( *fn == '.' ) {
			ext = fn;
			extTgt = fnTgt;
		}
		*(fnTgt++) = *(fn++);
	}

	if( !ext ) {
		strcpy( fnTgt, beforeExt );
	} else {
		size_t bfExtLen = strlen( beforeExt );
		char *tgtAfterBfExt = fnTgt + bfExtLen;
		*tgtAfterBfExt = '\0';
		while( fnTgt != extTgt )
			*(--tgtAfterBfExt) = *(--fnTgt);
		memcpy( fnTgt, beforeExt, bfExtLen );
	}
}

// ---------------------------------------------------------------------------
static sgns2::uint64 runSim( sgns2::SimulationLoader *ld, unsigned index = (unsigned)-1 ) {
	// Sets up the sampler for a simulation and runs one simulation

	// Set up output
	if( ld->getOutputFormat() == sgns2::SimulationLoader::OUTPUT_NULL ) {
		NullSampler samp;
		return runSim( ld, &samp, index );
	}
	
	SamplerTarget *outputTarget;
	if( ld->getOutputTarget() == sgns2::SimulationLoader::OUTPUTTGT_FILE ) {
		char compartmentPattern[PATH_MAX], preExt[32] = "";
		char srcFn[PATH_MAX];
		if( index != (unsigned)-1 ) {
			// Include the batch index if this is one run of many
			sprintf( preExt, "#%d", index );
			cleanFileNamePatternInto( ld->getParameterS( sgns2::parse::ParseListener::READOUT_FILE_TEMPLATE ), preExt, &srcFn[0], false );
		} else {
			// Single run
			strcpy( srcFn, ld->getParameterS( sgns2::parse::ParseListener::READOUT_FILE_TEMPLATE ) );
		}

		strcpy( preExt, "@%s-%d" );
		cleanFileNamePatternInto( srcFn, preExt, &compartmentPattern[0] );
		outputTarget = new FileSamplerTarget( srcFn, compartmentPattern );
	} else { // sgns2::SimulationLoader::OUTPUTTGT_STDOUT
		outputTarget = new StdoutSamplerTarget;
	}

	SimulationSampler *sampler;
	switch( ld->getOutputFormat() ) {
	case sgns2::SimulationLoader::OUTPUT_BIN32:
		sampler = new Bin32Sampler( outputTarget, ld );
		break;
	case sgns2::SimulationLoader::OUTPUT_BIN64:
		sampler = new Bin64Sampler( outputTarget, ld );
		break;
	case sgns2::SimulationLoader::OUTPUT_CSV:
		sampler = new DlmTextSampler( outputTarget, ld, ',' );
		break;
	case sgns2::SimulationLoader::OUTPUT_TSV:
		sampler = new DlmTextSampler( outputTarget, ld, '\t' );
		break;
	default:
		sampler = new NullSampler();
		break;
	}

	sgns2::uint64 steps = runSim( ld, sampler, index );

	delete sampler;
	delete outputTarget;

	return steps;
}

// ---------------------------------------------------------------------------
struct BatchMTContext {
	// Struct used in the following two functions
	// Stores the synchronization primitives used to run multiple simulations
	// in parallel

	void *batchMutex;
	void *endSema;
	unsigned batchIndex;
	unsigned batchCount;
	sgns2::SimulationLoader *ld;
};

// ---------------------------------------------------------------------------
static void runBatchMT( BatchMTContext *ctx ) {
	// Main function for the batch worker thread - runs simulations until
	// there are no more simulations to run

	sgns2::uint64 steps = 0;
	while( true ) {
		unsigned batchNo;
		mt::lock( ctx->batchMutex );
		g_stepCount += steps;
		if( ctx->batchIndex < ctx->batchCount ) {
			// Start the next batch
			batchNo = ctx->batchIndex++;
		} else {
			// Batches done
			mt::unlock( ctx->batchMutex );
			mt::v( ctx->endSema );
			return;
		}
		mt::unlock( ctx->batchMutex );

		steps = runSim( ctx->ld, batchNo );
	}
}

// ---------------------------------------------------------------------------
void runBatch( sgns2::SimulationLoader *ld ) {
	// Runs a batch of simulations
	// Determines what multithreading is available and creates threads as
	// necessary to occupy all logical cores

	{
		// Is the stop time after the start time?
		double startTime = ld->getParameterD( sgns2::parse::ParseListener::START_TIME );
		double stopTime = ld->getParameterD( sgns2::parse::ParseListener::STOP_TIME );
		if( stopTime <= startTime ) {
			std::cout << "The simulation stop time is before the start time. Doing nothing." << std::endl;
			return;
		}
	}

	double dBatches = ld->getParameterD( sgns2::parse::ParseListener::BATCH_COUNT );
	unsigned uBatches = (unsigned)floor( dBatches );

	if( dBatches < 2.0 ) {
		// Single run
		if( uBatches == 1 )
			g_stepCount = runSim( ld );
	} else {
		double dThreads = ld->getParameterD( sgns2::parse::ParseListener::BATCH_THREADS );
		unsigned uThreads = (unsigned)floor( dThreads );
		if( dThreads < 1.0 )
			uThreads = mt::coreCount();

		if( uThreads == 1 ) {
			// Single-core processor or -T1
			// Run all simulations in this thread
			g_stepCount = 0;
			for( unsigned i = 0; i < uBatches; i++ )
				g_stepCount += runSim( ld, i );
		} else {
			// Multi-threaded batch
			BatchMTContext ctx;
			ctx.batchMutex = mt::newMutex();
			ctx.endSema = mt::newSemaphore( 0 );
			ctx.batchIndex = 0;
			ctx.batchCount = uBatches;
			ctx.ld = ld;
			ld->beginBatchRun();

			// Don't spawn more threads than simulations
			uThreads = std::min( uThreads, uBatches );

			// Spawn one worker for each core
			for( unsigned i = 1; i < uThreads; i++ )
				mt::spawnThread( (void (*)(void*))&runBatchMT, &ctx );
			runBatchMT( &ctx ); // Use this thread as well

			// Wait for completion of the batch
			for( unsigned i = 0; i < uThreads; i++ )
				mt::p( ctx.endSema );

			// Done
			mt::deleteMutex( ctx.batchMutex );
			mt::deleteSemaphore( ctx.endSema );
		}
	}
}

// ---------------------------------------------------------------------------
double getClockspeed() {
	// Platform-specific clockspeed access for performance display

#ifdef _WIN32
	char szKey[256];
	HKEY hKey;
	DWORD dwSpeed;

	_snprintf( szKey, sizeof(szKey)/sizeof(char), "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0\\" );

	// Open the key
	if( RegOpenKeyExA( HKEY_LOCAL_MACHINE, szKey, 0, KEY_QUERY_VALUE, &hKey ) != ERROR_SUCCESS )
		return 0;

	// Read the value
	DWORD dwLen = 4;
	if( RegQueryValueExA( hKey, "~MHz", NULL, NULL, (LPBYTE)&dwSpeed, &dwLen ) != ERROR_SUCCESS ) {
		RegCloseKey( hKey );
		return 0;
	}

	// Cleanup and return
	RegCloseKey( hKey );
    return dwSpeed * 1.0e6;
#else
	return 0.0;
#endif
}

// ---------------------------------------------------------------------------
void showPerformance( sgns2::SimulationLoader *ld ) {
	// Performance display

	double initTime = ((double)(g_initClock - g_startClock) / CLOCKS_PER_SEC);
	double runTime = ((double)(g_finishClock - g_initClock) / CLOCKS_PER_SEC);
	std::cout << "Model statistics:" << std::endl;
	std::cout << "    Reactions:      " << ld->getReactionCount() << std::endl;
	std::cout << "    Elements:       " << ld->getChemicalCount() << std::endl;
	std::cout << "    Total steps:    " << g_stepCount << std::endl;
	if( ld->getParameterD( sgns2::parse::ParseListener::BATCH_COUNT ) >= 2.0 )
		std::cout << "    Steps / sim:    " << (g_stepCount / floor(ld->getParameterD( sgns2::parse::ParseListener::BATCH_COUNT ))) << std::endl;
	std::cout << "Performance:" << std::endl;
	std::cout << "    Init time:      " << initTime << " s" << std::endl;
	std::cout << "    Run time:       " << runTime << " s" << std::endl;
	unsigned stepsPerSec = (unsigned)floor( g_stepCount / runTime );
	std::cout << "    Steps / sec:    " << stepsPerSec << std::endl;
#ifdef _WIN32
	PROCESS_MEMORY_COUNTERS mem;
	if( GetProcessMemoryInfo( GetCurrentProcess(), &mem, sizeof( mem ) ) ) {
		std::cout << "    Peak Mem Use:   " << mem.PeakWorkingSetSize << std::endl;
	}
#endif
	double clocksPerSec = getClockspeed();
	if( clocksPerSec > 0.0 ) {
		std::cout << "    CPU Clockspeed: " << (clocksPerSec / 1.0e6) << " MHz" << std::endl;
		std::cout << "    Clocks / step:  " << (clocksPerSec / stepsPerSec) << std::endl;
	}
}

// ---------------------------------------------------------------------------
int main( int argc, const char **argv ) {
	setlocale( LC_ALL, "C" );
	sgns2::SimulationLoader ld;

	// Read command line (loads the model)
	g_startClock = clock();
	parseCommandLine( argc, argv, &ld );
	ld.loadingComplete();
	g_initClock = clock();
	
	// Run the simulations
	runBatch( &ld );
	g_finishClock = clock();

	// All done - print performance
	if( ld.shouldShow( sgns2::SimulationLoader::SHOW_PERFORMANCE ) )
		showPerformance( &ld );

	return 0;
}
