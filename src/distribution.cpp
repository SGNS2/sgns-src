
// See distribution.h for a description of the contents of this file.

#include "stdafx.h"

#include <algorithm>

#include "rng.h"
#include "distribution.h"

namespace sgns2 {

// ---------------------------------------------------------------------------
void DistributionContext::allocateSplitBuffer( unsigned size ) {
	delete[] splitBuffer;
	splitBuffer = new Population[size];
}

// ---------------------------------------------------------------------------
RuntimeDistribution RuntimeDistribution::DeltaDistribution( double c ) {
	RuntimeDistribution distr;
	distr.distr_sampler = &deltaSampler;
	distr.a1 = c;
	distr.a2 = 0.0;
	return distr;
}

double SGNS_FASTCALL RuntimeDistribution::deltaSampler( RuntimeDistribution *me, const DistributionContext * ) {
	return me->a1;
}

// ---------------------------------------------------------------------------
// ~ U*(x-m)+m, in range [m,x)
RuntimeDistribution BasicRuntimeDistribution::UniformDistribution( double m, double x ) {
	BasicRuntimeDistribution distr;
	distr.distr_sampler = &uniformSampler;
	distr.a1 = m;
	distr.a2 = x;
	return distr;
}

double SGNS_FASTCALL BasicRuntimeDistribution::uniformSampler( RuntimeDistribution *me, const DistributionContext *dc ) {
	BasicRuntimeDistribution *me2 = (BasicRuntimeDistribution*)me;
	return dc->getRNG()->uniform( me2->a1, me2->a2 );
}

// ---------------------------------------------------------------------------
// ~ N(m,s), can be negative - don't use as a time delay
RuntimeDistribution BasicRuntimeDistribution::GaussianDistribution( double m, double s ) {
	BasicRuntimeDistribution distr;
	distr.distr_sampler = &gaussianSampler;
	distr.a1 = m;
	distr.a2 = s;
	return distr;
}

double SGNS_FASTCALL BasicRuntimeDistribution::gaussianSampler( RuntimeDistribution *me, const DistributionContext *dc ) {
	BasicRuntimeDistribution *me2 = (BasicRuntimeDistribution*)me;
	return dc->getRNG()->normal( me2->a1, me2->a2 );
}

// ---------------------------------------------------------------------------
// ~ max(0,N(m,s))
RuntimeDistribution BasicRuntimeDistribution::TruncGaussianDistribution( double m, double s ) {
	BasicRuntimeDistribution distr;
	distr.distr_sampler = &truncGaussianSampler;
	distr.a1 = m;
	distr.a2 = s;
	return distr;
}

double SGNS_FASTCALL BasicRuntimeDistribution::truncGaussianSampler( RuntimeDistribution *me, const DistributionContext *dc ) {
	BasicRuntimeDistribution *me2 = (BasicRuntimeDistribution*)me;
	return std::max( 0.0, dc->getRNG()->normal( me2->a1, me2->a2 ) );
}

// ---------------------------------------------------------------------------
// ~ N(m,s) with the negative tail removed and the rest of the distribution rescaled
RuntimeDistribution BasicRuntimeDistribution::NonNegGaussianDistribution( double m, double s ) {
	BasicRuntimeDistribution distr;
	distr.distr_sampler = &nonNegGaussianSampler;
	distr.a1 = m;
	distr.a2 = s;
	return distr;
}

double SGNS_FASTCALL BasicRuntimeDistribution::nonNegGaussianSampler( RuntimeDistribution *me, const DistributionContext *dc ) {
	BasicRuntimeDistribution *me2 = (BasicRuntimeDistribution*)me;
	double d;
	do {
		d = dc->getRNG()->normal( me2->a1, me2->a2 );
	} while( d < 0.0 );
	return d;
}

// ---------------------------------------------------------------------------
// ~ Exp(lambda)
RuntimeDistribution BasicRuntimeDistribution::ExponentialDistribution( double lambda ) {
	BasicRuntimeDistribution distr;
	distr.distr_sampler = &exponentialSampler;
	distr.a1 = lambda;
	distr.a2 = 0.0;
	return distr;
}

double SGNS_FASTCALL BasicRuntimeDistribution::exponentialSampler( RuntimeDistribution *me, const DistributionContext *dc ) {
	BasicRuntimeDistribution *me2 = (BasicRuntimeDistribution*)me;
	return dc->getRNG()->exponential( me2->a1 );
}

// ---------------------------------------------------------------------------
// ~ Gamma(shape,scale)
RuntimeDistribution BasicRuntimeDistribution::GammaDistribution( double shape, double scale ) {
	BasicRuntimeDistribution distr;
	distr.distr_sampler = &gammaSampler;
	distr.a1 = shape;
	distr.a2 = scale;
	return distr;
}

double SGNS_FASTCALL BasicRuntimeDistribution::gammaSampler( RuntimeDistribution *me, const DistributionContext *dc ) {
	BasicRuntimeDistribution *me2 = (BasicRuntimeDistribution*)me;
	return dc->getRNG()->gamma( me2->a1, me2->a2 );
}

// ---------------------------------------------------------------------------
// ~ Beta(alpha,beta)
RuntimeDistribution BasicRuntimeDistribution::BetaDistribution( double alpha, double beta ) {
	BasicRuntimeDistribution distr;
	distr.distr_sampler = &betaSampler;
	distr.a1 = alpha;
	distr.a2 = beta;
	return distr;
}

double SGNS_FASTCALL BasicRuntimeDistribution::betaSampler( RuntimeDistribution *me, const DistributionContext *dc ) {
	BasicRuntimeDistribution *me2 = (BasicRuntimeDistribution*)me;
	return dc->getRNG()->beta( me2->a1, me2->a2 );
}

} // namespace
