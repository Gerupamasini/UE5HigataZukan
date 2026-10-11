// Port of the web build's src/world/Sun.ts: keep the arithmetic in the same order so the golden fixtures match.

#include "TideCore/SunCore.h"

#include "TideCore/TideCore.h"

#include <algorithm>
#include <cmath>

namespace HigataTide
{
	namespace
	{
		constexpr double SunPi = 3.14159265358979323846;
		constexpr double D2R = SunPi / 180;
		constexpr double R2D = 180 / SunPi;
	}

	FSunPosition ComputeSunPosition(double Ms, double LatDeg, double LonDeg)
	{
		const double Jd = JulianDate(Ms);
		const double T = (Jd - 2451545) / 36525;
		const double L0 = std::fmod(std::fmod(280.46646 + T * (36000.76983 + T * 0.0003032), 360.0) + 360, 360.0);
		const double M = 357.52911 + T * (35999.05029 - 0.0001537 * T);
		const double E = 0.016708634 - T * (0.000042037 + 0.0000001267 * T);
		const double Mr = M * D2R;
		const double C = (1.914602 - T * (0.004817 + 0.000014 * T)) * std::sin(Mr) + (0.019993 - 0.000101 * T) * std::sin(2 * Mr)
			+ 0.000289 * std::sin(3 * Mr);
		const double TrueLong = L0 + C;
		const double Omega = 125.04 - 1934.136 * T;
		const double Lambda = TrueLong - 0.00569 - 0.00478 * std::sin(Omega * D2R);
		const double Eps0 = 23 + (26 + (21.448 - T * (46.815 + T * (0.00059 - T * 0.001813))) / 60) / 60;
		const double Eps = Eps0 + 0.00256 * std::cos(Omega * D2R);
		const double Decl = std::asin(std::sin(Eps * D2R) * std::sin(Lambda * D2R));
		const double Y = std::pow(std::tan((Eps / 2) * D2R), 2);
		const double L0r = L0 * D2R;
		const double Eot = 4 * R2D
			* (Y * std::sin(2 * L0r) - 2 * E * std::sin(Mr) + 4 * E * Y * std::sin(Mr) * std::cos(2 * L0r) - 0.5 * Y * Y * std::sin(4 * L0r)
				- 1.25 * E * E * std::sin(2 * Mr));
		const double MinutesUT = std::fmod(std::fmod(Ms / 60000, 1440.0) + 1440, 1440.0);
		const double Tst = std::fmod(MinutesUT + Eot + 4 * LonDeg + 1440, 1440.0);
		const double H = (Tst / 4 - 180) * D2R;
		const double Phi = LatDeg * D2R;
		const double SinEl = std::sin(Phi) * std::sin(Decl) + std::cos(Phi) * std::cos(Decl) * std::cos(H);
		const double El = std::asin(std::max(-1.0, std::min(1.0, SinEl)));
		double Az = std::atan2(std::sin(H), std::cos(H) * std::sin(Phi) - std::tan(Decl) * std::cos(Phi)) * R2D + 180;
		Az = std::fmod(std::fmod(Az, 360.0) + 360, 360.0);

		FSunPosition Out;
		Out.Elevation = El * R2D;
		Out.Azimuth = Az;
		Out.Declination = Decl * R2D;
		return Out;
	}
}
