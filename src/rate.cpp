
// See rate.h for a description of the contents of this file.

#include "stdafx.h"

#include <cmath>
#include <new>
#include <float.h>
#include <algorithm>
#include <cassert>

#include "rate.h"

namespace sgns2 {

// ---------------------------------------------------------------------------
RateFunction::RateFunction()
: fn( unitRateFunction )
{
}

// ---------------------------------------------------------------------------
RateFunction::~RateFunction() {
}

// ---------------------------------------------------------------------------
// f(x) = 1
RateFunction RateFunction::Unit() {
	RateFunction f;
	f.fn = &unitRateFunction;
	return f;
}

double SGNS_FASTCALL RateFunction::unitRateFunction( RateFunction*, Population ) {
	return 1.0;
}

// ---------------------------------------------------------------------------
// f(x) = x
RateFunction RateFunction::Linear() {
	RateFunction f;
	f.fn = &linearRateFunction;
	return f;
}

double SGNS_FASTCALL RateFunction::linearRateFunction( RateFunction*, Population X ) {
	return (double)X;
}

// ---------------------------------------------------------------------------
// f(x) = X!/N!(X-N)!
RateFunction BasicRateFunction::GilH( int N ) {
	if( N == 0 ) {
		return RateFunction::Unit();
	} else if( N == 1 ) {
		return RateFunction::Linear();
	}
	
	BasicRateFunction f;
	f.fn = N == 2 ? gilh2RateFunction : &gilhRateFunction;
	f.p1.i = N;
	return f;
}

double SGNS_FASTCALL BasicRateFunction::gilhRateFunction( RateFunction *me, Population X ) {
	// General
	BasicRateFunction *me2 = (BasicRateFunction*)me;
	double x = (double)X;
	double v = x;
	for( int i = 1; i < me2->p1.i; i++ )
		v *= (x - i) / (i + 1);
	return v;
}

double SGNS_FASTCALL BasicRateFunction::gilh2RateFunction( RateFunction*, Population X ) {
	// Special case, N = 2
	// fabs so that we do not return -0, resulting in a next firing time of -INF
	return fabs( (double)X * (double)(X-1) / 2.0 );
}

// ---------------------------------------------------------------------------
// f(x) = x^n
RateFunction BasicRateFunction::Pow( double n ) {
	if( fabs(n) < 0.00001 ) {
		return RateFunction::Unit();
	} else if( fabs(n-1.0) < 0.00001 ) {
		return RateFunction::Linear();
	}

	BasicRateFunction f;
	f.fn = &powRateFunction;
	f.p1.d = n;
	if( fabs(n-2) < 0.00001 ) {
		f.fn = &squareRateFunction;
	} else if( fabs(n-3) < 0.00001 ) {
		f.fn = &cubeRateFunction;
	}
	return f;

}

double SGNS_FASTCALL BasicRateFunction::squareRateFunction( RateFunction*, Population X ) {
	double x = (double)X;
	return x*x;
}

double SGNS_FASTCALL BasicRateFunction::cubeRateFunction( RateFunction*, Population X ) {
	double x = (double)X;
	return x*x*x;
}

double SGNS_FASTCALL BasicRateFunction::powRateFunction( RateFunction *me, Population X ) {
	BasicRateFunction *me2 = (BasicRateFunction*)me;
	return pow( (double)X, me2->p1.d );
}

// ---------------------------------------------------------------------------
// f(x) = x^n / (an + x^n)
RateFunction BasicRateFunction::Hill( double an, double n ) {
	BasicRateFunction f;
	f.fn = &hillnRateFunction;
	f.p1.d = an;
	f.p2.d = n;
	if( fabs(n-1) < 0.00001 ) {
		f.fn = &hill1RateFunction;
	} else if( fabs(n-2) < 0.00001 ) {
		f.fn = &hill2RateFunction;
	}
	return f;
}

double SGNS_FASTCALL BasicRateFunction::hill1RateFunction( RateFunction *me, Population X ) {
	BasicRateFunction *me2 = (BasicRateFunction*)me;
	double x = (double)X;
	return x / (x + me2->p1.d);
}

double SGNS_FASTCALL BasicRateFunction::hill2RateFunction( RateFunction *me, Population X ) {
	BasicRateFunction *me2 = (BasicRateFunction*)me;
	double x = (double)X;
	return x*x / (x*x + me2->p1.d);
}

double SGNS_FASTCALL BasicRateFunction::hillnRateFunction( RateFunction *me, Population X ) {
	BasicRateFunction *me2 = (BasicRateFunction*)me;
	double x = (double)X;
	double xn = pow( x, me2->p2.d );
	return xn / (xn + me2->p1.d);
}

// ---------------------------------------------------------------------------
// f(x) = x^n / (an + x^n)
RateFunction BasicRateFunction::Invhill( double an, double n ) {
	BasicRateFunction f;
	f.fn = &invhillnRateFunction;
	f.p1.d = an;
	f.p2.d = n;
	if( fabs(n-1) < 0.00001 ) {
		f.fn = &invhill1RateFunction;
	} else if( fabs(n-2) < 0.00001 ) {
		f.fn = &invhill2RateFunction;
	}
	return f;
}

double SGNS_FASTCALL BasicRateFunction::invhill1RateFunction( RateFunction *me, Population X ) {
	BasicRateFunction *me2 = (BasicRateFunction*)me;
	double x = (double)X;
	return me2->p1.d / (x + me2->p1.d);
}

double SGNS_FASTCALL BasicRateFunction::invhill2RateFunction( RateFunction *me, Population X ) {
	BasicRateFunction *me2 = (BasicRateFunction*)me;
	double x = (double)X;
	return me2->p1.d / (x*x + me2->p1.d);
}

double SGNS_FASTCALL BasicRateFunction::invhillnRateFunction( RateFunction *me, Population X ) {
	BasicRateFunction *me2 = (BasicRateFunction*)me;
	double x = (double)X;
	double xn = pow( x, me2->p2.d );
	return me2->p1.d / (xn + me2->p1.d);
}

// ---------------------------------------------------------------------------
// f(x) = min(a,x)
RateFunction BasicRateFunction::Min( double a ) {
	BasicRateFunction f;
	f.fn = &minRateFunction;
	f.p1.d = a;
	return f;
}

double SGNS_FASTCALL BasicRateFunction::minRateFunction( RateFunction *me, Population X ) {
	BasicRateFunction *me2 = (BasicRateFunction*)me;
	return std::min( me2->p1.d, (double)X );
}

// ---------------------------------------------------------------------------
// f(x) = max(a,x)
RateFunction BasicRateFunction::Max( double a ) {
	BasicRateFunction f;
	f.fn = &maxRateFunction;
	f.p1.d = a;
	return f;
}

double SGNS_FASTCALL BasicRateFunction::maxRateFunction( RateFunction *me, Population X ) {
	BasicRateFunction *me2 = (BasicRateFunction*)me;
	return std::max( me2->p1.d, (double)X );
}

// ---------------------------------------------------------------------------
// f(x) = v if x < thresh, else 1
RateFunction BasicRateFunction::Step( Population thresh, double v ) {
	BasicRateFunction f;
	f.fn = &stepRateFunction;
	f.p1.pop = thresh;
	f.p2.d = v;
	return f;
}

double SGNS_FASTCALL BasicRateFunction::stepRateFunction( RateFunction *me, Population X ) {
	BasicRateFunction *me2 = (BasicRateFunction*)me;
	return X < me2->p1.pop ? me2->p2.d : 1.0;
}

// ---------------------------------------------------------------------------
// f(x) = 1 if x < thresh, else v
RateFunction BasicRateFunction::Step2( Population thresh, double v ) {
	BasicRateFunction f;
	f.fn = &step2RateFunction;
	f.p1.pop = thresh;
	f.p2.d = v;
	return f;
}

double SGNS_FASTCALL BasicRateFunction::step2RateFunction( RateFunction *me, Population X ) {
	BasicRateFunction *me2 = (BasicRateFunction*)me;
	return X < me2->p1.pop ? 1.0 : me2->p2.d;
}

} // namespace
