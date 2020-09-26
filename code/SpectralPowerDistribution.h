#pragma once
#include <vector>

static const float kSpectrumMinWavelength = 360.0f;
static const float kSpectrumMaxWavelength = 830.0f;
static const float kSpectrumRange = kSpectrumMaxWavelength - kSpectrumMinWavelength;
static const uint32_t kSpectrumSamples = 50;


class SpectralPowerDistribution
{
public:
	SpectralPowerDistribution() = default;
	SpectralPowerDistribution(const float* wavelength, const float* values, uint32_t entriesNum);

	bool InitFromFile(const char* filename);

	const float* Wavelength() const
	{
		return m_wavelengths.data();
	}

	const float* Values() const
	{
		return m_values.data();
	}

	size_t Size() const
	{
		return m_values.size();
	}

	float& operator[](uint32_t i)
	{
		return m_values[i];
	}

	const float& operator[](uint32_t i) const
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
	std::vector<float> m_wavelengths;
	std::vector<float> m_values;
};


class Spectrum
{
public:
	Spectrum() = default;
	Spectrum(const float* wavelength, const float* values, uint32_t entriesNum);
	Spectrum(const SpectralPowerDistribution& spd);
	Spectrum(float v);

	uint32_t Size() const;

	float operator[](uint32_t i) const;
	float& operator[](uint32_t i);

	Spectrum operator+(float x) const;
	Spectrum operator-(float x) const;
	Spectrum operator*(float x) const;
	Spectrum operator/(float x) const;
	Spectrum& operator*=(float x);
	Spectrum operator+(const Spectrum& x) const;
	Spectrum& operator+=(const Spectrum& x);
	Spectrum operator-(const Spectrum& x) const;
	Spectrum operator*(const Spectrum& x) const;
	Spectrum& operator*=(const Spectrum& x);
	Spectrum operator/(const Spectrum& x) const;
	Spectrum safe_sqrt() const;

	void ToXYZ(float& x, float& y, float& z);
	void ToLinearRGB(float& x, float& y, float& z);

	enum ESpectrumType
	{
		kReflectance,
		kIlluminant
	};

	void FromLinearRGB(float r, float g, float b, ESpectrumType type = kReflectance);

private:
	float m_values[kSpectrumSamples] = {};
};


inline uint32_t Spectrum::Size() const
{
	return kSpectrumSamples;
}


inline float& Spectrum::operator[](uint32_t i)
{
	return m_values[i];
}


inline float Spectrum::operator[](uint32_t i) const
{
	return m_values[i];
}


inline Spectrum Spectrum::operator+(float x) const
{
	Spectrum ret = *this;
	for (float& val : ret.m_values)
		val += x;
	return ret;
}


inline Spectrum Spectrum::operator-(float x) const
{
	Spectrum ret = *this;
	for (float& val : ret.m_values)
		val -= x;
	return ret;
}


inline Spectrum Spectrum::operator*(float x) const
{
	Spectrum ret = *this;
	for (float& val : ret.m_values)
		val *= x;
	return ret;
}


inline Spectrum Spectrum::operator/(float x) const
{
	Spectrum ret = *this;
	for (float& val : ret.m_values)
		val /= x;
	return ret;
}


inline Spectrum& Spectrum::operator*=(float x)
{
	for (float& val : m_values)
		val *= x;
	return *this;
}


inline Spectrum Spectrum::operator+(const Spectrum& x) const
{
	Spectrum ret = *this;
	for (uint32_t i = 0; i < kSpectrumSamples; i++)
		ret[i] += x[i];
	return ret;
}


inline Spectrum& Spectrum::operator+=(const Spectrum& x)
{
	for (uint32_t i = 0; i < kSpectrumSamples; i++)
		m_values[i] += x[i];
	return *this;
}


inline Spectrum Spectrum::operator-(const Spectrum& x) const
{
	Spectrum ret = *this;
	for (uint32_t i = 0; i < kSpectrumSamples; i++)
		ret[i] -= x[i];
	return ret;
}


inline Spectrum Spectrum::operator*(const Spectrum& x) const
{
	Spectrum ret = *this;
	for (uint32_t i = 0; i < kSpectrumSamples; i++)
		ret[i] *= x[i];
	return ret;
}


inline Spectrum& Spectrum::operator*=(const Spectrum& x)
{
	for (uint32_t i = 0; i < kSpectrumSamples; i++)
		m_values[i] *= x[i];
	return *this;
}


inline Spectrum Spectrum::operator/(const Spectrum& x) const
{
	Spectrum ret = *this;
	for (uint32_t i = 0; i < kSpectrumSamples; i++)
		ret[i] /= x[i];
	return ret;
}


inline Spectrum Spectrum::safe_sqrt() const
{
	Spectrum ret = *this;
	for (uint32_t i = 0; i < kSpectrumSamples; i++)
		ret[i] = std::sqrt(std::max(0.0f, m_values[i]));
	return ret;
}


inline Spectrum operator*(float f, const Spectrum& spec)
{
	return spec * f;
}


void InitSpectrum();
const Spectrum& GetD65();