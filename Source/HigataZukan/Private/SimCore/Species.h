// Species definitions read from Content/Data/species/<id>.json, with the defaults the web build's zod schema fills
// in (src/data/schemas/species.ts). Only what the simulation and the encyclopedia use so far.

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace HigataSim
{
	class FJson;

	struct FSpawnRule
	{
		std::vector<std::string> Tags;
		bool bHasSubstrate = false;
		std::vector<std::string> Substrate;
		bool bHasDepth = false;
		double Depth0 = 0.0;
		double Depth1 = 0.0;
		bool bHasTime = false;
		std::vector<std::string> Time;
		bool bHasSeason = false;
		std::vector<std::string> Season;
		std::string Tide = "any";
		bool bHasMaps = false;
		std::vector<std::string> Maps;
		double DensityPer100M2 = 0.0;
		int Group0 = 1;
		int Group1 = 1;
		int MaxPopulation = 50;
		bool bHasLength = false;
		double Length0 = 0.0;
		double Length1 = 0.0;
	};

	struct FStage
	{
		std::string Id;
		std::string Ja;
		bool bHasMaxLength = false;
		double MaxLengthMm = 0.0;
	};

	struct FTrait
	{
		std::string Id;
		std::string Ja;
		std::string When;
	};

	struct FSpecies
	{
		std::string Id;
		std::string NameJa;
		std::string NameSci;
		std::string NameEn;
		std::string Group;
		std::string Locomotion;
		bool bHasAquatic = false;
		bool bAquatic = false;
		bool bCollectable = false;
		bool bProtected = false;

		double ModelLengthMm = 0.0;
		double ViewDistanceM = 0.0;
		std::string Placeholder;
		std::string Driver;

		double LengthMin = 0.0;
		double LengthMax = 0.0;
		double LengthMean = 0.0;
		double LengthSd = 1.0;
		double WeightA = 0.0;
		double WeightB = 3.0;
		bool bHasMinDepthFraction = false;
		double MinDepthFraction = 0.15;

		double MaleRatio = 0.5;

		bool bHasBreeding = false;
		std::vector<int> BreedingMonths;
		double GravidShare = 0.7;
		double OffSeasonShare = 0.0;
		double DressShare = 0.0;

		std::vector<FStage> Stages;
		std::vector<FTrait> Traits;
		std::vector<FSpawnRule> Spawn;

		std::string BrainTree;
		/** The brain's numeric parameters (fleeDistance_m, restMean_s, ...). */
		std::vector<std::pair<std::string, double>> BrainNumbers;

		std::vector<std::string> CaptureTools;
		double BaseDifficulty = 0.5;
		double AlertPenalty = 0.4;

		std::string Description;
		std::string HabitatHint;

		double BrainNumber(const std::string& Key, double Default) const;
	};

	bool ParseSpecies(const FJson& Json, FSpecies& Out, std::string& Error);

	/** Kept in the water: swimmers and crustaceans unless the species says otherwise. */
	bool IsAquatic(const FSpecies& Species);

	/** The least water an aquatic animal is placed in or stays in (m). */
	double MinDepthFor(const FSpecies& Species, double LengthMm);
}
