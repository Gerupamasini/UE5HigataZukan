// Checks the simulation core against the web build: for each scenario (time, player position) the tide level and
// the spawn environment, the habitat tags, and every spawn request with the individual it becomes.
// Fixtures: tests/sim/fixtures/*.txt, written by tools/golden/spawn_golden.test.ts in the web build.

#include "SimCore/Habitat.h"
#include "SimCore/Individual.h"
#include "SimCore/JstTime.h"
#include "SimCore/Json.h"
#include "SimCore/SimTerrain.h"
#include "SimCore/Spawner.h"
#include "SimCore/Species.h"
#include "TideCore/SunCore.h"
#include "TideCore/TideCore.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using namespace HigataSim;

namespace
{
	int Failures = 0;
	int Checks = 0;

	void Check(bool bOk, const std::string& What)
	{
		++Checks;
		if (!bOk && Failures++ < 30) std::fprintf(stderr, "FAIL %s\n", What.c_str());
	}

	bool ReadBytes(const std::string& Path, std::vector<uint8_t>& Out)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In) return false;
		Out.assign(std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>());
		return true;
	}

	bool Near(double A, double B, double Tol = 1e-9) { return std::fabs(A - B) <= Tol * std::max(1.0, std::fabs(B)); }

	std::vector<std::string> Split(const std::string& S, char Sep)
	{
		std::vector<std::string> Out;
		std::string Cur;
		for (char C : S)
		{
			if (C == Sep)
			{
				Out.push_back(Cur);
				Cur.clear();
			}
			else Cur += C;
		}
		Out.push_back(Cur);
		return Out;
	}

	std::string Join(const std::vector<std::string>& V, const char* Sep)
	{
		std::string Out;
		for (size_t I = 0; I < V.size(); ++I) Out += (I ? Sep : "") + V[I];
		return Out;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::fprintf(stderr, "usage: %s <Content/Data> <fixture.txt>\n", Argv[0]);
		return 2;
	}
	const std::string Data = std::string(Argv[1]) + "/";
	std::string Error;

	// species, in the manifest's order (the spawner walks them in that order)
	FJson Manifest;
	if (!FJson::ParseFile(Data + "manifest.json", Manifest, Error)) { std::fprintf(stderr, "%s\n", Error.c_str()); return 2; }
	std::vector<FSpecies> Species;
	for (const std::string& Id : Manifest.Strings("species"))
	{
		FJson J;
		FSpecies S;
		if (!FJson::ParseFile(Data + "species/" + Id + ".json", J, Error) || !ParseSpecies(J, S, Error)) { std::fprintf(stderr, "%s\n", Error.c_str()); return 2; }
		Species.push_back(S);
	}

	FJson Map;
	if (!FJson::ParseFile(Data + "maps/hashirimizu.json", Map, Error)) { std::fprintf(stderr, "%s\n", Error.c_str()); return 2; }
	FSimTerrain Terrain;
	Terrain.SetGrid(static_cast<int>(Map.NumberOr("resolution", 0)), Map.NumberOr("size_m", 0));
	const FJson* Height = Map.Get("height");
	std::vector<uint8_t> Raw;
	if (!Height || !ReadBytes(Data + "maps/hashirimizu/height.u16", Raw) || !Terrain.DecodeHeights(Raw, Height->NumberOr("min_tp_m", 0), Height->NumberOr("max_tp_m", 0)))
	{
		std::fprintf(stderr, "cannot read the height grid\n");
		return 2;
	}
	if (!ReadBytes(Data + "maps/hashirimizu/substrate.u8", Terrain.Substrate)) { std::fprintf(stderr, "cannot read substrate\n"); return 2; }
	Terrain.Palette = Map.Get("substrate")->Strings("palette");
	const double CoarseM = Map.Get("habitat") ? Map.Get("habitat")->NumberOr("coarse_m", 5) : 5;
	FHabitat Habitat(Terrain, CoarseM);
	std::vector<uint8_t> Features;
	Check(ReadBytes(Data + "maps/hashirimizu/features.u8", Features) && Habitat.SetFeatureBits(Features), "features.u8 matches the coarse grid");

	FJson StationJson;
	if (!FJson::ParseFile(Data + "tide/stations/" + Map.StringOr("station", "") + ".json", StationJson, Error)) { std::fprintf(stderr, "%s\n", Error.c_str()); return 2; }
	HigataTide::FStation Station;
	Station.Id = StationJson.StringOr("id", "");
	Station.PhaseReference = StationJson.StringOr("phaseReference", "UTC") == "JST135E" ? HigataTide::EPhaseReference::JST135E : HigataTide::EPhaseReference::UTC;
	for (const FJson& C : StationJson.Get("constituents")->Items)
	{
		Station.Constituents.push_back({C.StringOr("name", ""), C.NumberOr("amplitude_cm", 0), C.NumberOr("phase_deg", 0)});
	}
	const HigataTide::FTideModel Tide(Station);
	const double Lat = Map.Get("origin")->NumberOr("lat", 0), Lon = Map.Get("origin")->NumberOr("lon", 0);

	FSpawner Spawner(Habitat, Species);
	std::ifstream In(Argv[2]);
	if (!In) { std::fprintf(stderr, "cannot open %s\n", Argv[2]); return 2; }

	std::vector<FSpawnRequest> Got;
	size_t Next = 0;
	std::string Scenario;
	FSpawnEnv Env;
	auto FinishScenario = [&]()
	{
		if (Scenario.empty()) return;
		Check(Next == Got.size(), Scenario + ": " + std::to_string(Got.size()) + " requests, fixture has " + std::to_string(Next));
	};

	int Requests = 0;
	for (std::string Line; std::getline(In, Line);)
	{
		if (Line.empty() || Line[0] == '#') continue;
		std::istringstream S(Line);
		std::string Tag;
		S >> Tag;
		if (Tag == "scenario")
		{
			FinishScenario();
			int K = 0;
			double Px = 0, Pz = 0, MinDist = 0, Level = 0, Day = 0;
			S >> K >> Env.GameMs >> Px >> Pz >> MinDist >> Level >> Env.Tod >> Env.Season >> Env.TidePhase >> Day;
			Scenario = "scenario " + std::to_string(K);
			Env.MapId = "hashirimizu";
			Env.Day = Day;

			// the environment as the game computes it, from the C++ tide, sun and calendar
			const double MyLevel = Tide.Level(Env.GameMs);
			Check(Near(MyLevel, Level), Scenario + " tide level");
			const HigataTide::FSunPosition Sun = HigataTide::ComputeSunPosition(Env.GameMs, Lat, Lon);
			Check(TimeOfDayOf(Sun.Elevation, JstParts(Env.GameMs).Hour) == Env.Tod, Scenario + " time of day");
			Check(SeasonOf(Env.GameMs) == Env.Season, Scenario + " season");
			Check(TidePhaseOf(MyLevel, Tide.Rate(Env.GameMs), Tide.MaxAmplitude()) == Env.TidePhase, Scenario + " tide phase");
			Check(std::floor(Env.GameMs / 86400000.0) == Day, Scenario + " day");

			Habitat.Update(Env.GameMs, Level);
			Got = Spawner.Plan(Px, Pz, Env, {}, MinDist, 1.0);
			Next = 0;
		}
		else if (Tag == "tags")
		{
			std::string Rest;
			std::getline(S, Rest);
			const std::vector<std::string> Cells = Split(Rest.substr(1), ' ');
			Check(Cells.size() == Habitat.Tags.size(), Scenario + " tag cell count");
			for (size_t K = 0; K < Cells.size() && K < Habitat.Tags.size(); ++K)
			{
				const std::string Mine = Habitat.Tags[K].empty() ? "-" : Join(Habitat.Tags[K], ",");
				Check(Mine == Cells[K], Scenario + " tags of cell " + std::to_string(K) + ": " + Mine + " != " + Cells[K]);
			}
		}
		else if (Tag == "req")
		{
			++Requests;
			std::string SpeciesId, Id, Sex, Stage, Traits;
			int Rule = 0, Cell = 0, Gravid = 0, Dress = 0;
			double Seed = 0, X = 0, Z = 0, Length = 0, Weight = 0, Heading = 0;
			S >> SpeciesId >> Rule >> Cell >> Seed >> X >> Z >> Id >> Length >> Weight >> Sex >> Stage >> Traits >> Gravid >> Dress >> Heading;
			const std::string What = Scenario + " request " + std::to_string(Next) + " (" + Id + ")";
			if (Next >= Got.size())
			{
				Check(false, What + ": missing");
				++Next;
				continue;
			}
			const FSpawnRequest& R = Got[Next++];
			Check(R.Species->Id == SpeciesId && R.RuleIndex == Rule && R.Cell == Cell && R.Seed == static_cast<uint32_t>(Seed), What + " species/rule/cell/seed: got " + R.Species->Id + " " + std::to_string(R.RuleIndex) + " " + std::to_string(R.Cell) + " " + std::to_string(R.Seed));
			Check(Near(R.X, X) && Near(R.Z, Z), What + " position");
			const FIndividual Ind = GenerateIndividual(*R.Species, R.Seed, R.X, R.Z, R.Cell, R.RuleIndex, Env.GameMs, R.bHasLengthRange, R.Length0, R.Length1);
			Check(Ind.Id == Id, What + " id " + Ind.Id);
			Check(Near(Ind.LengthMm, Length) && Near(Ind.WeightG, Weight), What + " length/weight");
			Check(std::string(1, Ind.Sex) == Sex && Ind.Stage == Stage, What + " sex/stage");
			Check((Ind.Traits.empty() ? "-" : Join(Ind.Traits, ",")) == Traits, What + " traits");
			Check(Ind.bGravid == (Gravid != 0) && Ind.bDress == (Dress != 0), What + " gravid/dress");
			Check(Near(Ind.Heading, Heading), What + " heading");
		}
	}
	FinishScenario();
	std::printf("%d requests, %d checks, %d failures\n", Requests, Checks, Failures);
	return Failures == 0 && Requests > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
