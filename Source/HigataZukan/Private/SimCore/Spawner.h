// Who appears where, ported from the web build (src/creatures/Spawner.ts plan / cull / ruleMatches).
// Deterministic per coarse cell and day; checked against the web build by tests/sim.

#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace HigataSim
{
	class FHabitat;
	struct FSpecies;
	struct FSpawnRule;

	struct FSpawnEnv
	{
		std::string Tod;
		std::string Season;
		std::string TidePhase;
		std::string MapId;
		double GameMs = 0.0;
		double Day = 0.0;
	};

	struct FSpawnRequest
	{
		const FSpecies* Species = nullptr;
		int RuleIndex = 0;
		int Cell = 0;
		uint32_t Seed = 0;
		double X = 0.0;
		double Z = 0.0;
		bool bHasLengthRange = false;
		double Length0 = 0.0;
		double Length1 = 0.0;
	};

	/** What the spawner needs to know about a live individual. */
	struct FLiveRef
	{
		std::string Id;
		const FSpecies* Species = nullptr;
		int Cell = 0;
		int RuleIndex = 0;
		double X = 0.0;
		double Z = 0.0;
		/** Game time its rule stopped matching (0 = matches). */
		double MismatchSince = 0.0;
	};

	class FSpawner
	{
	public:
		FSpawner(const FHabitat& InHabitat, const std::vector<FSpecies>& InSpecies);

		bool RuleMatches(const FSpawnRule& Rule, int CellIndex, const FSpawnEnv& Env) const;

		/** Spawns for the cells around (Px, Pz). Scale is the quality preset's share of the animals. */
		std::vector<FSpawnRequest> Plan(double Px, double Pz, const FSpawnEnv& Env, const std::vector<FLiveRef>& Live, double MinDist = 10.0,
			double Scale = 1.0) const;

		/** Ids of individuals that should leave; updates their MismatchSince. */
		std::vector<std::string> Cull(double Px, double Pz, const FSpawnEnv& Env, std::vector<FLiveRef>& Live) const;

		/** Individuals caught or otherwise gone today: never planned again. */
		std::set<std::string> Removed;

	private:
		const FHabitat& Habitat;
		const std::vector<FSpecies>& Species;
	};
}
