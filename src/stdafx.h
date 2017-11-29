
#ifdef USE_PCH

// Lua headers
#include <lua.hpp>
extern "C" {
#include <pluto.h>
}

// Libraries
#include <algorithm>

// Core headers
#include "chemical.h"
#include "compartment.h"
#include "compartmenttype.h"
#include "distribution.h"
#include "event.h"
#include "hiercompartment.h"
#include "rate.h"
#include "reaction.h"
#include "reactionbank.h"
#include "simtypes.h"
#include "simulation.h"
#include "split.h"
#include "waitlist.h"

// Utility headers
#include "mempool.h"
#include "MersenneTwister.h"
#include "rng.h"
#include "simplesll.h"

// Main headers
#include "multithread.h"
//#include "platform.h"
#include "samplertarget.h"
#include "sbmlreader.h"
#include "simulationinit.h"
#include "simulationloader.h"
#include "simulationsampler.h"

// Parser headers
#include "parser.h"
#include "parsestream.h"

#endif // USE_PCH
