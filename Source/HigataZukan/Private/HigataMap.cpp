#include "HigataMap.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool FHigataMap::Load(const FString& MapId, FHigataMap& Out, FString& Error)
{
	const FString MapsDir = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/maps"));
	const FString JsonPath = FPaths::Combine(MapsDir, MapId + TEXT(".json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *JsonPath))
	{
		Error = FString::Printf(TEXT("cannot read %s"), *JsonPath);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		Error = FString::Printf(TEXT("%s is not valid JSON"), *JsonPath);
		return false;
	}

	const TSharedPtr<FJsonObject>* Origin = nullptr;
	const TSharedPtr<FJsonObject>* Height = nullptr;
	const TSharedPtr<FJsonObject>* Spawn = nullptr;
	FString HeightFile;
	if (!Root->TryGetStringField(TEXT("id"), Out.Id) || !Root->TryGetStringField(TEXT("station"), Out.StationId)
		|| !Root->TryGetNumberField(TEXT("size_m"), Out.SizeM) || !Root->TryGetNumberField(TEXT("resolution"), Out.N)
		|| !Root->TryGetObjectField(TEXT("origin"), Origin) || !Root->TryGetObjectField(TEXT("height"), Height)
		|| !Root->TryGetObjectField(TEXT("spawnStart"), Spawn)
		|| !(*Origin)->TryGetNumberField(TEXT("lat"), Out.OriginLat) || !(*Origin)->TryGetNumberField(TEXT("lon"), Out.OriginLon)
		|| !(*Height)->TryGetStringField(TEXT("file"), HeightFile) || !(*Height)->TryGetNumberField(TEXT("min_tp_m"), Out.MinHeightM)
		|| !(*Height)->TryGetNumberField(TEXT("max_tp_m"), Out.MaxHeightM)
		|| !(*Spawn)->TryGetNumberField(TEXT("x"), Out.SpawnX) || !(*Spawn)->TryGetNumberField(TEXT("z"), Out.SpawnZ)
		|| !(*Spawn)->TryGetNumberField(TEXT("heading"), Out.SpawnHeadingDeg))
	{
		Error = FString::Printf(TEXT("%s: missing fields (id, station, size_m, resolution, origin, height, spawnStart)"), *JsonPath);
		return false;
	}
	if (Out.N < 2 || Out.SizeM <= 0)
	{
		Error = FString::Printf(TEXT("%s: bad size or resolution"), *JsonPath);
		return false;
	}

	const FString RawPath = FPaths::Combine(MapsDir, FPaths::ChangeExtension(HeightFile, TEXT("u16")));
	TArray<uint8> Raw;
	if (!FFileHelper::LoadFileToArray(Raw, *RawPath) || Raw.Num() != Out.N * Out.N * 2)
	{
		Error = FString::Printf(TEXT("cannot read %s as %d x %d uint16 (run tools/convert_map_png.py)"), *RawPath, Out.N, Out.N);
		return false;
	}
	Out.Heights.SetNumUninitialized(Out.N * Out.N);
	const double Span = Out.MaxHeightM - Out.MinHeightM;
	for (int32 K = 0; K < Out.N * Out.N; ++K)
	{
		const uint16 V = static_cast<uint16>(Raw[K * 2] | (Raw[K * 2 + 1] << 8));
		Out.Heights[K] = static_cast<float>(Out.MinHeightM + (V / 65535.0) * Span);
	}

	// Optional substrate grid: "substrate": {"file": ".../substrate.png", "palette": [...]}, raw indices in .u8.
	const TSharedPtr<FJsonObject>* SubstrateObj = nullptr;
	FString SubstrateFile;
	const TArray<TSharedPtr<FJsonValue>>* Palette = nullptr;
	if (Root->TryGetObjectField(TEXT("substrate"), SubstrateObj) && (*SubstrateObj)->TryGetStringField(TEXT("file"), SubstrateFile)
		&& (*SubstrateObj)->TryGetArrayField(TEXT("palette"), Palette))
	{
		for (const TSharedPtr<FJsonValue>& Name : *Palette)
		{
			Out.SubstratePalette.Add(Name.IsValid() ? Name->AsString() : FString());
		}
		const FString SubstratePath = FPaths::Combine(MapsDir, FPaths::ChangeExtension(SubstrateFile, TEXT("u8")));
		if (!FFileHelper::LoadFileToArray(Out.Substrate, *SubstratePath) || Out.Substrate.Num() != Out.N * Out.N)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Higata] %s: cannot read %d x %d substrate indices; the ground gets one colour"), *SubstratePath,
				Out.N, Out.N);
			Out.Substrate.Reset();
		}
	}
	return true;
}

FString FHigataMap::GridSubstrate(int32 I, int32 J) const
{
	if (Substrate.Num() != N * N)
	{
		return TEXT("mud");
	}
	const int32 Index = Substrate[FMath::Clamp(J, 0, N - 1) * N + FMath::Clamp(I, 0, N - 1)];
	return SubstratePalette.IsValidIndex(Index) ? SubstratePalette[Index] : FString(TEXT("mud"));
}

double FHigataMap::HeightAt(double X, double Z) const
{
	const double Gx = FMath::Clamp((X + Half()) / Cell(), 0.0, N - 1.000001);
	const double Gz = FMath::Clamp((Z + Half()) / Cell(), 0.0, N - 1.000001);
	const int32 I = FMath::FloorToInt32(Gx), J = FMath::FloorToInt32(Gz);
	const double Fx = Gx - I, Fz = Gz - J;
	const double A = GridHeight(I, J), B = GridHeight(I + 1, J), C = GridHeight(I, J + 1), D = GridHeight(I + 1, J + 1);
	return (A * (1 - Fx) + B * Fx) * (1 - Fz) + (C * (1 - Fx) + D * Fx) * Fz;
}
