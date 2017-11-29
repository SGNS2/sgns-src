
// See split.h for a description of the contents of this file.

#include "stdafx.h"

#include "split.h"
#include "distribution.h"
#include "rng.h"
#include "simulation.h"

namespace sgns2 {

// ---------------------------------------------------------------------------
// T ~ Bernoulli(p)*N
SplitFunction SplitFunction::AllOrNothing( double p, bool virt){
	SplitFunction split;
	split.splitter = &allOrNothingSplitter;
	split.a1 = p;
	split.a2 = 0.0;
	split.virtuality = virt;
	split.biasness = false;
	return split;
}

void SGNS_FASTCALL SplitFunction::allOrNothingSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx ) {
	if(ctx->getRNG()->uniform() < me->a1 )
		X[1] = X[0] ;
	else
		X[1] = 0;
	if(!me->virtuality)
		X[0] -= X[1];
}

// ---------------------------------------------------------------------------
//  T ~ Bino(N, Beta(a,b))
// Unbiased: Uses Beta(a,b) or Beta(b,a) with equal probability
SplitFunction SplitFunction::BetaBinomialSplit( double a, double b, bool virt, bool unbiased ){
	SplitFunction split;
	split.splitter = &betaPartitionSplitter;
	split.a1 = a;
	split.a2 = b;
	split.virtuality = virt;
	split.biasness = unbiased;
	return split;
}

void SGNS_FASTCALL SplitFunction::betaPartitionSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx ) {
	if( ctx->getRNG()->uniform() < 0.5 )
		X[1] = ctx->getRNG()->binomial( ctx->getRNG()->beta(me->a1,me->a2), (int)X[0] );
	else
		X[1] = ctx->getRNG()->binomial( ctx->getRNG()->beta(me->a2,me->a1), (int)X[0] );
	if( me->biasness && ctx->getRNG()->uniform() < 0.5 )
		X[1] = X[0] - X[1];
	if(!me->virtuality)
		X[0] -= X[1];
}
// ---------------------------------------------------------------------------
// T ~ Bino(N, p), L = N - T
SplitFunction SplitFunction::BinomialSplit( double p, bool virt, bool unbiased ) {
	SplitFunction split;
	split.splitter = &binomialSplitter;
	split.a1 = p;
	split.a2 = 0.0;
	split.virtuality = virt;
	split.biasness = unbiased;
	return split;
}

void SGNS_FASTCALL SplitFunction::binomialSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx ) {
	X[1] = ctx->getRNG()->binomial( me->a1, (int)X[0] );
	if( me->biasness && ctx->getRNG()->uniform() < 0.5 )
		X[1] = X[0] - X[1];
	if(!me->virtuality)
		X[0] -= X[1];
}

SplitFunction SplitFunction::BinomialSplit_P( int split1, int split2, bool virt, bool unbiased ) {
	SplitFunction split;
	split.splitter = &binomialSplitter_P;
	split.a1 = split1 - 1.0;
	split.a2 = split2 - 1.0;
	split.virtuality = virt;
	split.biasness = unbiased;
	return split;
}

void SGNS_FASTCALL SplitFunction::binomialSplitter_P( SplitFunction *me, Population *X, const DistributionContext *ctx ) {
	Population *splits = ctx->getSplitBuffer();
	Population L1 = splits[(int)me->a1];
	Population L2 = splits[(int)me->a2];
	double p;
	if( L1 == 0 && L2 == 0 ) {
		p = 0.5;
	} else {
		p = L1 / (L1 + (double)L2);
	}
	X[1] = ctx->getRNG()->binomial( p, (int)X[0] );
	if( me->biasness && ctx->getRNG()->uniform() < 0.5 )
		X[1] = X[0] - X[1];
	if(!me->virtuality)
		X[0] -= X[1];
}

