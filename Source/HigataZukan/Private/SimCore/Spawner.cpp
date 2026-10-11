#include "SimCore/Spawner.h"

#include "SimCore/Habitat.h"
#include "SimCore/Individual.h"
#include "SimCore/Rng.h"
#include "SimCore/SimTerrain.h"
#include "SimCore/Species.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace HigataSim
{
	namespace
	{
		constexpr double SpawnRadius = 60.0;
		constexpr double DespawnRadius = 95.0;

		bool Contains(const std::vector<std::string>& List, const std::string& V)
		{
			return std::find(List.begin(), List.end(), V) != List.end();
		}

		/** Species the world lays itself (the clam field's asari, the reef's oysters), not the spawner. */
		bool IsFieldSpecies(const std::string& Id)
		{
			return Id == "ruditapes_philippinarum" || Id == "crassostrea_gigas";
		}
	}

	FSpawner::FSpawner(const FHabitat& InHabitat, const std::vector<FSpecies>& InSpecies)
		: Habitat(InHabitat), Species(InSpecies)
	{
	}

	bool FSpawner::RuleMatches(const FSpawnRule& Rule, int CellIndex, const FSpawnEnv& Env) const
	{
		if (Rule.bHasMaps && !Contains(Rule.Maps, Env.MapId)) return false;
		if (Rule.bHasTime && !Contains(Rule.Time, Env.Tod)) return false;
		if (Rule.bHasSeason && !Contains(Rule.Season, Env.Season)) return false;
		if (Rule.Tide != "any" && Rule.Tide != Env.TidePhase) return false;
		const std::vector<std::string>& CellTags = Habitat.Tags[static_cast<size_t>(CellIndex)];
		bool bTag = false;
		for (const std::string& T : Rule.Tags)
		{
			if (Contains(CellTags, T))
			{
				bTag = true;
				break;
			}
		}
		if (!bTag) return false;
		if (Rule.bHasSubstrate)
		{
			const int Index = Habitat.CoarseSubstrate[static_cast<size_t>(CellIndex)];
			const std::vector<std::string>& Palette = Habitat.Terrain.Palette;
			if (Index < 0 || static_cast<size_t>(Index) >= Palette.size() || !Contains(Rule.Substrate, Palette[static_cast<size_t>(Index)])) return false;
		}
		if (Rule.bHasDepth)
		{
			const double Ground = Habitat.CoarseHeight[static_cast<size_t>(CellIndex)];
			const double S = Habitat.CoarseSpill[static_cast<size_t>(CellIndex)];
			const double Water = S > Habitat.TideLevel && S > Ground + 0.02 ? S : Habitat.TideLevel;
			const double Depth = Water - Ground;
			if (Depth < Rule.Depth0 || Depth > Rule.Depth1) return false;
		}
		return true;
	}

	std::vector<FSpawnRequest> FSpawner::Plan(double Px, double Pz, const FSpawnEnv& Env, const std::vector<FLiveRef>& Live, double MinDist,
		double Scale) const
	{
		const int Cn = Habitat.Cn;
		const double Cs = Habitat.Coarse;
		std::map<std::string, int> Counts;
		std::set<std::string> Occupied;
		for (const FLiveRef& L : Live)
		{
			if (!L.Species) continue;
			++Counts[L.Species->Id];
			Occupied.insert(L.Species->Id + ":" + std::to_string(L.Cell));
		}
		std::vector<FSpawnRequest> Out;
		const int Ci = Habitat.CoarseIndex(Px, Pz);
		const int Ci0 = Ci % Cn, Cj0 = Ci / Cn;
		const int R = static_cast<int>(std::ceil(SpawnRadius / Cs));

		struct FCell
		{
			int Cell;
			double Cx, Cz, D;
		};
		std::vector<FCell> Cells;
		for (int Cj = std::max(0, Cj0 - R); Cj < std::min(Cn, Cj0 + R + 1); ++Cj)
		{
			for (int Ci1 = std::max(0, Ci0 - R); Ci1 < std::min(Cn, Ci0 + R + 1); ++Ci1)
			{
				double Cx = 0, Cz = 0;
				Habitat.CoarseCenter(Ci1, Cj, Cx, Cz);
				const double D = JsHypot(Cx - Px, Cz - Pz);
				if (D > SpawnRadius || D < MinDist) continue;
				Cells.push_back({Cj * Cn + Ci1, Cx, Cz, D});
			}
		}
		std::stable_sort(Cells.begin(), Cells.end(), [](const FCell& A, const FCell& B) { return A.D < B.D; });

		for (const FCell& C : Cells)
		{
			for (const FSpecies& Sp : Species)
			{
				if (IsFieldSpecies(Sp.Id) || Hidden.count(Sp.Id)) continue;
				if (Occupied.count(Sp.Id + ":" + std::to_string(C.Cell))) continue;
				const bool bBird = Sp.Group == "bird";
				for (size_t Ri = 0; Ri < Sp.Spawn.size(); ++Ri)
				{
					const FSpawnRule& Rule = Sp.Spawn[Ri];
					const int Cap = std::max(1, static_cast<int>(std::ceil(Rule.MaxPopulation * (bBird ? 1.0 : Scale))));
					if (Counts[Sp.Id] >= Cap) continue;
					if (!RuleMatches(Rule, C.Cell, Env)) continue;
					const uint32_t Seed = HashInts({static_cast<double>(C.Cell), static_cast<double>(Ri), Env.Day, static_cast<double>(Sp.Id.size() * 131)});
					FRng Rng(Seed);
					const double Expected = (Rule.DensityPer100M2 * Cs * Cs) / 100;
					const double GroupMean = (Rule.Group0 + Rule.Group1) / 2.0;
					const double PGroup = std::min(1.0, Expected / GroupMean) * (bBird ? 1.0 : Scale);
					if (!Rng.Chance(PGroup)) continue;
					const int N = Rng.Int(Rule.Group0, Rule.Group1);
					const double TypicalLength = Rule.bHasLength ? Rule.Length1 : Sp.LengthMean;
					for (int K = 0; K < N; ++K)
					{
						const uint32_t MemberSeed = HashInts({static_cast<double>(Seed), static_cast<double>(K)});
						if (Removed.count(IndividualId(Sp.Id, MemberSeed))) continue;
						if (Counts[Sp.Id] >= Cap) break;
						double X = C.Cx, Z = C.Cz;
						bool bOk = false;
						const bool bAquatic = IsAquatic(Sp);
						if (Contains(Rule.Tags, "small_pool"))
						{
							if (Habitat.RandomPoolPoint(C.Cell, [&Rng]() { return Rng.Next(); }, X, Z))
							{
								bOk = Habitat.Sample(X, Z).Depth >= std::max(Rule.bHasDepth ? Rule.Depth0 : 0.03, MinDepthFor(Sp, TypicalLength));
							}
						}
						for (int Tries = 0; Tries < 8 && !bOk; ++Tries)
						{
							X = C.Cx + Rng.Range(-Cs / 2, Cs / 2);
							Z = C.Cz + Rng.Range(-Cs / 2, Cs / 2);
							const FHabitatSample S = Habitat.Sample(X, Z);
							const double Need = bAquatic ? std::max(Rule.bHasDepth ? Rule.Depth0 : 0.0, MinDepthFor(Sp, TypicalLength)) : 0.0;
							if (Rule.bHasDepth)
							{
								bOk = S.Depth >= (bAquatic ? std::max(Rule.Depth0, Need) : Rule.Depth0) && S.Depth <= Rule.Depth1;
							}
							else
							{
								bOk = bAquatic ? S.Depth >= Need : S.bExposed;
							}
						}
						if (!bOk) continue;
						FSpawnRequest Req;
						Req.Species = &Sp;
						Req.RuleIndex = static_cast<int>(Ri);
						Req.Cell = C.Cell;
						Req.Seed = MemberSeed;
						Req.X = X;
						Req.Z = Z;
						Req.bHasLengthRange = Rule.bHasLength;
						Req.Length0 = Rule.Length0;
						Req.Length1 = Rule.Length1;
						Out.push_back(Req);
						++Counts[Sp.Id];
					}
					Occupied.insert(Sp.Id + ":" + std::to_string(C.Cell));
					break;
				}
			}
		}
		return Out;
	}

	std::vector<std::string> FSpawner::Cull(double Px, double Pz, const FSpawnEnv& Env, std::vector<FLiveRef>& Live) const
	{
		std::vector<std::string> Out;
		for (FLiveRef& L : Live)
		{
			const double D = JsHypot(L.X - Px, L.Z - Pz);
			if (D > DespawnRadius)
			{
				Out.push_back(L.Id);
				continue;
			}
			if (!L.Species) continue;
			const int Cell = Habitat.CoarseIndex(L.X, L.Z);
			bool bMatches = true;
			if (L.RuleIndex >= 0 && static_cast<size_t>(L.RuleIndex) < L.Species->Spawn.size())
			{
				bMatches = RuleMatches(L.Species->Spawn[static_cast<size_t>(L.RuleIndex)], Cell, Env);
				for (size_t Ri = 0; !bMatches && Ri < L.Species->Spawn.size(); ++Ri)
				{
					bMatches = RuleMatches(L.Species->Spawn[Ri], Cell, Env);
				}
			}
			if (bMatches)
			{
				L.MismatchSince = 0;
				continue;
			}
			if (L.MismatchSince == 0) L.MismatchSince = Env.GameMs;
			else if (Env.GameMs - L.MismatchSince > 30000 && D > 20) Out.push_back(L.Id);
		}
		return Out;
	}
}
