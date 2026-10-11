#include "HigataFlat.h"

#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "TideCore/SunCore.h"
#include "TideSubsystem.h"

namespace HigataFlatLocal
{
	const FTimespan JstOffset = FTimespan::FromHours(9);
	constexpr uint64 HudKeyStatus = 0x48474131;
	constexpr uint64 HudKeyHelp = 0x48474132;
	constexpr uint64 HudKeyError = 0x48474133;
	/** How far the sea and the land around the map reach (metres). */
	constexpr double FarM = 2000.0;
	constexpr double EyeHeightM = 1.6;

	void ShowError(const FString& Text)
	{
		UE_LOG(LogTemp, Error, TEXT("[Higata] %s"), *Text);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(HudKeyError, 600.f, FColor::Red, Text);
		}
	}

	/**
	 * Adds a quad's two triangles once per winding, so the face shows from either side. The procedural meshes
	 * are built in code with no material asset to mark them two-sided, and the ground is only ever seen from above.
	 */
	void AddQuadBothSides(TArray<int32>& Tris, int32 A, int32 B, int32 C, int32 D)
	{
		Tris.Append({A, B, C, A, C, D, A, C, B, A, D, C});
	}

	/** One flat rectangle (web-space corners, metres) as a procedural mesh section. */
	void MakeFlatSection(UProceduralMeshComponent* Mesh, int32 Section, double X0, double Z0, double X1, double Z1, double Y)
	{
		const TArray<FVector> Verts = {FHigataMap::ToUnreal(X0, Z0, Y), FHigataMap::ToUnreal(X1, Z0, Y), FHigataMap::ToUnreal(X1, Z1, Y),
			FHigataMap::ToUnreal(X0, Z1, Y)};
		TArray<int32> Tris;
		AddQuadBothSides(Tris, 0, 1, 2, 3);
		const TArray<FVector> Normals = {FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector};
		Mesh->CreateMeshSection_LinearColor(Section, Verts, Tris, Normals, TArray<FVector2D>(), TArray<FLinearColor>(),
			TArray<FProcMeshTangent>(), false);
	}
}

namespace HFL = HigataFlatLocal;

AHigataFlat::AHigataFlat()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Ground = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Ground"));
	Ground->SetupAttachment(RootComponent);
	Ground->bUseAsyncCooking = true;

	Surroundings = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Surroundings"));
	Surroundings->SetupAttachment(RootComponent);

	Sea = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Sea"));
	Sea->SetupAttachment(RootComponent);
	Sea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sea->SetCastShadow(false);
}

void AHigataFlat::BeginPlay()
{
	Super::BeginPlay();

	FString Error;
	if (!FHigataMap::Load(MapId, Map, Error))
	{
		HFL::ShowError(TEXT("Map: ") + Error);
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UTideSubsystem* Tide = GameInstance ? GameInstance->GetSubsystem<UTideSubsystem>() : nullptr;
	if (!Tide || !Tide->LoadStation(Map.StationId))
	{
		HFL::ShowError(FString::Printf(TEXT("Tide: cannot load station %s (see the Output Log)"), *Map.StationId));
		return;
	}

	HideTemplateMeshes();
	BuildGround();
	BuildSurroundings();
	BuildSea();
	FindOrSpawnSun();
	bReady = true;
}

void AHigataFlat::HideTemplateMeshes()
{
	// The stock template levels put a floor and a backdrop of hills at the origin, right where the flat goes.
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		It->SetActorHiddenInGame(true);
		It->SetActorEnableCollision(false);
	}
}

UMaterialInterface* AHigataFlat::MakePlainMaterial(const FLinearColor& Color)
{
	// The engine's basic shape material has a "Color" parameter; good enough until the flat gets its own materials.
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, this);
	Mid->SetVectorParameterValue(TEXT("Color"), Color);
	return Mid;
}

void AHigataFlat::BuildGround()
{
	const int32 N = Map.N;
	const double Cell = Map.Cell();
	TArray<FVector> Verts;
	TArray<FVector> Normals;
	TArray<FVector2D> Uvs;
	Verts.Reserve(N * N);
	Normals.Reserve(N * N);
	Uvs.Reserve(N * N);
	for (int32 J = 0; J < N; ++J)
	{
		for (int32 I = 0; I < N; ++I)
		{
			const double X = -Map.Half() + I * Cell, Z = -Map.Half() + J * Cell;
			Verts.Add(FHigataMap::ToUnreal(X, Z, Map.GridHeight(I, J)));
			// Web normal (-dh/dx, 1, -dh/dz) becomes Unreal (X, Y, Z) = (-dh/dx, -dh/dz, 1).
			const double Dx = (Map.GridHeight(I + 1, J) - Map.GridHeight(I - 1, J)) / (2 * Cell);
			const double Dz = (Map.GridHeight(I, J + 1) - Map.GridHeight(I, J - 1)) / (2 * Cell);
			Normals.Add(FVector(-Dx, -Dz, 1.0).GetSafeNormal());
			Uvs.Add(FVector2D(static_cast<double>(I) / (N - 1), static_cast<double>(J) / (N - 1)));
		}
	}
	TArray<int32> Tris;
	Tris.Reserve((N - 1) * (N - 1) * 12);
	for (int32 J = 0; J < N - 1; ++J)
	{
		for (int32 I = 0; I < N - 1; ++I)
		{
			const int32 A = J * N + I;
			HFL::AddQuadBothSides(Tris, A, A + 1, A + N + 1, A + N);
		}
	}
	Ground->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, Uvs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
	// A grey-beige close to the web build's wet sand.
	Ground->SetMaterial(0, MakePlainMaterial(FLinearColor(0.30f, 0.28f, 0.24f)));
}

