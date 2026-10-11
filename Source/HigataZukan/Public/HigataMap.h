#pragma once

#include "CoreMinimal.h"

/**
 * A field map from Content/Data/maps/<id>.json plus its raw height grid (<height file>.u16, written by
 * tools/convert_map_png.py from the web build's height.png).
 *
 * Coordinates are the web build's, in metres: x east, z south, origin at the map centre. Unreal is X east,
 * Y south, Z up in centimetres (see ToUnreal), which keeps the same handedness, so nothing is mirrored.
 */
struct HIGATAZUKAN_API FHigataMap
{
	FString Id;
	FString StationId;
	double OriginLat = 0.0;
	double OriginLon = 0.0;
	double SizeM = 0.0;
	int32 N = 0;
	double MinHeightM = 0.0;
	double MaxHeightM = 0.0;
	/** N * N heights in metres (T.P., about mean sea level), row 0 = north edge, column 0 = west edge. */
	TArray<float> Heights;
	double SpawnX = 0.0;
	double SpawnZ = 0.0;
	/** Web convention: the player looks along (-sin h, -cos h) in (x, z). */
	double SpawnHeadingDeg = 0.0;

	static bool Load(const FString& MapId, FHigataMap& Out, FString& Error);

	double Cell() const { return SizeM / (N - 1); }
	double Half() const { return SizeM / 2; }
	float GridHeight(int32 I, int32 J) const { return Heights[FMath::Clamp(J, 0, N - 1) * N + FMath::Clamp(I, 0, N - 1)]; }

	/** Bilinear height (metres) at a web-space point, clamped to the map edge. */
	double HeightAt(double X, double Z) const;

	static FVector ToUnreal(double X, double Z, double Y) { return FVector(X * 100.0, Z * 100.0, Y * 100.0); }
};
