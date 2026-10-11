// Japan time, seasons, time of day and the tide's phase, ported from the web build (src/core/Time.ts, Moon.ts,
// src/world/Sun.ts timeOfDay, App.tidePhase). Game time is always JST (decided for every player, 2026-10-11).

#pragma once

#include <string>

namespace HigataSim
{
	struct FJstParts
	{
		int Year = 1970;
		int Month = 1; // 1-12
		int Day = 1;
		int Hour = 0;
		int Minute = 0;
		int Second = 0;
	};

	FJstParts JstParts(double Ms);

	/** "spring" (Mar-May), "summer" (Jun-Aug), "autumn" (Sep-Nov), "winter". */
	std::string SeasonOf(double Ms);

	/** "day" above 6 deg, "dawn" / "dusk" within 6 deg of the horizon (by the JST hour), else "night". */
	std::string TimeOfDayOf(double SunElevationDeg, int HourJst);

	/** "low" / "high" beyond 35 % of the amplitude, else "rising" / "falling" by the rate. */
	std::string TidePhaseOf(double LevelM, double RateMPerH, double AmplitudeM);

	/** Moon age in days and the Japanese tide name (oshio, nakashio, koshio, nagashio, wakashio; UTF-8) for the lunar day. */
	double MoonAge(double Ms);
	std::string TideNameJa(double Ms);
}
