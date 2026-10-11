#include "SimCore/SimTerrain.h"

#include <algorithm>
#include <cmath>

namespace HigataSim
{
	double JsRound(double V)
	{
		return std::floor(V + 0.5);
	}

	double JsHypot(double A, double B)
	{
		A = std::fabs(A);
		B = std::fabs(B);
		const double Max = std::max(A, B);
		if (std::isinf(Max)) return Max;
		if (std::isnan(A) || std::isnan(B)) return std::nan("");
		if (Max == 0) return 0;
		double Sum = 0, Compensation = 0;
		for (const double V : {A, B})
		{
			const double N = V / Max;
			const double Summand = N * N - Compensation;
			const double Preliminary = Sum + Summand;
			Compensation = (Preliminary - Sum) - Summand;
			Sum = Preliminary;
		}
		return std::sqrt(Sum) * Max;
	}

	void FSimTerrain::SetGrid(int InN, double InSize)
	{
		N = InN;
		Size = InSize;
		Half = InSize / 2;
		Cell = InSize / (InN - 1);
	}

	bool FSimTerrain::DecodeHeights(const std::vector<uint8_t>& Raw, double MinM, double MaxM)
	{
		const size_t Count = static_cast<size_t>(N) * static_cast<size_t>(N);
		if (Raw.size() != Count * 2)
		{
			return false;
		}
		Heights.resize(Count);
		for (size_t K = 0; K < Count; ++K)
		{
			const unsigned V = static_cast<unsigned>(Raw[K * 2]) | (static_cast<unsigned>(Raw[K * 2 + 1]) << 8);
			Heights[K] = static_cast<float>(MinM + (V / 65535.0) * (MaxM - MinM));
		}
		return true;
	}

	double FSimTerrain::HeightAt(double X, double Z) const
	{
		const double Gx = (X + Half) / Cell;
		const double Gz = (Z + Half) / Cell;
		const int I = std::max(0, std::min(N - 2, static_cast<int>(std::floor(Gx))));
		const int J = std::max(0, std::min(N - 2, static_cast<int>(std::floor(Gz))));
		const double Fx = std::max(0.0, std::min(1.0, Gx - I));
		const double Fz = std::max(0.0, std::min(1.0, Gz - J));
		const double A = Heights[static_cast<size_t>(J * N + I)];
		const double B = Heights[static_cast<size_t>(J * N + I + 1)];
		const double C = Heights[static_cast<size_t>((J + 1) * N + I)];
		const double D = Heights[static_cast<size_t>((J + 1) * N + I + 1)];
		return (A * (1 - Fx) + B * Fx) * (1 - Fz) + (C * (1 - Fx) + D * Fx) * Fz;
	}

	int FSimTerrain::SubstrateIndexAt(double X, double Z) const
	{
		const int I = std::max(0, std::min(N - 1, static_cast<int>(JsRound((X + Half) / Cell))));
		const int J = std::max(0, std::min(N - 1, static_cast<int>(JsRound((Z + Half) / Cell))));
		return Substrate.empty() ? 0 : Substrate[static_cast<size_t>(J * N + I)];
	}

	const std::string& FSimTerrain::PaletteName(int Index) const
	{
		static const std::string Mud = "mud";
		return Index >= 0 && static_cast<size_t>(Index) < Palette.size() ? Palette[static_cast<size_t>(Index)] : Mud;
	}

	const std::string& FSimTerrain::SubstrateAt(double X, double Z) const
	{
		return PaletteName(SubstrateIndexAt(X, Z));
	}
}
