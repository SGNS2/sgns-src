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

/* split.h/cpp

SplitFunction class contents:
	- Stores a particular split function and its parameters
	- Behaves similar to RateFunction and RuntimeDistribution
*/

#ifndef SPLIT_H
#define SPLIT_H

#include <iostream>

#include "rng.h"
#include "distribution.h" // For DistributionContext
#include "simtypes.h"

namespace sgns2 {

class SimulationInstance;

class SplitFunction {
public:
	SplitFunction()
		: splitter(&allOrNothingSplitter)
	{ }
	SplitFunction( const SplitFunction &other )
		: a1(other.a1), a2(other.a2), splitter(other.splitter)
		, virtuality(other.virtuality), biasness(other.biasness)
	{ }

	inline SplitFunction &operator =( const SplitFunction &other ) {
		splitter = other.splitter;
		a1 = other.a1;
		a2 = other.a2;
		virtuality = other.virtuality;
		biasness = other.biasness;
		return *this;
	}

	inline ~SplitFunction() { }

	// Indirection is done with a function pointer directly to remove
	// one unnecessary level of indirection
	typedef void (SGNS_FASTCALL *Splitter)( SplitFunction *me, Population *X, const DistributionContext *ctx );

	// X should be a two-element vector with the population to split
	inline void split( Population *X, const DistributionContext *ctx ) {
		return splitter( this, X, ctx );
	}

	// Subclasses should store parameters in a1 and a2 so that
	// the class can be passed around easily

	// N is the incoming population, T is the amount taken:

	// T ~ Bernoulli(p)*N
	static SplitFunction AllOrNothing( double p, bool virt );
	// T ~ Bino(N, Bino(M,p)/M)
	// Unbiased: Uses p or 1-p with equal probability
	//static SplitFunction ClusterSplit( double M, double p, bool virt, bool unbiased ); // NYI
	// T ~ Bino(N, Beta(a,b))
	// Unbiased: Uses Beta(a,b) or Beta(b,a) with equal probability
	static SplitFunction BetaBinomialSplit( double a, double b, bool virt, bool unbiased );
	// T ~ Bino(N, p)
	// Unbiased: Uses p 1-p with equal probability
	static SplitFunction BinomialSplit( double p, bool virt, bool unbiased );
	static SplitFunction BinomialSplit_P( int split1, int split2, bool virt, bool unbiased );
	// N molecules bind to L sites in this compartment and R in the other
	// Remaining molecules are binomially partitioned
	// T ~ Hypergeo(L+R,L,min(L+R,N)) + Bino(max(0,N-L-R),0.5)
	//static SplitFunction BindSplit( double L, double R, bool virt );
	// u ~ Bino(floor(N/2), r)    // Pairs to partition as pairs
	// v ~ Bino(u, p)             // Pairs that succeeded in partitioning equally
	// T ~ Bino(N-2u, 0.5) + 2Bino(u-v,0.5) + v
	static SplitFunction PairSplit( double p, double r, bool virt );
	// T = floor(N * fraction)
	static SplitFunction Take( double fraction, bool virt );
	static SplitFunction TakeRound( double fraction, bool virt );
	// For v>=u: N<=u -> T = 0; u<N<=v -> T = N-u; v<N -> T = v-u
	// T = min(max(0,N-u),v-u)
	static SplitFunction Range( double u, double v, bool virt );

protected:
	//static void SGNS_FASTCALL straightCopySplitter( SplitFunction *me, Population *X, const DistributionContext *ctx );
	//static void SGNS_FASTCALL copySplitter( SplitFunction *me, Population *X, const DistributionContext *ctx );

	static void SGNS_FASTCALL allOrNothingSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx );
	//static void SGNS_FASTCALL allOrNothingSplitter_virt( SplitFunction *me, Population *X, const DistributionContext *ctx );

	static void SGNS_FASTCALL clusterSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx );
	static void SGNS_FASTCALL clusterSplitter_virt( SplitFunction *me, Population *X, const DistributionContext *ctx );
	static void SGNS_FASTCALL unbiasedClusterSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx );
	static void SGNS_FASTCALL unbiasedClusterSplitter_virt( SplitFunction *me, Population *X, const DistributionContext *ctx );

	static void SGNS_FASTCALL betaPartitionSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx );
	
	static void SGNS_FASTCALL binomialSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx );
	static void SGNS_FASTCALL binomialSplitter_P( SplitFunction *me, Population *X, const DistributionContext *ctx );
	
	static void SGNS_FASTCALL pairSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx );

	static void SGNS_FASTCALL takeSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx );
	static void SGNS_FASTCALL takeRoundSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx );
	
	static void SGNS_FASTCALL rangeSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx );

	double a1;
	double a2;
	Splitter splitter;
	bool virtuality;
	bool biasness;
};

} // namespace

#endif // SPLIT_H

//copy:L1 + take:L2 + split_P(1,2):Protein

