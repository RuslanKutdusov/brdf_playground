#pragma once

/**
 * \brief Computes the integral of a one-dimensional function
 * using adaptive Gauss-Lobatto quadrature.
 *
 * Given a target error \f$ \epsilon \f$, the integral of
 * a function \f$ f \f$ between \f$ a \f$ and \f$ b \f$ is
 * calculated by means of the Gauss-Lobatto formula.
 *
 * References:
 * This algorithm is a C++ implementation of the algorithm outlined in
 *
 * W. Gander and W. Gautschi, Adaptive Quadrature - Revisited.
 * BIT, 40(1):84-101, March 2000. CS technical report:
 * ftp.inf.ethz.ch/pub/publications/tech-reports/3xx/306.ps.gz
 *
 * The original MATLAB version can be downloaded here
 * http://www.inf.ethz.ch/personal/gander/adaptlob.m
 *
 * This particular implementation is based on code in QuantLib,
 * a free-software/open-source library for financial quantitative
 * analysts and developers - http://quantlib.org/
 *
 */
class GaussLobattoIntegrator
{
public:
	/**
	 * Initialize a Gauss-Lobatto integration scheme
	 *
	 * \param maxEvals Maximum number of function evaluations. The
	 *    integrator will print a warning when this limit is
	 *    exceeded. It will then stop the recursion, but a few
	 *    further evaluations may still take place. Hence the limit
	 *    is not a strict one.
	 *
	 * \param absError Absolute error requirement (0 to disable)
	 * \param relError Relative error requirement (0 to disable)
	 *
	 * \param useConvergenceEstimate Estimate the convergence behavior
	 *     of the GL-quadrature by comparing the 4, 7 and 13-point
	 *     variants and increase the absolute tolerance accordingly.
	 *
	 * \param warn Should the integrator warn when the number of
	 *     function evaluations is exceeded?
	 */
	GaussLobattoIntegrator(size_t maxEvals, float absError = 0, float relError = 0, bool useConvergenceEstimate = true, bool warn = true);

	/**
	 * \brief Integrate the function \c f from \c a to \c b.
	 *
	 * Also returns the total number of evaluations if requested
	 */
	float integrate(const std::function<float(float)>& f, float a, float b, size_t* evals = nullptr) const;

protected:
	/**
	 * \brief Perform one step of the 4-point Gauss-Lobatto rule, then
	 * compute the same integral using a 7-point Kronrod extension and
	 * compare. If the accuracy is deemed too low, recurse.
	 *
	 * \param f Function to integrate
	 * \param a Lower integration limit
	 * \param b Upper integration limit
	 * \param fa Function evaluated at the lower limit
	 * \param fb Function evaluated at the upper limit
	 * \param is Absolute tolerance in epsilons
	 */
	float adaptiveGaussLobattoStep(const std::function<float(float)>& f, float a, float b, float fa, float fb, float is, size_t& evals) const;

	/**
	 * Compute the absolute error tolerance using a 13-point
	 * Gauss-Lobatto rule.
	 */
	float calculateAbsTolerance(const std::function<float(float)>& f, float a, float b, size_t& evals) const;

protected:
	float m_absError, m_relError;
	size_t m_maxEvals;
	bool m_useConvergenceEstimate;
	bool m_warn;
	static const float m_alpha;
	static const float m_beta;
	static const float m_x1;
	static const float m_x2;
	static const float m_x3;
};
