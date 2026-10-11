#include "SimCore/Rng.h"

#include <cmath>
#include <cstdio>

namespace HigataSim
{
	int32_t JsFloorToInt32(double X)
	{
		if (!std::isfinite(X))
		{
			return 0;
		}
		double M = std::fmod(std::floor(X), 4294967296.0);
		if (M < 0)
		{
			M += 4294967296.0;
		}
		return static_cast<int32_t>(static_cast<uint32_t>(M));
	}

	uint32_t HashInts(const std::vector<double>& Xs)
	{
		uint32_t H = 2166136261u;
		for (double X : Xs)
		{
			uint32_t V = static_cast<uint32_t>(JsFloorToInt32(X));
			for (int I = 0; I < 4; ++I)
			{
				H ^= V & 0xffu;
				H *= 16777619u;
				V >>= 8;
			}
		}
		H ^= H >> 13;
		H *= 0x5bd1e995u;
		H ^= H >> 15;
		return H;
	}

	uint32_t HashInts(std::initializer_list<double> Xs)
	{
		return HashInts(std::vector<double>(Xs));
	}

	FRng::FRng(double Seed)
		: A(static_cast<uint32_t>(JsFloorToInt32(Seed)))
	{
	}

	double FRng::Next()
	{
		A += 0x6d2b79f5u;
		uint32_t T = A;
		T = (T ^ (T >> 15)) * (T | 1u);
		T ^= T + (T ^ (T >> 7)) * (T | 61u);
		return static_cast<double>(T ^ (T >> 14)) / 4294967296.0;
	}

	int FRng::Int(int Min, int MaxInclusive)
	{
		return static_cast<int>(std::floor(Range(Min, MaxInclusive + 1.0)));
	}

	double FRng::Normal()
	{
		double U = 0, V = 0;
		while (U == 0)
		{
			U = Next();
		}
		while (V == 0)
		{
			V = Next();
		}
		return std::sqrt(-2 * std::log(U)) * std::cos(2 * 3.14159265358979323846 * V);
	}

	std::string Hex8(uint32_t V)
	{
		char Buf[16];
		std::snprintf(Buf, sizeof Buf, "%08x", V);
		return Buf;
	}
}
