#include "HigataCreature.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HigataField.h"
#include "HigataMap.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SimCore/Species.h"

namespace HigataCreatureLocal
{
	/** Stand-in body proportions (length, width, height as shares of the length) by species. */
	struct FShape
	{
		const TCHAR* Mesh;
		double W;
		double H;
		/** Lies on the bottom (else swims in mid water). */
		bool bBottom;
	};

	FShape ShapeOf(const HigataSim::FSpecies& S)
	{
		const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");
		const std::string& Id = S.Id;
		if (Id == "hemitrygon_akajei") return {Sphere, 1.1, 0.12, true};        // the disc (its length here is the disc's)
		if (Id == "platichthys_bicoloratus") return {Sphere, 0.55, 0.09, true};
		if (Id == "syngnathus_schlegeli") return {Sphere, 0.045, 0.05, false};
		if (Id == "rudarius_ercodes") return {Sphere, 0.25, 0.55, false};
		if (Id == "mugil_cephalus") return {Sphere, 0.18, 0.2, false};
		if (Id == "meretrix_lusoria" || Id == "ruditapes_philippinarum") return {Sphere, 0.8, 0.45, true};
		if (Id == "pagurus_minutus") return {Sphere, 0.85, 0.75, true};
		if (Id == "reticunassa_festiva") return {Sphere, 0.6, 0.55, true};
		if (S.Group == "crustacean") return {Sphere, 0.18, 0.18, true};
		return {Sphere, 0.2, 0.18, true};                                       // gobies and the like
	}

	FLinearColor ColorOf(const std::string& Id)
	{
		if (Id == "acanthogobius_flavimanus") return FLinearColor(0.30f, 0.26f, 0.18f);
		if (Id == "gymnogobius_macrognathos") return FLinearColor(0.26f, 0.23f, 0.17f);
		if (Id == "favonigobius_gymnauchen") return FLinearColor(0.42f, 0.39f, 0.31f);
		if (Id == "exopalaemon_orientis" || Id == "palaemon_pacificus") return FLinearColor(0.55f, 0.55f, 0.50f);
		if (Id == "pagurus_minutus") return FLinearColor(0.33f, 0.28f, 0.23f);
		if (Id == "reticunassa_festiva") return FLinearColor(0.38f, 0.33f, 0.26f);
		if (Id == "mugil_cephalus") return FLinearColor(0.60f, 0.62f, 0.65f);
		if (Id == "rudarius_ercodes") return FLinearColor(0.33f, 0.28f, 0.14f);
		if (Id == "syngnathus_schlegeli") return FLinearColor(0.28f, 0.31f, 0.18f);
		if (Id == "platichthys_bicoloratus") return FLinearColor(0.33f, 0.30f, 0.23f);
		if (Id == "hemitrygon_akajei") return FLinearColor(0.24f, 0.19f, 0.14f);
		if (Id == "meretrix_lusoria") return FLinearColor(0.58f, 0.53f, 0.43f);
		return FLinearColor(0.4f, 0.4f, 0.4f);
	}
}

namespace HCL = HigataCreatureLocal;

AHigataCreature::AHigataCreature()
{
	PrimaryActorTick.bCanEverTick = false;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetMobility(EComponentMobility::Movable);
}

void AHigataCreature::Init(const HigataSim::FIndividual& InIndividual, const FHigataField* InField, UMaterialInstanceDynamic* InMaterial)
{
	Individual = InIndividual;
	Field = InField;
	const HigataSim::FSpecies& Sp = *Individual.Species;
	X = Individual.X;
	Z = Individual.Z;
	Heading = Individual.Heading;
	LengthM = Individual.LengthMm / 1000.0;
	bAquatic = HigataSim::IsAquatic(Sp);
	bBurrowed = Sp.Locomotion == "burrow";
	MinDepth = HigataSim::MinDepthFor(Sp, Individual.LengthMm);
	FleeDistance = Sp.BrainNumber("fleeDistance_m", 1.2) * Individual.Wariness;

	// pace in body lengths a second, with floors so the small ones still get somewhere
	if (Sp.Locomotion == "swim")
	{
		CruiseSpeed = FMath::Max(0.06, 1.5 * LengthM);
		FleeSpeed = FMath::Max(0.6, 8.0 * LengthM);
	}
	else if (Sp.Group == "crustacean")
	{
		CruiseSpeed = FMath::Max(0.02, 0.8 * LengthM);
		FleeSpeed = FMath::Max(0.3, 6.0 * LengthM);
	}
	else
	{
		CruiseSpeed = FMath::Max(0.004, 0.4 * LengthM);
		FleeSpeed = CruiseSpeed;
	}

	const HCL::FShape Shape = HCL::ShapeOf(Sp);
	bBottom = Shape.bBottom;
	BodyScale = FVector(LengthM, LengthM * Shape.W, LengthM * Shape.H);
	BodyLift = bBurrowed ? 0.0 : LengthM * Shape.H * 0.5;
	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Shape.Mesh))
	{
		Body->SetStaticMesh(Mesh);
	}
	if (InMaterial)
	{
		InMaterial->SetVectorParameterValue(TEXT("Color"), HCL::ColorOf(Sp.Id));
		Body->SetMaterial(0, InMaterial);
	}
	// the basic sphere is 1 m across
	Body->SetWorldScale3D(BodyScale);
	StateLeft = FMath::FRandRange(1.0, 6.0);
	Place();
}

