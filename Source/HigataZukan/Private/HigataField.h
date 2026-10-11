#pragma once

#include "CoreMinimal.h"
#include "SimCore/Habitat.h"
#include "SimCore/Individual.h"
#include "SimCore/SimTerrain.h"
#include "SimCore/Spawner.h"
#include "SimCore/Species.h"

#include <memory>
#include <string>
#include <vector>

/**
 * The simulation side of a field map, built from Content/Data: the species (manifest order), the terrain and
 * substrate grids, the habitat with its baked features, and the spawner. Engine-independent logic lives in
 * SimCore (checked against the web build by tests/sim); this class only loads the files and holds it together.
 * Coordinates are the web build's metres: x east, z south.
 */
class FHigataField
{
public:
	bool Load(const FString& MapId, FString& OutError);

	/** Recompute the habitat's tags for the tide. */
	void SetTide(double NowMs, double TideLevelM);

	HigataSim::FSpawnEnv MakeEnv(double NowMs, double TideLevelM, double TideRateMPerH, double AmplitudeM, double SunElevationDeg) const;

	const HigataSim::FHabitat& GetHabitat() const { return *Habitat; }
	HigataSim::FSpawner& GetSpawner() { return *Spawner; }
	const std::vector<HigataSim::FSpecies>& GetSpecies() const { return Species; }
	const HigataSim::FSpecies* FindSpecies(const std::string& Id) const;

	double GroundAt(double X, double Z) const { return Terrain.HeightAt(X, Z); }
	double DepthAt(double X, double Z) const { return Habitat->DepthAt(X, Z); }
	double WaterAt(double X, double Z) const { return Habitat->WaterAt(X, Z); }
	bool Inside(double X, double Z, double Margin) const { return FMath::Abs(X) < Terrain.Half - Margin && FMath::Abs(Z) < Terrain.Half - Margin; }

	/** Nearest spot with at least MinDepth of water (rings of 0.75 m out to Radius), as the web build's nearestWater. */
	bool NearestWater(double X, double Z, double MinDepth, double Radius, double& OutX, double& OutZ) const;

	/** Species that can go in the encyclopedia by being caught (collectable, not birds). */
	int32 NumCatchable() const;

private:
	std::string MapId;
	std::vector<HigataSim::FSpecies> Species;
	HigataSim::FSimTerrain Terrain;
	std::unique_ptr<HigataSim::FHabitat> Habitat;
	std::unique_ptr<HigataSim::FSpawner> Spawner;
};
