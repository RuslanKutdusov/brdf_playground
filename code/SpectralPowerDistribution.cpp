#include "Precompiled.h"
#include "SpectralPowerDistribution.h"
#include "GaussLobattoIntegrator.h"
#include <iostream>
#include <sstream>
#include <fstream>


static std::string TrimString(const std::string& str)
{
	std::string::size_type start = str.find_first_not_of(" \t\r\n"), end = str.find_last_not_of(" \t\r\n");
	return str.substr(start == std::string::npos ? 0 : start, end == std::string::npos ? str.length() - 1 : end - start + 1);
}


inline float lerp(float t, float v1, float v2)
{
	return (1.0f - t) * v1 + t * v2;
}


SpectralPowerDistribution::SpectralPowerDistribution(const float* wavelength, const float* values, uint32_t entriesNum)
{
	m_values.resize(entriesNum);
	for (uint32_t i = 0; i < entriesNum; i++)
		m_values[i] = {wavelength[i], values[i]};
}


bool SpectralPowerDistribution::InitFromFile(const char* filename)
{
	FilePath fullPath = "data\\SPDs";
	fullPath /= filename;

	std::ifstream fileStream(fullPath.c_str());
	if (fileStream.bad() || fileStream.fail())
		return false;

	std::string line;
	while (true)
	{
		if (!std::getline(fileStream, line))
			break;
		line = TrimString(line);
		if (line.length() == 0 || line[0] == '#')
			continue;
		std::istringstream iss(line);
		float lambda, value;
		if (!(iss >> lambda >> value))
			break;
		m_values.push_back({lambda, value});
	}

	return true;
}


float SpectralPowerDistribution::Eval(float lambda) const
{
	if (m_values.size() < 2 || lambda < m_values[0].wavelength || lambda > m_values.back().wavelength)
		return 0.0f;

	/* Find the associated table entries using binary search */
	Value v = {lambda, 0.0f};
	auto result = std::equal_range(m_values.begin(), m_values.end(), v);

	size_t idx1 = result.first - m_values.begin();
	size_t idx2 = result.second - m_values.begin();
	if (idx1 == idx2)
	{
		const Value& a = m_values[idx1 - 1];
		const Value& b = m_values[idx1];
		return lerp((lambda - a.wavelength) / (b.wavelength - a.wavelength), b.value, a.value);
	}
	else if (idx2 == idx1 + 1)
	{
		/* Hit a value exactly */
		return m_values[idx1].value;
	}
	else
	{
		Assert(false);
		return 0.0f;
	}
}


float SpectralPowerDistribution::Average(float lambdaMin, float lambdaMax) const
{
#if 0
	const float kEpsilon = 1e-04f;
	GaussLobattoIntegrator integrator(10000, kEpsilon, kEpsilon, false, false);

	if (lambdaMax <= lambdaMin)
		return 0.0f;

	float integral = 0;

	/// Integrate over 50nm-sized regions
	size_t nSteps = std::max((size_t)1, (size_t)std::ceil((lambdaMax - lambdaMin) / 50));
	float stepSize = (lambdaMax - lambdaMin) / nSteps, pos = lambdaMin;

	for (size_t i = 0; i < nSteps; ++i)
	{
		integral += integrator.integrate([this](float x) { return Eval(x); }, pos, pos + stepSize);
		pos += stepSize;
	}

	return integral / (lambdaMax - lambdaMin);
#else
	if (m_values.size() < 2)
	    return 0.0f;

	float rangeStart = std::max(lambdaMin, m_values[0].wavelength);
	float rangeEnd = std::min(lambdaMax, m_values.back().wavelength);

	if (rangeEnd <= rangeStart)
	    return 0.0f;

	/* Find the starting index using binary search */
	Value rangeStartVal{rangeStart, 0.0f};
	size_t entry = std::lower_bound(m_values.begin(), m_values.end(), rangeStartVal) - m_values.begin();
	entry = std::max(entry, (size_t)1) - 1;

	float result = 0.0f;
	for (; entry + 1 < m_values.size() && rangeEnd >= m_values[entry].wavelength; ++entry)
	{
	    /* Step through the samples and integrate trapezoids */
	    const Value& a = m_values[entry];
	    float ca = std::max(a.wavelength, rangeStart);
	    const Value& b = m_values[entry + 1];
	    float cb = std::min(b.wavelength, rangeEnd);
	    float invAB = 1.0f / (b.wavelength - a.wavelength);

	    if (cb <= ca)
	        continue;

	    float interpA = lerp((ca - a.wavelength) * invAB, a.value, b.value);
	    float interpB = lerp((cb - a.wavelength) * invAB, a.value, b.value);

	    result += 0.5f * (interpA + interpB) * (cb - ca);
	}

	return result / (lambdaMax - lambdaMin);
#endif
}


Spectrum::Spectrum()
{
	float stepSize = kSpectrumRange / (float)(kSpectrumSamples - 1);
	for (uint32_t i = 0; i <= kSpectrumSamples; i++)
		m_wavelength[i] = kSpectrumMinWavelength + stepSize * i;
}


Spectrum::Spectrum(const SpectralPowerDistribution& spd) : Spectrum()
{
	for (uint32_t i = 0; i < kSpectrumSamples; i++)
		m_values[i] = spd.Average(m_wavelength[i], m_wavelength[i + 1]);
}


float Spectrum::Eval(float wavelength) const
{
	float stepSize = kSpectrumRange / (float)(kSpectrumSamples - 1);
	int index = (int)((wavelength - kSpectrumMinWavelength) / stepSize);
	if (index < 0 || index >= sizeof(m_values))
		return 0.0f;
	else
		return m_values[index];
}


void Spectrum::ToXYZ(float& x, float& y, float& z, const Spectrum& CIE_X, const Spectrum& CIE_Y, const Spectrum& CIE_Z, float CIE_normalization)
{
	x = y = z = 0.0f;
	for (uint32_t i = 0; i < kSpectrumSamples; ++i)
	{
		x += CIE_X[i] * m_values[i];
		y += CIE_Y[i] * m_values[i];
		z += CIE_Z[i] * m_values[i];
	}
	x *= CIE_normalization;
	y *= CIE_normalization;
	z *= CIE_normalization;
}


void Spectrum::ToLinearRGB(float& r, float& g, float& b, const Spectrum& CIE_X, const Spectrum& CIE_Y, const Spectrum& CIE_Z, float CIE_normalization)
{
	float x, y, z;
	ToXYZ(x, y, z, CIE_X, CIE_Y, CIE_Z, CIE_normalization);
	/* Convert from XYZ tristimulus values to ITU-R Rec. BT.709 linear RGB */
	r = 3.240479f * x + -1.537150f * y + -0.498535f * z;
	g = -0.969256f * x + 1.875991f * y + 0.041556f * z;
	b = 0.055648f * x + -0.204043f * y + 1.057311f * z;
}