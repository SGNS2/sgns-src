
// See chemical.h for a description of the contents of this file.

#include "stdafx.h"

#include "chemical.h"

namespace sgns2 {

// ---------------------------------------------------------------------------
Chemical::Chemical( const char *name )
: name(name), outputChemical(true)
{
}

// ---------------------------------------------------------------------------
Chemical::Chemical( const std::string name )
: name(name), outputChemical(true)
{
}

// ---------------------------------------------------------------------------
Chemical::~Chemical() {
}

} // namespace sgns2
