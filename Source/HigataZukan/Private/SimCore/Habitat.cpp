#include "SimCore/Habitat.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>

namespace HigataSim
{
	namespace
	{
		/** Pools smaller than this (m2) count as 'small_pool' habitat. */
		constexpr double SmallPoolM2 = 40.0;
		const int Dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	}

	FHabitat::FHabitat(const FSimTerrain& InTerrain, double CoarseCell)
		: Terrain(InTerrain)
	{
		ComputeSpill();
		FindPools();
		BuildPoolLevels();
		Coarse = CoarseCell;
		Cn = std::max(2, static_cast<int>(JsRound(Terrain.Size / CoarseCell)));
		const size_t Cn2 = static_cast<size_t>(Cn) * static_cast<size_t>(Cn);
		Tags.assign(Cn2, {});
		FeatureTags.assign(Cn2, {});
		LastWet.assign(Cn2, 0.0);
		CoarseHeight.assign(Cn2, 0.0f);
		CoarseSubstrate.assign(Cn2, 0);
		CoarseSpill.assign(Cn2, 0.0f);
		CoarseDist.assign(Cn2, 0.0f);
		CoarseSmallPool.assign(Cn2, -1e3f);
		const int N = Terrain.N;
		for (const FPoolInfo& P : Pools)
		{
			if (P.Area >= SmallPoolM2) continue;
			for (int K : P.Cells)
			{
				const int Ci = CoarseIndex(-Terrain.Half + (K % N) * Terrain.Cell, -Terrain.Half + (K / N) * Terrain.Cell);
				CoarseSmallPool[static_cast<size_t>(Ci)] = std::max(CoarseSmallPool[static_cast<size_t>(Ci)], P.Level);
			}
		}
		for (int J = 0; J < Cn; ++J)
		{
			for (int I = 0; I < Cn; ++I)
			{
				double X = 0, Z = 0;
				CoarseCenter(I, J, X, Z);
				const size_t K = static_cast<size_t>(J * Cn + I);
				CoarseHeight[K] = static_cast<float>(Terrain.HeightAt(X, Z));
				CoarseSubstrate[K] = static_cast<uint8_t>(Terrain.SubstrateIndexAt(X, Z));
				CoarseSpill[K] = static_cast<float>(SpillAt(X, Z));
			}
		}
	}

	void FHabitat::ComputeSpill()
	{
		// Priority flood: for each fine cell, the lowest height one must climb to in order to reach the map edge.
		const int N = Terrain.N;
		const std::vector<float>& H = Terrain.Heights;
		const size_t Count = static_cast<size_t>(N) * static_cast<size_t>(N);
		Spill.assign(Count, std::numeric_limits<float>::infinity());
		std::vector<uint8_t> Done(Count, 0);
		using FItem = std::pair<float, int>;
		std::priority_queue<FItem, std::vector<FItem>, std::greater<FItem>> Heap;
		for (int I = 0; I < N; ++I)
		{
			for (int K : {I, (N - 1) * N + I, I * N, I * N + N - 1})
			{
				if (!Done[static_cast<size_t>(K)])
				{
					Done[static_cast<size_t>(K)] = 1;
					Spill[static_cast<size_t>(K)] = H[static_cast<size_t>(K)];
					Heap.push({H[static_cast<size_t>(K)], K});
				}
			}
		}
		while (!Heap.empty())
		{
			const FItem Top = Heap.top();
			Heap.pop();
			const int K = Top.second, I = K % N, J = K / N;
			for (const auto& D : Dirs)
			{
				const int Ii = I + D[0], Jj = J + D[1];
				if (Ii < 0 || Jj < 0 || Ii >= N || Jj >= N) continue;
				const size_t Kk = static_cast<size_t>(Jj * N + Ii);
				if (Done[Kk]) continue;
				Done[Kk] = 1;
				Spill[Kk] = std::max(Top.first, H[Kk]);
				Heap.push({Spill[Kk], static_cast<int>(Kk)});
			}
		}
	}

