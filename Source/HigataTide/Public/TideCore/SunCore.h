// Solar position, ported from the web build (Gerupamasini/gerupamasini, src/world/Sun.ts).
// NOAA approximation, accuracy ~0.01 deg for 1950-2050. Plain C++17 like TideCore; checked by tests/tide.

#pragma once

#include "TideCore/TideCoreApi.h"

namespace HigataTide
{
	struct FSunPosition
	{
		/** Degrees above the horizon. */
		double Elevation = 0.0;
		/** Degrees clockwise from north. */
		double Azimuth = 0.0;
		double Declination = 0.0;
	};

	/** Sun position at a UTC epoch (ms) seen from a latitude / longitude (degrees, east positive). */
	HIGATATIDE_API FSunPosition ComputeSunPosition(double Ms, double LatDeg, double LonDeg);
}
