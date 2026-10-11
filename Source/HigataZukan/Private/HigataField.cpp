#include "HigataField.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SimCore/JstTime.h"
#include "SimCore/Json.h"

namespace HigataFieldLocal
{
	FString DataDir()
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"));
	}

	bool ReadBytes(const FString& Relative, std::vector<uint8_t>& Out)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *FPaths::Combine(DataDir(), Relative)))
		{
			return false;
		}
		Out.assign(Bytes.GetData(), Bytes.GetData() + Bytes.Num());
		return true;
	}

	bool ReadJson(const FString& Relative, HigataSim::FJson& Out, FString& OutError)
	{
		std::vector<uint8_t> Bytes;
		if (!ReadBytes(Relative, Bytes))
		{
			OutError = FString::Printf(TEXT("cannot read Content/Data/%s"), *Relative);
			return false;
		}
		std::string Text(Bytes.begin(), Bytes.end());
		if (Text.size() >= 3 && static_cast<unsigned char>(Text[0]) == 0xEF)
		{
			Text.erase(0, 3);
		}
		std::string Error;
		if (!HigataSim::FJson::Parse(Text, Out, Error))
		{
			OutError = FString::Printf(TEXT("Content/Data/%s: %s"), *Relative, UTF8_TO_TCHAR(Error.c_str()));
			return false;
		}
		return true;
	}
}

namespace HFD = HigataFieldLocal;

bool FHigataField::Load(const FString& InMapId, FString& OutError)
{
	using namespace HigataSim;
	MapId = TCHAR_TO_UTF8(*InMapId);

	FJson Manifest;
	if (!HFD::ReadJson(TEXT("manifest.json"), Manifest, OutError)) return false;
	Species.clear();
	for (const std::string& Id : Manifest.Strings("species"))
	{
		FJson Json;
		if (!HFD::ReadJson(FString::Printf(TEXT("species/%s.json"), UTF8_TO_TCHAR(Id.c_str())), Json, OutError)) return false;
		FSpecies S;
		std::string Error;
		if (!ParseSpecies(Json, S, Error))
		{
			OutError = UTF8_TO_TCHAR(Error.c_str());
			return false;
		}
		Species.push_back(std::move(S));
	}

	FJson Map;
	if (!HFD::ReadJson(FString::Printf(TEXT("maps/%s.json"), *InMapId), Map, OutError)) return false;
	Terrain = FSimTerrain();
	Terrain.SetGrid(static_cast<int>(Map.NumberOr("resolution", 0)), Map.NumberOr("size_m", 0));
	const FJson* Height = Map.Get("height");
	if (Terrain.N < 2 || !Height)
	{
		OutError = TEXT("map JSON: bad resolution or no height");
		return false;
	}
	std::vector<uint8_t> Raw;
	const FString HeightFile = FPaths::ChangeExtension(UTF8_TO_TCHAR(Height->StringOr("file", "").c_str()), TEXT("u16"));
	if (!HFD::ReadBytes(TEXT("maps/") + HeightFile, Raw) || !Terrain.DecodeHeights(Raw, Height->NumberOr("min_tp_m", 0), Height->NumberOr("max_tp_m", 0)))
	{
		OutError = FString::Printf(TEXT("cannot read Content/Data/maps/%s"), *HeightFile);
		return false;
	}
	if (const FJson* Substrate = Map.Get("substrate"))
	{
		Terrain.Palette = Substrate->Strings("palette");
		const FString SubstrateFile = FPaths::ChangeExtension(UTF8_TO_TCHAR(Substrate->StringOr("file", "").c_str()), TEXT("u8"));
		if (!HFD::ReadBytes(TEXT("maps/") + SubstrateFile, Terrain.Substrate) || Terrain.Substrate.size() != Terrain.Heights.size())
		{
			Terrain.Substrate.assign(Terrain.Heights.size(), 0);
		}
	}
	const double Coarse = Map.Get("habitat") ? Map.Get("habitat")->NumberOr("coarse_m", 5) : 5;
	Habitat = std::make_unique<FHabitat>(Terrain, Coarse);
	std::vector<uint8_t> Features;
	if (HFD::ReadBytes(FString::Printf(TEXT("maps/%s/features.u8"), *InMapId), Features) && !Habitat->SetFeatureBits(Features))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Higata] maps/%s/features.u8 does not match the habitat grid; no eelgrass tags"), *InMapId);
	}
	Spawner = std::make_unique<FSpawner>(*Habitat, Species);
	return true;
}

void FHigataField::SetTide(double NowMs, double TideLevelM)
{
	Habitat->Update(NowMs, TideLevelM);
}

HigataSim::FSpawnEnv FHigataField::MakeEnv(double NowMs, double TideLevelM, double TideRateMPerH, double AmplitudeM, double SunElevationDeg) const
{
	using namespace HigataSim;
	FSpawnEnv Env;
	Env.Tod = TimeOfDayOf(SunElevationDeg, JstParts(NowMs).Hour);
	Env.Season = SeasonOf(NowMs);
	Env.TidePhase = TidePhaseOf(TideLevelM, TideRateMPerH, AmplitudeM);
	Env.MapId = MapId;
	Env.GameMs = NowMs;
	Env.Day = FMath::FloorToDouble(NowMs / 86400000.0);
	return Env;
}

const HigataSim::FSpecies* FHigataField::FindSpecies(const std::string& Id) const
{
	for (const HigataSim::FSpecies& S : Species)
	{
		if (S.Id == Id) return &S;
	}
	return nullptr;
}

bool FHigataField::NearestWater(double X, double Z, double MinDepth, double Radius, double& OutX, double& OutZ) const
{
	const double Step = 0.75;
	const int Rays = 12;
	for (int Ring = 1; Ring * Step <= Radius; ++Ring)
	{
		double BestD = MinDepth;
		bool bFound = false;
		for (int R = 0; R < Rays; ++R)
		{
			const double Ang = (static_cast<double>(R) / Rays) * UE_DOUBLE_PI * 2 + Ring * 0.26;
			const double Px = X + FMath::Sin(Ang) * Step * Ring, Pz = Z + FMath::Cos(Ang) * Step * Ring;
			if (!Inside(Px, Pz, 2.0)) continue;
			const double D = DepthAt(Px, Pz);
			if (D >= BestD)
			{
				BestD = D;
				OutX = Px;
				OutZ = Pz;
				bFound = true;
			}
		}
		if (bFound) return true;
	}
	return false;
}

int32 FHigataField::NumCatchable() const
{
	int32 Count = 0;
	for (const HigataSim::FSpecies& S : Species)
	{
		if (S.bCollectable && S.Group != "bird") ++Count;
	}
	return Count;
}