	void FHabitat::FindPools()
	{
		const int N = Terrain.N;
		const std::vector<float>& H = Terrain.Heights;
		const size_t Count = static_cast<size_t>(N) * static_cast<size_t>(N);
		std::vector<uint8_t> Seen(Count, 0);
		for (size_t K = 0; K < Count; ++K)
		{
			if (Seen[K] || static_cast<double>(Spill[K]) - H[K] < 0.03) continue;
			const float Level = Spill[K];
			std::vector<int> Cells;
			std::vector<int> Stack = {static_cast<int>(K)};
			Seen[K] = 1;
			double Sx = 0, Sz = 0;
			while (!Stack.empty())
			{
				const int C = Stack.back();
				Stack.pop_back();
				Cells.push_back(C);
				const int I = C % N, J = C / N;
				Sx += I;
				Sz += J;
				for (const auto& D : Dirs)
				{
					const int Ii = I + D[0], Jj = J + D[1];
					if (Ii < 0 || Jj < 0 || Ii >= N || Jj >= N) continue;
					const size_t Kk = static_cast<size_t>(Jj * N + Ii);
					if (Seen[Kk] || static_cast<double>(Spill[Kk]) - H[Kk] < 0.03 || std::abs(static_cast<double>(Spill[Kk]) - Level) > 1e-4) continue;
					Seen[Kk] = 1;
					Stack.push_back(static_cast<int>(Kk));
				}
			}
			if (Cells.size() < 6) continue;
			FPoolInfo Pool;
			Pool.Id = static_cast<int>(Pools.size());
			Pool.Level = Level;
			Pool.Area = static_cast<double>(Cells.size()) * Terrain.Cell * Terrain.Cell;
			Pool.Cx = -Terrain.Half + (Sx / static_cast<double>(Cells.size())) * Terrain.Cell;
			Pool.Cz = -Terrain.Half + (Sz / static_cast<double>(Cells.size())) * Terrain.Cell;
			Pool.Cells = std::move(Cells);
			Pools.push_back(std::move(Pool));
		}
		std::stable_sort(Pools.begin(), Pools.end(), [](const FPoolInfo& A, const FPoolInfo& B) { return A.Area > B.Area; });
	}

