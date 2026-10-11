// One animal's fixed traits, generated deterministically from its species and seed, ported from the web build
// (src/creatures/Individual.ts generateIndividual).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace HigataSim
{
	struct FSpecies;

	struct FIndividual
	{
		std::string Id;
		const FSpecies* Species = nullptr;
		uint32_t Seed = 0;
		double X = 0.0;
		double Z = 0.0;
		/** Radians, the web build's convention. */
		double Heading = 0.0;
		double LengthMm = 0.0;
		double WeightG = 0.0;
		/** 'm' or 'f'. */
		char Sex = 'f';
		std::string Stage;
		std::vector<std::string> Traits;
		bool bGravid = false;
		bool bDress = false;
		double LengthPct = 50.0;
		double Wariness = 1.0;
		double Energy = 0.5;
		int Cell = 0;
		int RuleIndex = 0;
	};

	FIndividual GenerateIndividual(const FSpecies& Species, uint32_t Seed, double X, double Z, int Cell, int RuleIndex, double NowMs,
		bool bHasLengthRange = false, double Length0 = 0.0, double Length1 = 0.0);

	/** The id the web build gives a seed's individual: "<species>#<hashInts(seed, 7) as 8 hex>". */
	std::string IndividualId(const std::string& SpeciesId, uint32_t Seed);
}
