// Habitat state from the terrain and the tide, ported from the web build (src/world/Habitat.ts): spill heights
// (priority flood), tide pools, the coarse cells the spawn rules read, and their tags for the current tide.
// The standing features (eelgrass, its edge, bare sand) come baked from the web build (features.u8). No feeding
// pits yet (the Hashirimizu map has none).

#pragma once

#include "SimCore/SimTerrain.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace HigataSim
{
	struct FPoolInfo
	{
		int Id = 0;
		float Level = 0.0f;
		std::vector<int> Cells;
		double Cx = 0.0;
		double Cz = 0.0;
		double Area = 0.0;
	};

	struct FHabitatSample
	{
		/** Water depth (m); negative when exposed. */
		double Depth = 0.0;
		std::string Substrate;
		bool bExposed = false;
		bool bInPool = false;
		double WaterLevel = 0.0;
		double GroundHeight = 0.0;
		/** Horizontal distance to the nearest wet coarse cell (m), 0 when submerged. */
		double DistToWater = 0.0;
	};

	class FHabitat
	{
	public:
		FHabitat(const FSimTerrain& InTerrain, double CoarseCell);

		/** The baked standing features per coarse cell: bit 0 eelgrass, bit 1 eelgrass_edge, bit 2 bare. */
		bool SetFeatureBits(const std::vector<uint8_t>& Bits);

		/** Recompute the coarse tags for the tide. */
		void Update(double NowMs, double TideLevelM);

		FHabitatSample Sample(double X, double Z) const;
		/** A random point in a tide pool within a coarse cell (water above the tide); false if none. */
		bool RandomPoolPoint(int CoarseIndex, const std::function<double()>& Rnd, double& OutX, double& OutZ) const;

		double SpillAt(double X, double Z) const;
		double WaterAt(double X, double Z) const;
		double DepthAt(double X, double Z) const;
		void CoarseCenter(int I, int J, double& OutX, double& OutZ) const;
		int CoarseIndex(double X, double Z) const;

		const FSimTerrain& Terrain;
		double Coarse = 5.0;
		int Cn = 2;
		double TideLevel = 0.0;

		std::vector<float> Spill;
		std::vector<FPoolInfo> Pools;
		std::vector<float> PoolLevels;
		std::vector<std::vector<std::string>> Tags;
		std::vector<std::vector<std::string>> FeatureTags;
		std::vector<double> LastWet;
		std::vector<float> CoarseHeight;
		std::vector<uint8_t> CoarseSubstrate;
		std::vector<float> CoarseSpill;
		std::vector<float> CoarseDist;
		std::vector<float> CoarseSmallPool;

	private:
		void ComputeSpill();
		void FindPools();
		void BuildPoolLevels();
	};
}