// ---------------------------------------------------------------------------
// u ~ Bino(floor(N/2), r)    // Pairs to partition as pairs
// v ~ Bino(u, p)             // Pairs that succeeded in partitioning equally
// T ~ Bino(N-2u, 0.5) + 2Bino(u-v,0.5) + v
SplitFunction SplitFunction::PairSplit( double p, double r, bool virt ) {
	SplitFunction split;
	split.splitter = &pairSplitter;
	split.a1 = p;
	split.a2 = r;
	split.virtuality = virt;
	split.biasness = false;
	return split;
}

void SGNS_FASTCALL SplitFunction::pairSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx ) {
	Population u = ctx->getRNG()->binomial( me->a2, (int)(X[0]>>1) );
	Population v = ctx->getRNG()->binomial( me->a1, (int)u );
	X[1] = ctx->getRNG()->binomial( 0.5, (int)(X[0] - (u<<1)) ) + (ctx->getRNG()->binomial( 0.5, (int)(u - v) ) << 1) + v;
	if(!me->virtuality)
		X[0] -= X[1];
}

// ---------------------------------------------------------------------------
// T = floor(N * fraction), L = N - T
SplitFunction SplitFunction::Take( double fraction, bool virt ) {
	SplitFunction split;
	split.splitter = &takeSplitter;
	split.a1 = fraction;
	split.a2 = 0.0;
	split.virtuality = virt;
	split.biasness = false;
	return split;
}

void SGNS_FASTCALL SplitFunction::takeSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx ) {
	(void)ctx;
	X[1] = (Population)floor( X[0] * me->a1 );
	if(!me->virtuality)
		X[0] -= X[1];
}

// ---------------------------------------------------------------------------
// T = floor(N * fraction), L = N - T
SplitFunction SplitFunction::TakeRound( double fraction, bool virt ) {
	SplitFunction split;
	split.splitter = &takeRoundSplitter;
	split.a1 = fraction;
	split.a2 = 0.0;
	split.virtuality = virt;
	split.biasness = false;
	return split;
}

void SGNS_FASTCALL SplitFunction::takeRoundSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx ) {
	(void)ctx;
	X[1] = (Population)floor( X[0] * me->a1 + 0.5 );
	if(!me->virtuality)
		X[0] -= X[1];
}

// ---------------------------------------------------------------------------
// For v>=u: N<=u -> T = 0; u<N<=v -> T = N-u; v<N -> T = v-u
// T = min(max(0,N-u),v-u)
SplitFunction SplitFunction::Range( double u, double v, bool virt  ) {
	SplitFunction split;
	split.splitter = &rangeSplitter;
	split.a1 = u;
	split.a2 = v;
	split.virtuality = virt;
	split.biasness = false;
	return split;
}

void SGNS_FASTCALL SplitFunction::rangeSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx ) {
	(void)ctx;
	if( X[0] <= me->a1 )
		X[1] = 0;
	else if( me->a2 >= X[0] && X[0] > me->a1 )
		X[1] = X[0] - static_cast<Population>(me->a1);
	else if( me->a2 < X[0] )
		X[1] = static_cast<Population>(me->a2 - me->a1);

	if(!me->virtuality)
		X[0] -= X[1];
}

/*
// ---------------------------------------------------------------------------
// T ~ Bino(N, p) or Bino(N, 1-p) with equal probability, L = N - T
SplitFunction SplitFunction::UnbiasedBinomialSplit( double p ) {
	SplitFunction split;
	split.splitter = &unbiasedBinomialSplitter;
	split.a1 = p;
	split.a2 = 0.0;
	return split;
}

void SGNS_FASTCALL SplitFunction::unbiasedBinomialSplitter( SplitFunction *me, Population *X, const DistributionContext *ctx ) {
	X[1] = ctx->getRNG()->binomial( me->a1, (int)X[0] );
	if( ctx->getRNG()->uniform() < 0.5 )
		X[1] = X[0] - X[1];
	X[0] -= X[1];
}
*/
} // namespace sgns2