bool AHigataCreature::GoodSpot(double Px, double Pz) const
{
	if (!Field->Inside(Px, Pz, 1.0)) return false;
	return !bAquatic || Field->DepthAt(Px, Pz) >= MinDepth;
}

void AHigataCreature::PickCruiseTarget()
{
	const double Reach = FMath::Clamp(LengthM * 30.0, 0.3, 6.0);
	for (int32 Try = 0; Try < 8; ++Try)
	{
		const double A = FMath::FRandRange(0.0, 2.0 * UE_DOUBLE_PI);
		const double D = FMath::FRandRange(0.3, 1.0) * Reach;
		const double Px = X + FMath::Sin(A) * D, Pz = Z + FMath::Cos(A) * D;
		if (GoodSpot(Px, Pz))
		{
			TargetX = Px;
			TargetZ = Pz;
			return;
		}
	}
	TargetX = X;
	TargetZ = Z;
}

void AHigataCreature::Startle(const FVector2D& From)
{
	if (bBurrowed) return;
	Alert = 1.0;
	FVector2D Away = FVector2D(X, Z) - From;
	if (Away.IsNearlyZero()) Away = FVector2D(FMath::Sin(Heading), FMath::Cos(Heading));
	Away.Normalize();
	const double Run = FMath::Clamp(FleeDistance * 2.0, 1.0, 4.0);
	TargetX = X + Away.X * Run;
	TargetZ = Z + Away.Y * Run;
	if (!GoodSpot(TargetX, TargetZ))
	{
		// sideways if straight away leaves the water
		TargetX = X - Away.Y * Run;
		TargetZ = Z + Away.X * Run;
		if (!GoodSpot(TargetX, TargetZ))
		{
			TargetX = X;
			TargetZ = Z;
		}
	}
	State = EState::Flee;
	StateLeft = 3.0;
}

void AHigataCreature::Step(float Dt, const FVector2D& Player)
{
	Alert = FMath::Max(0.0, Alert - Dt / 6.0);
	if (bBurrowed)
	{
		Place();
		return;
	}

	// the tide left it: back to water if there is some near, else it stays put (the spawner retires it)
	if (bAquatic && Field->DepthAt(X, Z) < MinDepth && State != EState::Flee)
	{
		double Wx = 0, Wz = 0;
		if (Field->NearestWater(X, Z, MinDepth, 6.0, Wx, Wz))
		{
			TargetX = Wx;
			TargetZ = Wz;
			State = EState::Flee;
			StateLeft = 4.0;
		}
	}

	const double PlayerD = FVector2D::Distance(Player, FVector2D(X, Z));
	if (State != EState::Flee && PlayerD < FleeDistance)
	{
		Startle(Player);
	}

	StateLeft -= Dt;
	switch (State)
	{
	case EState::Rest:
		if (StateLeft <= 0)
		{
			PickCruiseTarget();
			State = EState::Cruise;
			StateLeft = 12.0;
		}
		break;
	case EState::Cruise:
	case EState::Flee:
	{
		const double Speed = State == EState::Flee ? FleeSpeed : CruiseSpeed;
		const FVector2D To(TargetX - X, TargetZ - Z);
		const double Dist = To.Size();
		if (Dist < 0.02 || StateLeft <= 0)
		{
			State = EState::Rest;
			const double RestMean = Individual.Species->BrainNumber("restMean_s", 6.0);
			StateLeft = FMath::Max(0.5, -RestMean * FMath::Loge(FMath::Max(0.01, static_cast<double>(FMath::FRand()))));
			break;
		}
		const double Move = FMath::Min(Dist, Speed * Dt);
		const double Nx = X + To.X / Dist * Move, Nz = Z + To.Y / Dist * Move;
		if (GoodSpot(Nx, Nz) || !GoodSpot(X, Z))
		{
			X = Nx;
			Z = Nz;
		}
		else
		{
			TargetX = X;
			TargetZ = Z;
		}
		// turn toward the way it goes (web convention: forward is (sin h, cos h))
		const double Want = FMath::Atan2(To.X, To.Y);
		double Turn = FMath::Fmod(Want - Heading + 3.0 * UE_DOUBLE_PI, 2.0 * UE_DOUBLE_PI) - UE_DOUBLE_PI;
		Heading += FMath::Clamp(Turn, -6.0 * Dt, 6.0 * Dt);
		break;
	}
	}
	Place();
}

FVector AHigataCreature::GetBodyLocation() const
{
	return Body->GetComponentLocation();
}

void AHigataCreature::Place()
{
	const double Ground = Field->GroundAt(X, Z);
	double Y = Ground + BodyLift;
	if (bAquatic && !bBottom)
	{
		// mid water: a little under halfway up, never out of it
		const double Water = Field->WaterAt(X, Z);
		const double Depth = Water - Ground;
		Y = Ground + FMath::Clamp(Depth * 0.4, BodyLift, FMath::Max(BodyLift, Depth - BodyLift * 1.5));
	}
	SetActorLocation(FHigataMap::ToUnreal(X, Z, Y));
	// web heading h faces (sin h, cos h) in (x, z); Unreal yaw is atan2(Y, X) with X east, Y south
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(FMath::Cos(Heading), FMath::Sin(Heading)));
	SetActorRotation(FRotator(0.0, Yaw, 0.0));
}