void AHigataFlat::BuildSurroundings()
{
	// Until the map has real surroundings: land continuing west at the west edge's height, and a sea bed
	// at the map's lowest height everywhere else, so the edges do not hang over nothing.
	double WestEdge = 0.0;
	for (int32 J = 0; J < Map.N; ++J)
	{
		WestEdge += Map.GridHeight(0, J);
	}
	WestEdge /= Map.N;
	const double H = Map.Half();
	HFL::MakeFlatSection(Surroundings, 0, -HFL::FarM, -HFL::FarM, -H, HFL::FarM, WestEdge);
	HFL::MakeFlatSection(Surroundings, 1, -HFL::FarM, -HFL::FarM, HFL::FarM, HFL::FarM, Map.MinHeightM - 0.05);
	Surroundings->SetMaterial(0, MakePlainMaterial(FLinearColor(0.22f, 0.24f, 0.18f)));
	Surroundings->SetMaterial(1, MakePlainMaterial(FLinearColor(0.20f, 0.19f, 0.16f)));
}

void AHigataFlat::BuildSea()
{
	// A level sheet moved up and down with the tide (Tick sets its height).
	HFL::MakeFlatSection(Sea, 0, -HFL::FarM, -HFL::FarM, HFL::FarM, HFL::FarM, 0.0);
	Sea->SetMaterial(0, MakePlainMaterial(FLinearColor(0.02f, 0.07f, 0.08f)));
}

void AHigataFlat::FindOrSpawnSun()
{
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		SunLight = *It;
		break;
	}
	if (!SunLight)
	{
		SunLight = GetWorld()->SpawnActor<ADirectionalLight>();
		if (SunLight)
		{
			if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
			{
				Light->SetAtmosphereSunLight(true);
			}
		}
	}
	if (SunLight && SunLight->GetRootComponent())
	{
		SunLight->GetRootComponent()->SetMobility(EComponentMobility::Movable);
	}
}

void AHigataFlat::PlacePlayer()
{
	APlayerController* Pc = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = Pc ? Pc->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	const double GroundM = Map.HeightAt(Map.SpawnX, Map.SpawnZ);
	Pawn->SetActorLocation(FHigataMap::ToUnreal(Map.SpawnX, Map.SpawnZ, GroundM + HFL::EyeHeightM), false, nullptr, ETeleportType::TeleportPhysics);
	const double H = FMath::DegreesToRadians(Map.SpawnHeadingDeg);
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(-FMath::Cos(H), -FMath::Sin(H)));
	Pc->SetControlRotation(FRotator(-8.0, Yaw, 0.0));
	bPlacedPlayer = true;
}

void AHigataFlat::HandleClockKeys()
{
	APlayerController* Pc = GetWorld()->GetFirstPlayerController();
	if (!Pc)
	{
		return;
	}
	if (Pc->WasInputKeyJustPressed(EKeys::T))
	{
		++ClockOffsetHours;
	}
	if (Pc->WasInputKeyJustPressed(EKeys::G))
	{
		--ClockOffsetHours;
	}
	if (Pc->WasInputKeyJustPressed(EKeys::R))
	{
		ClockOffsetHours = 0;
	}
}

void AHigataFlat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bReady)
	{
		return;
	}
	if (!bPlacedPlayer)
	{
		PlacePlayer();
	}
	HandleClockKeys();

	const FDateTime Utc = FDateTime::UtcNow() + FTimespan::FromHours(ClockOffsetHours);
	const double Ms = UTideSubsystem::ToEpochMs(Utc);

	UTideSubsystem* Tide = GetGameInstance()->GetSubsystem<UTideSubsystem>();
	const double Level = Tide->GetLevelAt(Utc);
	const bool bRising = Tide->GetRateAt(Utc) >= 0;
	Sea->SetRelativeLocation(FVector(0.0, 0.0, Level * 100.0));

	const HigataTide::FSunPosition SunPos = HigataTide::ComputeSunPosition(Ms, Map.OriginLat, Map.OriginLon);
	if (SunLight)
	{
		// Toward the sun in web space is (sin az cos el, sin el, -cos az cos el); in Unreal (X east, Y south, Z up):
		const double El = FMath::DegreesToRadians(SunPos.Elevation), Az = FMath::DegreesToRadians(SunPos.Azimuth);
		const FVector ToSun(FMath::Sin(Az) * FMath::Cos(El), -FMath::Cos(Az) * FMath::Cos(El), FMath::Sin(El));
		SunLight->SetActorRotation((-ToSun).Rotation());
	}

	if (GEngine)
	{
		const FString Offset = ClockOffsetHours == 0 ? TEXT("now") : FString::Printf(TEXT("now %+d h"), ClockOffsetHours);
		GEngine->AddOnScreenDebugMessage(HFL::HudKeyStatus, 1.f, FColor::Cyan,
			FString::Printf(TEXT("Hashirimizu  %s JST (%s)   tide %+.2f m %s   sun %.0f deg"), *(Utc + HFL::JstOffset).ToString(TEXT("%m/%d %H:%M")),
				*Offset, Level, bRising ? TEXT("rising") : TEXT("falling"), SunPos.Elevation));
		GEngine->AddOnScreenDebugMessage(HFL::HudKeyHelp, 1.f, FColor::White,
			TEXT("[T] +1 hour   [G] -1 hour   [R] now      WASD + mouse: fly   E / Q: up / down"));
	}
}
