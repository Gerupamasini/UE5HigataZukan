// Checks the C++ sun position (SunCore.cpp) against fixtures written by the web build's src/world/Sun.ts.

#include "TideCore/SunCore.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

using namespace HigataTide;

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "usage: %s sun.txt\n", Argv[0]);
		return 2;
	}
	std::ifstream In(Argv[1]);
	if (!In)
	{
		std::fprintf(stderr, "cannot open %s\n", Argv[1]);
		return 2;
	}
	int Checks = 0, Failures = 0;
	for (std::string Line; std::getline(In, Line);)
	{
		std::istringstream S(Line);
		std::string Tag, Id;
		double Lat, Lon, T, El, Az, Decl;
		if (!(S >> Tag >> Id >> Lat >> Lon >> T >> El >> Az >> Decl) || Tag != "sun") continue;
		const FSunPosition P = ComputeSunPosition(T, Lat, Lon);
		++Checks;
		// Same formulas in the same order: only libm differences are allowed (1e-9 deg).
		if (std::fabs(P.Elevation - El) > 1e-9 || std::fabs(P.Azimuth - Az) > 1e-9 || std::fabs(P.Declination - Decl) > 1e-9)
		{
			if (Failures++ < 20)
			{
				std::fprintf(stderr, "FAIL %s @%.17g: el %.17g/%.17g az %.17g/%.17g decl %.17g/%.17g\n", Id.c_str(), T, P.Elevation, El,
					P.Azimuth, Az, P.Declination, Decl);
			}
		}
	}
	std::printf("%d checks, %d failures\n", Checks, Failures);
	return Failures == 0 && Checks > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
