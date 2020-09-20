#pragma once
#include <vector>
#include "CIE.h"

static const float kSpectrumMinWavelength = 360.0f;
static const float kSpectrumMaxWavelength = 830.0f;
static const float kSpectrumRange = kSpectrumMaxWavelength - kSpectrumMinWavelength;
static const uint32_t kSpectrumSamples = (uint32_t) kSpectrumRange + 1;


class SpectralPowerDistribution
{
public:
	struct Value
	{
		float wavelength;
		float value;

		bool operator<(const Value& v) const
		{
			return wavelength < v.wavelength;
		}
	};

	SpectralPowerDistribution() = default;
	SpectralPowerDistribution(const float* wavelength, const float* values, uint32_t entriesNum);

	bool InitFromFile(const char* filename);

	size_t Size() const
	{
		return m_values.size();
	}

	Value& operator[](uint32_t i)
	{
		return m_values[i];
	}

	const Value& operator[](uint32_t i) const
	{
		return m_values[i];
	}

	/**
	 * \brief Return the value of the spectral power distribution
	 * at the given wavelength.
	 */
	float Eval(float lambda) const;

	/**
	 * \brief Integrate the spectral power distribution
	 * over a given interval and return the average value
	 *
	 * This method overrides the implementation in
	 * \ref ContinousSpectrum, since the integral can be
	 * analytically computed for linearly interpolated spectra.
	 *
	 * \param lambdaMin
	 *     The lower interval bound in nanometers
	 *
	 * \param lambdaMax
	 *     The upper interval bound in nanometers
	 *
	 * \remark If \c lambdaMin >= \c lambdaMax, the
	 *     implementation will return zero.
	 */
	float Average(float lambdaMin, float lambdaMax) const;

private:
	std::vector<Value> m_values;
};


static const SpectralPowerDistribution kCIE_X_SPD(CIE_wavelengths, CIE_X_entries, kCIESamplesNum);
static const SpectralPowerDistribution kCIE_Y_SPD(CIE_wavelengths, CIE_Y_entries, kCIESamplesNum);
static const SpectralPowerDistribution kCIE_Z_SPD(CIE_wavelengths, CIE_Z_entries, kCIESamplesNum);


class Spectrum
{
public:
	Spectrum();
	Spectrum(const SpectralPowerDistribution& spd);

	uint32_t Size() const;
	float GetWavelength(uint32_t i) const;
	float Eval(float wavelength) const;

	float operator[](uint32_t i) const;
	float& operator[](uint32_t i);

	Spectrum& operator*(const float x);

	void ToXYZ(float& x,
	           float& y,
	           float& z,
	           const Spectrum& CIE_X,
	           const Spectrum& CIE_Y,
	           const Spectrum& CIE_Z,
	           float CIE_normalization);
	void ToLinearRGB(float& x,
	                 float& y,
	                 float& z,
	                 const Spectrum& CIE_X,
	                 const Spectrum& CIE_Y,
	                 const Spectrum& CIE_Z,
	                 float CIE_normalization);

private:
	float m_wavelength[kSpectrumSamples + 1] = {};
	float m_values[kSpectrumSamples] = {};
};


inline uint32_t Spectrum::Size() const
{
	return kSpectrumSamples;
}


inline float Spectrum::GetWavelength(uint32_t i) const
{
	return m_wavelength[i];
}


inline float& Spectrum::operator[](uint32_t i)
{
	return m_values[i];
}


inline float Spectrum::operator[](uint32_t i) const
{
	return m_values[i];
}


inline Spectrum& Spectrum::operator*(const float x)
{
	for (float& val : m_values)
		val *= x;
	return *this;
}