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

/* distribution.h/cpp

DistributionContext struct contents:
	- Provides the link between the low-level distributions and the higher-level SimulationInstance
	- Provides the distribution samplers with access to the simulation's random number generator

RuntimeDistribution class contents:
	- Stores a distribution function which can be called to sample the distribution
	- Stores two flexible parameters for use by the distribution function
	- Provides distribution functions for the Delta distribution

BasicRuntimeDistribution class contents:
	- Samplers for some more complex distributions:
	    Uniform, Normal, Gamma, Beta
*/


#ifndef RUNTIMEDISTRIBUTION_H
#define RUNTIMEDISTRIBUTION_H

#include <iostream>

#include "rng.h"
#include "simtypes.h"

namespace sgns2 {

class SimulationInstance;

class DistributionContext {
public:
	DistributionContext( SimulationInstance *newSim, unsigned seed )
		: sim(newSim), splitBuffer(NULL), rng(seed) { }
	~DistributionContext() {
		delete splitBuffer;
	}

	void allocateSplitBuffer( unsigned size );

	inline SimulationInstance *getSimulation() const throw() { return sim; }
	inline Population *getSplitBuffer() const throw() { return splitBuffer; }
	inline RNG::RNG *getRNG() const throw() { return &rng; }

private:
	SimulationInstance *sim;
	Population *splitBuffer;
	mutable RNG::RNG rng;
};

class RuntimeDistribution {
public:
	RuntimeDistribution( const RuntimeDistribution &other )
		: distr_sampler(other.distr_sampler)
		, a1(other.a1), a2(other.a2)
	{ }

	RuntimeDistribution &operator =( const RuntimeDistribution &other ) {
		distr_sampler = other.distr_sampler;
		a1 = other.a1;
		a2 = other.a2;
		return *this;
	}

	inline ~RuntimeDistribution() { }

	// Indirection is done with a function pointer directly to remove
	// one unnecessary level of indirection
	typedef double (SGNS_FASTCALL *Sampler)( RuntimeDistribution *me, const DistributionContext *dc );

	inline double sample( const DistributionContext *dc ) {
		return distr_sampler( this, dc );
	}

	// Special cases for optimization
	inline bool isConstant() { return distr_sampler == &deltaSampler; }
	inline bool isZero() { return isConstant() && a1 == 0.0; }

	// Subclasses should store parameters in data, a1 and a2 so that
	// the class can be passed around easily

	static RuntimeDistribution DeltaDistribution( double c );

protected:
	inline RuntimeDistribution() { }

	static double SGNS_FASTCALL deltaSampler( RuntimeDistribution *me, const DistributionContext * );

	Sampler distr_sampler;
	double a1;
	double a2;
};

class BasicRuntimeDistribution : public RuntimeDistribution {
public:
	// ~ U*(x-m)+m, in range [m,x)
	static RuntimeDistribution UniformDistribution( double m, double x );

	// ~ N(m,s), can be negative - don't use as a time delay
	static RuntimeDistribution GaussianDistribution( double m, double s );

	// ~ max(0,N(m,s))
	static RuntimeDistribution TruncGaussianDistribution( double m, double s );

	// ~ N(m,s) with the negative tail removed and the rest of the distribution rescaled
	static RuntimeDistribution NonNegGaussianDistribution( double m, double s );

	// ~ Exp(lambda)
	static RuntimeDistribution ExponentialDistribution( double lambda );

	// ~ Gamma(shape,scale)
	static RuntimeDistribution GammaDistribution( double shape, double scale );

	// ~ Beta(alpha,beta)
	static RuntimeDistribution BetaDistribution( double alpha, double beta );

protected:
	// Samplers
	static double SGNS_FASTCALL uniformSampler( RuntimeDistribution *me, const DistributionContext *dc );
	static double SGNS_FASTCALL gaussianSampler( RuntimeDistribution *me, const DistributionContext *dc );
	static double SGNS_FASTCALL truncGaussianSampler( RuntimeDistribution *me, const DistributionContext *dc );
	static double SGNS_FASTCALL nonNegGaussianSampler( RuntimeDistribution *me, const DistributionContext *dc );
	static double SGNS_FASTCALL exponentialSampler( RuntimeDistribution *me, const DistributionContext *dc );
	static double SGNS_FASTCALL gammaSampler( RuntimeDistribution *me, const DistributionContext *dc );
	static double SGNS_FASTCALL betaSampler( RuntimeDistribution *me, const DistributionContext *dc );
};

} // namespace

#endif // RUNTIMEDISTRIBUTION_H
