#include "SimCore/Species.h"

#include "SimCore/Json.h"

#include <algorithm>

namespace HigataSim
{
	namespace
	{
		bool Pair(const FJson* V, double& A, double& B)
		{
			if (!V || !V->IsArray() || V->Items.size() != 2 || !V->Items[0].IsNumber() || !V->Items[1].IsNumber())
			{
				return false;
			}
			A = V->Items[0].Number;
			B = V->Items[1].Number;
			return true;
		}

		bool ParseRule(const FJson& J, FSpawnRule& R, std::string& Error)
		{
			R.Tags = J.Strings("tags");
			if (R.Tags.empty())
			{
				Error = "spawn rule without tags";
				return false;
			}
			if (J.Get("substrate")) { R.bHasSubstrate = true; R.Substrate = J.Strings("substrate"); }
			R.bHasDepth = Pair(J.Get("depth_m"), R.Depth0, R.Depth1);
			if (J.Get("time")) { R.bHasTime = true; R.Time = J.Strings("time"); }
			if (J.Get("season")) { R.bHasSeason = true; R.Season = J.Strings("season"); }
			R.Tide = J.StringOr("tide", "any");
			if (J.Get("maps")) { R.bHasMaps = true; R.Maps = J.Strings("maps"); }
			R.DensityPer100M2 = J.NumberOr("density_per_100m2", 0.0);
			double G0 = 1, G1 = 1;
			if (Pair(J.Get("group"), G0, G1))
			{
				R.Group0 = static_cast<int>(G0);
				R.Group1 = static_cast<int>(G1);
			}
			R.MaxPopulation = static_cast<int>(J.NumberOr("maxPopulation", 50));
			R.bHasLength = Pair(J.Get("length_mm"), R.Length0, R.Length1);
			return true;
		}
	}

	double FSpecies::BrainNumber(const std::string& Key, double Default) const
	{
		for (const auto& P : BrainNumbers)
		{
			if (P.first == Key) return P.second;
		}
		return Default;
	}

	bool ParseSpecies(const FJson& J, FSpecies& S, std::string& Error)
	{
		S = FSpecies();
		S.Id = J.StringOr("id", "");
		if (S.Id.empty())
		{
			Error = "species without id";
			return false;
		}
		if (const FJson* Names = J.Get("names"))
		{
			S.NameJa = Names->StringOr("ja", S.Id);
			S.NameSci = Names->StringOr("sci", "");
			S.NameEn = Names->StringOr("en", "");
		}
		if (const FJson* Taxon = J.Get("taxon"))
		{
			S.Group = Taxon->StringOr("group", "other");
		}
		S.Locomotion = J.StringOr("locomotion", "swim");
		if (const FJson* Aquatic = J.Get("aquatic"); Aquatic && Aquatic->Type == FJson::EType::Bool)
		{
			S.bHasAquatic = true;
			S.bAquatic = Aquatic->Bool;
		}
		S.bCollectable = J.BoolOr("collectable", false);
		S.bProtected = J.BoolOr("protected", false);

		if (const FJson* Model = J.Get("model"))
		{
			S.ModelLengthMm = Model->NumberOr("modelLength_mm", 0.0);
			S.ViewDistanceM = Model->NumberOr("viewDistance_m", 0.0);
			S.Placeholder = Model->StringOr("placeholder", "");
			S.Driver = Model->StringOr("driver", "");
		}

		const FJson* Size = J.Get("size");
		const FJson* Length = Size ? Size->Get("length_mm") : nullptr;
		const FJson* Weight = Size ? Size->Get("weightCoef") : nullptr;
		if (!Length || !Weight)
		{
			Error = S.Id + ": size.length_mm and size.weightCoef are required";
			return false;
		}
		S.LengthMin = Length->NumberOr("min", 0.0);
		S.LengthMax = Length->NumberOr("max", 0.0);
		S.LengthMean = Length->NumberOr("mean", 0.0);
		S.LengthSd = Length->NumberOr("sd", 1.0);
		S.WeightA = Weight->NumberOr("a", 0.0);
		S.WeightB = Weight->NumberOr("b", 3.0);
		if (const FJson* F = Size->Get("minDepthFraction"); F && F->IsNumber())
		{
			S.bHasMinDepthFraction = true;
			S.MinDepthFraction = F->Number;
		}

		if (const FJson* Sex = J.Get("sex"))
		{
			S.MaleRatio = Sex->NumberOr("maleRatio", 0.5);
		}
		if (const FJson* Breeding = J.Get("breeding"))
		{
			S.bHasBreeding = true;
			if (const FJson* Months = Breeding->Get("months"); Months && Months->IsArray())
			{
				for (const FJson& M : Months->Items)
				{
					if (M.IsNumber()) S.BreedingMonths.push_back(static_cast<int>(M.Number));
				}
			}
			S.GravidShare = Breeding->NumberOr("gravidShare", 0.7);
			S.OffSeasonShare = Breeding->NumberOr("offSeasonShare", 0.0);
			S.DressShare = Breeding->NumberOr("dressShare", 0.0);
		}

		if (const FJson* Stages = J.Get("stages"); Stages && Stages->IsArray())
		{
			for (const FJson& St : Stages->Items)
			{
				FStage Stage;
				Stage.Id = St.StringOr("id", "");
				Stage.Ja = St.StringOr("ja", "");
				if (const FJson* Max = St.Get("maxLength_mm"); Max && Max->IsNumber())
				{
					Stage.bHasMaxLength = true;
					Stage.MaxLengthMm = Max->Number;
				}
				S.Stages.push_back(Stage);
			}
		}
		if (S.Stages.empty())
		{
			Error = S.Id + ": no stages";
			return false;
		}
		if (const FJson* Traits = J.Get("traits"); Traits && Traits->IsArray())
		{
			for (const FJson& T : Traits->Items)
			{
				S.Traits.push_back({T.StringOr("id", ""), T.StringOr("ja", ""), T.StringOr("when", "")});
			}
		}
		if (const FJson* Spawn = J.Get("spawn"); Spawn && Spawn->IsArray())
		{
			for (const FJson& R : Spawn->Items)
			{
				FSpawnRule Rule;
				if (!ParseRule(R, Rule, Error))
				{
					Error = S.Id + ": " + Error;
					return false;
				}
				S.Spawn.push_back(Rule);
			}
		}
		if (const FJson* Brain = J.Get("brain"))
		{
			S.BrainTree = Brain->StringOr("tree", "");
			if (const FJson* Params = Brain->Get("params"); Params && Params->IsObject())
			{
				for (const auto& P : Params->Members)
				{
					if (P.second.IsNumber()) S.BrainNumbers.emplace_back(P.first, P.second.Number);
				}
			}
		}
		if (const FJson* Capture = J.Get("capture"))
		{
			S.CaptureTools = Capture->Strings("tools");
			S.BaseDifficulty = Capture->NumberOr("baseDifficulty", 0.5);
			S.AlertPenalty = Capture->NumberOr("alertPenalty", 0.4);
		}
		if (const FJson* Enc = J.Get("encyclopedia"))
		{
			S.Description = Enc->StringOr("description", "");
			S.HabitatHint = Enc->StringOr("habitatHint", "");
		}
		return true;
	}

	bool IsAquatic(const FSpecies& S)
	{
		if (S.bHasAquatic) return S.bAquatic;
		return S.Locomotion == "swim" || S.Group == "crustacean";
	}

	double MinDepthFor(const FSpecies& S, double LengthMm)
	{
		return std::max(0.015, (LengthMm / 1000.0) * (S.bHasMinDepthFraction ? S.MinDepthFraction : 0.15));
	}
}
