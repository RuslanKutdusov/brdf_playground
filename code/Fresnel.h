#pragma once

static const float kAirIOR = 1.00028f;

inline float safe_sqrt(float value)
{
	return std::sqrt(std::max(0.0f, value));
}


inline float FresnelConductorExact(float cosThetaI, float eta, float k, float outterMediaIOR = kAirIOR)
{
	/* Modified from "Optics" by K.D. Moeller, University Science Books, 1988 */

	eta /= outterMediaIOR;
	k /= outterMediaIOR;

	float cosThetaI2 = cosThetaI * cosThetaI;
	float sinThetaI2 = 1 - cosThetaI2;
	float sinThetaI4 = sinThetaI2 * sinThetaI2;

	float temp1 = eta * eta - k * k - sinThetaI2;
	float a2pb2 = safe_sqrt(temp1 * temp1 + 4 * k * k * eta * eta);
	float a = safe_sqrt(0.5f * (a2pb2 + temp1));

	float term1 = a2pb2 + cosThetaI2;
	float term2 = 2 * a * cosThetaI;

	float Rs2 = (term1 - term2) / (term1 + term2);

	float term3 = a2pb2 * cosThetaI2 + sinThetaI4;
	float term4 = term2 * sinThetaI2;

	float Rp2 = Rs2 * (term3 - term4) / (term3 + term4);

	return 0.5f * (Rp2 + Rs2);
}


inline Spectrum FresnelConductorExact(float cosThetaI, const Spectrum& eta, const Spectrum& k, float outterMediaIOR = kAirIOR)
{
	/* Modified from "Optics" by K.D. Moeller, University Science Books, 1988 */
	float cosThetaI2 = cosThetaI * cosThetaI;
	float sinThetaI2 = 1.0f - cosThetaI2;
	Spectrum eta2 = eta * eta / (outterMediaIOR * outterMediaIOR);
	Spectrum etak2 = k * k / (outterMediaIOR * outterMediaIOR);

	Spectrum t0 = eta2 - etak2 - sinThetaI2;
	Spectrum a2plusb2 = (t0 * t0 +  eta2 * etak2 * 4.0f).safe_sqrt();
	Spectrum t1 = a2plusb2 + cosThetaI2;
	Spectrum a = ((a2plusb2 + t0) * 0.5f).safe_sqrt();
	Spectrum t2 = a * 2.0f * cosThetaI;
	Spectrum Rs = (t1 - t2) / (t1 + t2);

	Spectrum t3 = a2plusb2 * cosThetaI2 + sinThetaI2 * sinThetaI2;
	Spectrum t4 = t2 * sinThetaI2;
	Spectrum Rp = Rs * (t3 - t4) / (t3 + t4);

	return (Rp + Rs) * 0.5f;
}


inline float FresnelSchlick(float F0, float VoH)
{
	float Fc = powf(1.0f - VoH, 5.0f);
	return (1.0f - Fc) * F0 + Fc;
}