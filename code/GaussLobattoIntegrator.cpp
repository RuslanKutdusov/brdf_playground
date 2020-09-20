#include "Precompiled.h"
#include "GaussLobattoIntegrator.h"

/*!
 \brief integral of a one-dimensional function using an adaptive
 Gauss-Lobatto integral

 Copyright (C) 2008 Klaus Spanderen

 This code is based on code in QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the license for more details.
*/

const float GaussLobattoIntegrator::m_alpha = (float)std::sqrt(2.0 / 3.0);
const float GaussLobattoIntegrator::m_beta = (float)(1.0 / std::sqrt(5.0));
const float GaussLobattoIntegrator::m_x1 = (float)0.94288241569547971906;
const float GaussLobattoIntegrator::m_x2 = (float)0.64185334234578130578;
const float GaussLobattoIntegrator::m_x3 = (float)0.23638319966214988028;


GaussLobattoIntegrator::GaussLobattoIntegrator(size_t maxEvals, float absError, float relError, bool useConvergenceEstimate, bool warn)
    : m_absError(absError), m_relError(relError), m_maxEvals(maxEvals), m_useConvergenceEstimate(useConvergenceEstimate), m_warn(warn)
{
	Assert(m_absError != 0 || m_relError != 0);
}


float GaussLobattoIntegrator::integrate(const std::function<float(float)>& f, float a, float b, size_t* _evals) const
{
	float factor = 1;
	size_t evals = 0;
	if (a == b)
	{
		return 0;
	}
	else if (b < a)
	{
		std::swap(a, b);
		factor = -1;
	}
	const float absTolerance = calculateAbsTolerance(f, a, b, evals);
	evals += 2;
	float result = factor * adaptiveGaussLobattoStep(f, a, b, f(a), f(b), absTolerance, evals);
	if (evals >= m_maxEvals && m_warn)
		LogStdOut("GaussLobattoIntegrator: Maximum number of evaluations reached!");
	if (_evals)
		*_evals = evals;
	return result;
}


float GaussLobattoIntegrator::calculateAbsTolerance(const std::function<float(float)>& f, float a, float b, size_t& evals) const
{
	const float m = (a + b) / 2;
	const float h = (b - a) / 2;
	const float y1 = f(a);
	const float y3 = f(m - m_alpha * h);
	const float y5 = f(m - m_beta * h);
	const float y7 = f(m);
	const float y9 = f(m + m_beta * h);
	const float y11 = f(m + m_alpha * h);
	const float y13 = f(b);

	float acc =
	    h * ((float)0.0158271919734801831 * (y1 + y13) + (float)0.0942738402188500455 * (f(m - m_x1 * h) + f(m + m_x1 * h)) +
	         (float)0.1550719873365853963 * (y3 + y11) + (float)0.1888215739601824544 * (f(m - m_x2 * h) + f(m + m_x2 * h)) +
	         (float)0.1997734052268585268 * (y5 + y9) + (float)0.2249264653333395270 * (f(m - m_x3 * h) + f(m + m_x3 * h)) + (float)0.2426110719014077338 * y7);
	evals += 13;

	float r = 1.0;
	if (m_useConvergenceEstimate)
	{
		const float integral2 = (h / 6) * (y1 + y13 + 5 * (y5 + y9));
		const float integral1 = (h / 1470) * (77 * (y1 + y13) + 432 * (y3 + y11) + 625 * (y5 + y9) + 672 * y7);

		if (std::abs(integral2 - acc) != 0.0)
			r = std::abs(integral1 - acc) / std::abs(integral2 - acc);
		if (r == 0.0 || r > 1.0)
			r = 1.0;
	}
	float result = std::numeric_limits<float>::infinity();

	if (m_relError != 0 && acc != 0)
		result = acc * std::max(m_relError, std::numeric_limits<float>::epsilon()) / (r * std::numeric_limits<float>::epsilon());

	if (m_absError != 0)
		result = std::min(result, m_absError / (r * std::numeric_limits<float>::epsilon()));

	return result;
}


float GaussLobattoIntegrator::adaptiveGaussLobattoStep(const std::function<float(float)>& f, float a, float b, float fa, float fb, float acc, size_t& evals)
    const
{
	const float h = (b - a) / 2;
	const float m = (a + b) / 2;

	const float mll = m - m_alpha * h;
	const float ml = m - m_beta * h;
	const float mr = m + m_beta * h;
	const float mrr = m + m_alpha * h;

	const float fmll = f(mll);
	const float fml = f(ml);
	const float fm = f(m);
	const float fmr = f(mr);
	const float fmrr = f(mrr);

	const float integral2 = (h / 6) * (fa + fb + 5 * (fml + fmr));
	const float integral1 = (h / 1470) * (77 * (fa + fb) + 432 * (fmll + fmrr) + 625 * (fml + fmr) + 672 * fm);

	evals += 5;

	if (evals >= m_maxEvals)
		return integral1;

	float dist = acc + (integral1 - integral2);
	if (dist == acc || mll <= a || b <= mrr)
	{
		return integral1;
	}
	else
	{
		return adaptiveGaussLobattoStep(f, a, mll, fa, fmll, acc, evals) + adaptiveGaussLobattoStep(f, mll, ml, fmll, fml, acc, evals) +
		       adaptiveGaussLobattoStep(f, ml, m, fml, fm, acc, evals) + adaptiveGaussLobattoStep(f, m, mr, fm, fmr, acc, evals) +
		       adaptiveGaussLobattoStep(f, mr, mrr, fmr, fmrr, acc, evals) + adaptiveGaussLobattoStep(f, mrr, b, fmrr, fb, acc, evals);
	}
}