	void FHabitat::BuildPoolLevels()
	{
		const int N = Terrain.N;
		const size_t Count = static_cast<size_t>(N) * static_cast<size_t>(N);
		PoolLevels.assign(Count, -1e3f);
		for (const FPoolInfo& P : Pools)
		{
			for (int K : P.Cells) PoolLevels[static_cast<size_t>(K)] = P.Level;
		}
		// grow each pool by two cells so its shallow fringe is covered too
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			const std::vector<float> Src = PoolLevels;
			for (size_t K = 0; K < Count; ++K)
			{
				if (Src[K] > -1e2f) continue;
				const int I = static_cast<int>(K) % N, J = static_cast<int>(K) / N;
				float Best = -1e3f;
				for (const auto& D : Dirs)
				{
					const int Ii = I + D[0], Jj = J + D[1];
					if (Ii < 0 || Jj < 0 || Ii >= N || Jj >= N) continue;
					Best = std::max(Best, Src[static_cast<size_t>(Jj * N + Ii)]);
				}
				if (Best > -1e2f) PoolLevels[K] = Best;
			}
		}
	}

	bool FHabitat::SetFeatureBits(const std::vector<uint8_t>& Bits)
	{
		if (Bits.size() != FeatureTags.size()) return false;
		for (size_t K = 0; K < Bits.size(); ++K)
		{
			std::vector<std::string>& T = FeatureTags[K];
			T.clear();
			if (Bits[K] & 1) T.push_back("eelgrass");
			if (Bits[K] & 2) T.push_back("eelgrass_edge");
			if (Bits[K] & 4) T.push_back("bare");
		}
		return true;
	}

	void FHabitat::CoarseCenter(int I, int J, double& OutX, double& OutZ) const
	{
		OutX = -Terrain.Half + (I + 0.5) * Coarse;
		OutZ = -Terrain.Half + (J + 0.5) * Coarse;
	}

	int FHabitat::CoarseIndex(double X, double Z) const
	{
		const int I = std::max(0, std::min(Cn - 1, static_cast<int>(std::floor((X + Terrain.Half) / Coarse))));
		const int J = std::max(0, std::min(Cn - 1, static_cast<int>(std::floor((Z + Terrain.Half) / Coarse))));
		return J * Cn + I;
	}

	double FHabitat::SpillAt(double X, double Z) const
	{
		const int N = Terrain.N;
		const int I = std::max(0, std::min(N - 1, static_cast<int>(JsRound((X + Terrain.Half) / Terrain.Cell))));
		const int J = std::max(0, std::min(N - 1, static_cast<int>(JsRound((Z + Terrain.Half) / Terrain.Cell))));
		return Spill[static_cast<size_t>(J * N + I)];
	}

	double FHabitat::WaterAt(double X, double Z) const
	{
		const double Ground = Terrain.HeightAt(X, Z);
		const double S = SpillAt(X, Z);
		if (S > TideLevel && S > Ground + 0.02) return S;
		return TideLevel;
	}

	double FHabitat::DepthAt(double X, double Z) const
	{
		return WaterAt(X, Z) - Terrain.HeightAt(X, Z);
	}

	void FHabitat::Update(double NowMs, double TideLevelM)
	{
		TideLevel = TideLevelM;
		const size_t Cn2 = static_cast<size_t>(Cn) * static_cast<size_t>(Cn);
		std::vector<uint8_t> Wet(Cn2, 0);
		for (size_t K = 0; K < Cn2; ++K)
		{
			const double Ground = CoarseHeight[K];
			const double S = CoarseSpill[K];
			const double Water = S > TideLevelM && S > Ground + 0.02 ? S : TideLevelM;
			if (Water > Ground)
			{
				Wet[K] = 1;
				LastWet[K] = NowMs;
			}
		}
		// distance to water (BFS over coarse cells)
		std::fill(CoarseDist.begin(), CoarseDist.end(), 1e9f);
		std::vector<int> Queue;
		for (size_t K = 0; K < Cn2; ++K)
		{
			if (Wet[K])
			{
				CoarseDist[K] = 0;
				Queue.push_back(static_cast<int>(K));
			}
		}
		for (size_t Q = 0; Q < Queue.size(); ++Q)
		{
			const int K = Queue[Q], I = K % Cn, J = K / Cn;
			for (const auto& D : Dirs)
			{
				const int Ii = I + D[0], Jj = J + D[1];
				if (Ii < 0 || Jj < 0 || Ii >= Cn || Jj >= Cn) continue;
				const size_t Kk = static_cast<size_t>(Jj * Cn + Ii);
				const float Next = static_cast<float>(static_cast<double>(CoarseDist[static_cast<size_t>(K)]) + Coarse);
				if (CoarseDist[Kk] > static_cast<double>(CoarseDist[static_cast<size_t>(K)]) + Coarse)
				{
					CoarseDist[Kk] = Next;
					Queue.push_back(static_cast<int>(Kk));
				}
			}
		}
		for (size_t K = 0; K < Cn2; ++K)
		{
			std::vector<std::string>& T = Tags[K];
			T.clear();
			const double Ground = CoarseHeight[K];
			const double S = CoarseSpill[K];
			const bool bInPool = S > TideLevelM && S > Ground + 0.02;
			const double Water = bInPool ? S : TideLevelM;
			const double Depth = Water - Ground;
			const std::string& Sub = Terrain.PaletteName(CoarseSubstrate[K]);
			if (Depth <= 0)
			{
				T.push_back(Sub == "sand" || Sub == "gravel" || Sub == "rock" ? "exposed_sand" : "exposed_mud");
				if (CoarseDist[K] <= Coarse * 1.01) T.push_back("waterline");
			}
			else
			{
				if (bInPool) T.push_back("pool");
				else if (Depth <= 0.6) T.push_back("shallow");
				else T.push_back("deep");
				if (Sub == "channel" && Depth > 0.05) T.push_back("channel");
			}
			if (CoarseSmallPool[K] > TideLevelM + 0.01) T.push_back("small_pool");
			for (const std::string& F : FeatureTags[K]) T.push_back(F);
		}
	}

	FHabitatSample FHabitat::Sample(double X, double Z) const
	{
		FHabitatSample Out;
		const double Ground = Terrain.HeightAt(X, Z);
		const double S = SpillAt(X, Z);
		Out.bInPool = S > TideLevel && S > Ground + 0.02;
		Out.WaterLevel = Out.bInPool ? S : TideLevel;
		Out.Depth = Out.WaterLevel - Ground;
		Out.GroundHeight = Ground;
		Out.Substrate = Terrain.SubstrateAt(X, Z);
		Out.bExposed = Out.Depth <= 0;
		Out.DistToWater = Out.Depth > 0 ? 0.0 : static_cast<double>(CoarseDist[static_cast<size_t>(CoarseIndex(X, Z))]);
		return Out;
	}

	bool FHabitat::RandomPoolPoint(int CoarseIdx, const std::function<double()>& Rnd, double& OutX, double& OutZ) const
	{
		const int N = Terrain.N;
		const int Ci = CoarseIdx % Cn, Cj = CoarseIdx / Cn;
		const double X0 = -Terrain.Half + Ci * Coarse, Z0 = -Terrain.Half + Cj * Coarse;
		const int I0 = std::max(0, static_cast<int>(JsRound((X0 + Terrain.Half) / Terrain.Cell)));
		const int J0 = std::max(0, static_cast<int>(JsRound((Z0 + Terrain.Half) / Terrain.Cell)));
		const int Span = static_cast<int>(std::ceil(Coarse / Terrain.Cell));
		std::vector<int> Cands;
		for (int J = J0; J < std::min(N, J0 + Span); ++J)
		{
			for (int I = I0; I < std::min(N, I0 + Span); ++I)
			{
				const size_t K = static_cast<size_t>(J * N + I);
				if (PoolLevels[K] > TideLevel + 0.01 && PoolLevels[K] > Terrain.Heights[K] + 0.03) Cands.push_back(static_cast<int>(K));
			}
		}
		if (Cands.empty()) return false;
		const int K = Cands[static_cast<size_t>(std::floor(Rnd() * static_cast<double>(Cands.size())))];
		OutX = -Terrain.Half + (K % N) * Terrain.Cell + (Rnd() - 0.5) * Terrain.Cell * 0.6;
		OutZ = -Terrain.Half + (K / N) * Terrain.Cell + (Rnd() - 0.5) * Terrain.Cell * 0.6;
		return true;
	}
}
