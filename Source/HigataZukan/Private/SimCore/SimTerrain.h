// The ground the simulation samples: the map's height and substrate grids, sampled as the web build's Terrain
// does (bilinear heights through gridIndex, nearest substrate through Math.round). No feeding pits yet.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace HigataSim
{
	struct FSimTerrain
	{
		int N = 0;
		double Size = 0.0;
		double Half = 0.0;
		double Cell = 0.0;
		/** N * N heights (m), row 0 north, column 0 west; float like the web build's Float32Array. */
		std::vector<float> Heights;
		std::vector<uint8_t> Substrate;
		std::vector<std::string> Palette;

		/** Fill N, Size, Half and Cell. */
		void SetGrid(int InN, double InSize);

		/** Decode the raw height grid (little-endian uint16 spanning MinM..MaxM). */
		bool DecodeHeights(const std::vector<uint8_t>& Raw, double MinM, double MaxM);

		double HeightAt(double X, double Z) const;
		int SubstrateIndexAt(double X, double Z) const;
		/** The palette name, "mud" when out of range. */
		const std::string& SubstrateAt(double X, double Z) const;
		const std::string& PaletteName(int Index) const;
	};

	/** JavaScript's Math.round (halves toward +infinity). */
	double JsRound(double V);

	/**
	 * JavaScript's Math.hypot for two values as V8 computes it (normalised by the larger, Kahan-summed), which can
	 * differ from std::hypot in the last bit: the spawner sorts cells by it and ties must break the same way.
	 */
	double JsHypot(double A, double B);
}
