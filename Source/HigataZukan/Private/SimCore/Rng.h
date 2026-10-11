// Deterministic hashing and random numbers, ported from the web build (src/core/Rng.ts: FNV-style hashInts and
// mulberry32). Plain C++17 (no Unreal headers) so the standalone tests in tests/sim build it too.
//
// JavaScript works these in 32-bit integers through ToInt32 / >>> 0; the port keeps the same bits.

#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace HigataSim
{
	/** JavaScript's ToInt32 of Math.floor(x) (what `Math.floor(x) | 0` gives). */
	int32_t JsFloorToInt32(double X);

	/** hashInts(...xs): FNV-1a over each value's four low bytes, then a final mix. */
	uint32_t HashInts(std::initializer_list<double> Xs);
	uint32_t HashInts(const std::vector<double>& Xs);

	/** mulberry32, seeded with ToUint32(seed). */
	class FRng
	{
	public:
		explicit FRng(double Seed);
		/** [0, 1) */
		double Next();
		double Range(double Min, double Max) { return Min + (Max - Min) * Next(); }
		int Int(int Min, int MaxInclusive);
		/** Standard normal via Box-Muller. */
		double Normal();
		bool Chance(double P) { return Next() < P; }

	private:
		uint32_t A;
	};

	/** The hex id suffix the web build uses: hashInts(seed, 7) as 8 lowercase hex digits. */
	std::string Hex8(uint32_t V);
}
