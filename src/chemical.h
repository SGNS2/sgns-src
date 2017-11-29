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

/* chemical.h/cpp

Chemical class contents:
	- Stores the name of the chemical and whether to output it in the output files
*/

#ifndef CHEMICAL_H
#define CHEMICAL_H

#include <vector>
#include <string>

namespace sgns2 {

class Chemical {
public:
	// Creates a chemical with the given name
	explicit Chemical( const char *name );
	explicit Chemical( const std::string name );
	~Chemical();

	// Access to the Chemical's name
	inline const std::string &getName() const { return name; }
	// Should this Chemical be present in output files?
	inline bool shouldOutput() const { return outputChemical; }
	// Set whether this chemical shoult be present in output files
	inline void setOutput( bool output ) { outputChemical = output; }

private:
	std::string name; // Chemical name
	bool outputChemical; // Should be in output files?
};

} // namespace sgns2

#endif // CHEMICAL_H
