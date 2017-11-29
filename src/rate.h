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

/* rate.h/cpp

RateFunction class contents:
	- Function pointer to a calculation function
	- Two multi-purpose parameters
	- Simple rate functions: Unit and Linear

BasicRateFunction class contents:
	- Some more functions useful in SGNS
*/

#ifndef _RATE_H
#define _RATE_H

#include "simtypes.h"

namespace sgns2 {

class RateFunction
{
public:
	RateFunction();
	~RateFunction();

	// Indirection is done with a function pointer directly to remove
	// one unnecessary level of indirection
	typedef double (SGNS_FASTCALL *Function)( RateFunction *me, Population X );

	inline double evaluate( Population X ) {
		return fn( this, X );
	}

	inline bool isUnit() { return fn == &unitRateFunction; }

	// f(x) = 1
	static RateFunction Unit();
	// f(x) = x
	static RateFunction Linear();


	// Semi-hackish: These parameters are free to be co-opted by other
	// functions (in particular, special H-functions)
	union Parameter
	{
		double d;
		int i;
		unsigned int ui;
		void *p;
		Population pop;
	};

	union {
		Function fn;
		Parameter p0;
	};

	Parameter p1;
	Parameter p2;

protected:
	static double SGNS_FASTCALL unitRateFunction( RateFunction *me, Population X );
	static double SGNS_FASTCALL linearRateFunction( RateFunction *me, Population X );
};

class BasicRateFunction : public RateFunction {
public:
	// f(x) = X!/N!(X-N)!
	static RateFunction GilH( int N );

	// f(x) = x^n
	static RateFunction Pow( double n );

	// f(x) = x^n / (an + x^n)
	static RateFunction Hill( double an, double n );

	// f(x) = x^n / (an + x^n)
	static RateFunction Invhill( double an, double n );

	// f(x) = min(a,x)
	static RateFunction Min( double a );

	// f(x) = max(a,x)
	static RateFunction Max( double a );

	// f(x) = v if x < thresh, else 1
	static RateFunction Step( Population thresh, double v );
	// f(x) = 1 if x < thresh, else v
	static RateFunction Step2( Population thresh, double v );

protected:
	static double SGNS_FASTCALL gilhRateFunction( RateFunction *me, Population X );
	static double SGNS_FASTCALL gilh2RateFunction( RateFunction *me, Population X );

	static double SGNS_FASTCALL squareRateFunction( RateFunction *me, Population X );
	static double SGNS_FASTCALL cubeRateFunction( RateFunction *me, Population X );
	static double SGNS_FASTCALL powRateFunction( RateFunction *me, Population X );

	static double SGNS_FASTCALL hill1RateFunction( RateFunction *me, Population X );
	static double SGNS_FASTCALL hill2RateFunction( RateFunction *me, Population X );
	static double SGNS_FASTCALL hillnRateFunction( RateFunction *me, Population X );

	static double SGNS_FASTCALL invhill1RateFunction( RateFunction *me, Population X );
	static double SGNS_FASTCALL invhill2RateFunction( RateFunction *me, Population X );
	static double SGNS_FASTCALL invhillnRateFunction( RateFunction *me, Population X );

	static double SGNS_FASTCALL minRateFunction( RateFunction *me, Population X );
	static double SGNS_FASTCALL maxRateFunction( RateFunction *me, Population X );
	static double SGNS_FASTCALL stepRateFunction( RateFunction *me, Population X );
	static double SGNS_FASTCALL step2RateFunction( RateFunction *me, Population X );
};

} // namespace

#endif
