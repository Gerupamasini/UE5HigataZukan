#include "SimCore/JstTime.h"

#include <algorithm>
#include <cmath>

namespace HigataSim
{
	namespace
	{
		// Days since 1970-01-01 to a civil date (Howard Hinnant's algorithm), as JavaScript's Date does (proleptic Gregorian).
		void CivilFromDays(long long Z, int& Y, int& M, int& D)
		{
			Z += 719468;
			const long long Era = (Z >= 0 ? Z : Z - 146096) / 146097;
			const unsigned Doe = static_cast<unsigned>(Z - Era * 146097);
			const unsigned Yoe = (Doe - Doe / 1460 + Doe / 36524 - Doe / 146096) / 365;
			const long long YY = static_cast<long long>(Yoe) + Era * 400;
			const unsigned Doy = Doe - (365 * Yoe + Yoe / 4 - Yoe / 100);
			const unsigned Mp = (5 * Doy + 2) / 153;
			D = static_cast<int>(Doy - (153 * Mp + 2) / 5 + 1);
			M = static_cast<int>(Mp < 10 ? Mp + 3 : Mp - 9);
			Y = static_cast<int>(YY + (M <= 2 ? 1 : 0));
		}
	}

	FJstParts JstParts(double Ms)
	{
		const double Local = Ms + 9.0 * 3600 * 1000;
		const double Days = std::floor(Local / 86400000.0);
		const double MsOfDay = Local - Days * 86400000.0;
		FJstParts P;
		CivilFromDays(static_cast<long long>(Days), P.Year, P.Month, P.Day);
		const long long Sec = static_cast<long long>(std::floor(MsOfDay / 1000.0));
		P.Hour = static_cast<int>(Sec / 3600);
		P.Minute = static_cast<int>((Sec / 60) % 60);
		P.Second = static_cast<int>(Sec % 60);
		return P;
	}

	std::string SeasonOf(double Ms)
	{
		const int M = JstParts(Ms).Month;
		if (M >= 3 && M <= 5) return "spring";
		if (M >= 6 && M <= 8) return "summer";
		if (M >= 9 && M <= 11) return "autumn";
		return "winter";
	}

	std::string TimeOfDayOf(double SunElevationDeg, int HourJst)
	{
		if (SunElevationDeg > 6) return "day";
		if (SunElevationDeg > -6) return HourJst < 12 ? "dawn" : "dusk";
		return "night";
	}

	std::string TidePhaseOf(double LevelM, double RateMPerH, double AmplitudeM)
	{
		if (LevelM < -0.35 * AmplitudeM) return "low";
		if (LevelM > 0.35 * AmplitudeM) return "high";
		return RateMPerH >= 0 ? "rising" : "falling";
	}

	double MoonAge(double Ms)
	{
		const double Synodic = 29.530588853;
		const double RefNewMoon = 947182440000.0; // Date.UTC(2000, 0, 6, 18, 14)
		const double D = (Ms - RefNewMoon) / 86400000.0;
		return std::fmod(std::fmod(D, Synodic) + Synodic, Synodic);
	}

	std::string TideNameJa(double Ms)
	{
		const int D = std::min(30, static_cast<int>(std::floor(MoonAge(Ms))) + 1);
		if (D <= 3 || (D >= 15 && D <= 17) || D >= 29) return "\xE5\xA4\xA7\xE6\xBD\xAE";      // oshio (spring tide)
		if (D == 11 || D == 25) return "\xE9\x95\xB7\xE6\xBD\xAE";                             // nagashio
		if (D == 12 || D == 26) return "\xE8\x8B\xA5\xE6\xBD\xAE";                             // wakashio
		if ((D >= 8 && D <= 10) || (D >= 22 && D <= 24)) return "\xE5\xB0\x8F\xE6\xBD\xAE";    // koshio (neap)
		return "\xE4\xB8\xAD\xE6\xBD\xAE";                                                      // nakashio
	}
}
