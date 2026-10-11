#include "SimCore/Individual.h"

#include "SimCore/JstTime.h"
#include "SimCore/Rng.h"
#include "SimCore/SimTerrain.h"
#include "SimCore/Species.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace HigataSim
{
	namespace
	{
		double Erf(double X)
		{
			const double S = X > 0 ? 1.0 : X < 0 ? -1.0 : 0.0;
			X = std::abs(X);
			const double T = 1 / (1 + 0.3275911 * X);
			const double Y = 1 - (((((1.061405429 * T - 1.453152027) * T) + 1.421413741) * T - 0.284496736) * T + 0.254829592) * T * std::exp(-X * X);
			return S * Y;
		}

		/** "name op number" with op one of >=, <=, >, <, == (the web build's evalTraitCondition). */
		bool EvalTraitCondition(const std::string& Expr, double LengthPct, double LengthMm, double WeightG, bool bGravid, bool bDress)
		{
			size_t P = 0;
			auto SkipWs = [&]() { while (P < Expr.size() && std::isspace(static_cast<unsigned char>(Expr[P]))) ++P; };
			SkipWs();
			const size_t NameStart = P;
			while (P < Expr.size() && (std::islower(static_cast<unsigned char>(Expr[P])) || Expr[P] == '_')) ++P;
			if (P == NameStart) return false;
			const std::string Name = Expr.substr(NameStart, P - NameStart);
			SkipWs();
			std::string Op;
			for (const char* Candidate : {">=", "<=", "==", ">", "<"})
			{
				const std::string C = Candidate;
				if (Expr.compare(P, C.size(), C) == 0)
				{
					Op = C;
					P += C.size();
					break;
				}
			}
			if (Op.empty()) return false;
			SkipWs();
			const size_t NumStart = P;
			if (P < Expr.size() && Expr[P] == '-') ++P;
			while (P < Expr.size() && (std::isdigit(static_cast<unsigned char>(Expr[P])) || Expr[P] == '.')) ++P;
			if (P == NumStart) return false;
			const double C = std::strtod(Expr.substr(NumStart, P - NumStart).c_str(), nullptr);
			SkipWs();
			if (P != Expr.size()) return false;

			double V = 0;
			if (Name == "length_pct") V = LengthPct;
			else if (Name == "length_mm") V = LengthMm;
			else if (Name == "weight_g") V = WeightG;
			else if (Name == "gravid") V = bGravid ? 1 : 0;
			else if (Name == "dress") V = bDress ? 1 : 0;
			else return false;
			if (Op == ">=") return V >= C;
			if (Op == "<=") return V <= C;
			if (Op == ">") return V > C;
			if (Op == "<") return V < C;
			return V == C;
		}
	}

	std::string IndividualId(const std::string& SpeciesId, uint32_t Seed)
	{
		return SpeciesId + "#" + Hex8(HashInts({static_cast<double>(Seed), 7}));
	}

	FIndividual GenerateIndividual(const FSpecies& Sp, uint32_t Seed, double X, double Z, int Cell, int RuleIndex, double NowMs,
		bool bHasLengthRange, double Length0, double Length1)
	{
		FRng Rng(Seed);
		double Len = Sp.LengthMean + Sp.LengthSd * Rng.Normal();
		Len = std::max(Sp.LengthMin, std::min(Sp.LengthMax, Len));
		if (bHasLengthRange)
		{
			Len = std::max(Sp.LengthMin, std::min(Sp.LengthMax, Length0 + (Length1 - Length0) * Rng.Next()));
		}
		const double Pct = 50 * (1 + Erf((Len - Sp.LengthMean) / (Sp.LengthSd * std::sqrt(2.0))));
		const double Weight = Sp.WeightA * std::pow(Len, Sp.WeightB);

		FIndividual Ind;
		Ind.Species = &Sp;
		Ind.Seed = Seed;
		Ind.Sex = Rng.Chance(Sp.MaleRatio) ? 'm' : 'f';
		Ind.Stage = Sp.Stages.back().Id;
		for (const FStage& St : Sp.Stages)
		{
			if (St.bHasMaxLength && Len <= St.MaxLengthMm)
			{
				Ind.Stage = St.Id;
				break;
			}
		}
		const bool bAdult = Ind.Stage == Sp.Stages.back().Id;
		const int Month = JstParts(NowMs).Month;
		const bool bInSeason = Sp.bHasBreeding && std::find(Sp.BreedingMonths.begin(), Sp.BreedingMonths.end(), Month) != Sp.BreedingMonths.end();
		Ind.bGravid = Sp.bHasBreeding && Ind.Sex == 'f' && bAdult && Rng.Chance(bInSeason ? Sp.GravidShare : Sp.OffSeasonShare);
		Ind.bDress = Sp.bHasBreeding && Sp.DressShare > 0 && Ind.Sex == 'm' && bAdult && bInSeason && Rng.Chance(Sp.DressShare);
		for (const FTrait& Tr : Sp.Traits)
		{
			if (EvalTraitCondition(Tr.When, Pct, Len, Weight, Ind.bGravid, Ind.bDress)) Ind.Traits.push_back(Tr.Id);
		}
		Ind.Id = IndividualId(Sp.Id, Seed);
		Ind.X = X;
		Ind.Z = Z;
		Ind.Heading = Rng.Range(0, 3.14159265358979323846 * 2);
		Ind.LengthMm = JsRound(Len * 10) / 10;
		Ind.WeightG = JsRound(Weight * 10) / 10;
		Ind.LengthPct = Pct;
		Ind.Wariness = 0.7 + 0.6 * std::max(0.0, std::min(100.0, Pct)) / 100;
		Ind.Energy = Rng.Range(0.3, 0.9);
		Ind.Cell = Cell;
		Ind.RuleIndex = RuleIndex;
		return Ind;
	}
}
