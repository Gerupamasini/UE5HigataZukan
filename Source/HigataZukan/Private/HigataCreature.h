#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimCore/Individual.h"
#include "HigataCreature.generated.h"

class FHigataField;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * One animal on the flat. Its traits (species, length, sex, ...) come from SimCore's FIndividual; it is drawn as a
 * stand-in shape at its real length (the species' models come later) and moved by simple rules: rest, cruise to a
 * spot with enough water, flee from the player, get back to water when the tide leaves it. AHigataFlat ticks it.
 */
UCLASS()
class HIGATAZUKAN_API AHigataCreature : public AActor
{
	GENERATED_BODY()

public:
	AHigataCreature();

	void Init(const HigataSim::FIndividual& InIndividual, const FHigataField* InField, UMaterialInstanceDynamic* InMaterial);

	/** Advance by Dt seconds. Player position in web metres (x, z). */
	void Step(float Dt, const FVector2D& Player);

	/** Bolt away from a point (the net's swing, a footstep). */
	void Startle(const FVector2D& From);

	const HigataSim::FIndividual& GetIndividual() const { return Individual; }
	FVector2D GetWebXZ() const { return FVector2D(X, Z); }
	double GetAlert() const { return Alert; }

	/** Where the body is drawn (Unreal units). */
	FVector GetBodyLocation() const;

	/** Game time its spawn rule stopped matching (0 = matches); the spawner's cull reads and writes it. */
	double MismatchSince = 0.0;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;

	HigataSim::FIndividual Individual;
	const FHigataField* Field = nullptr;

	enum class EState : uint8 { Rest, Cruise, Flee };
	EState State = EState::Rest;
	double X = 0.0;
	double Z = 0.0;
	double Heading = 0.0;
	double TargetX = 0.0;
	double TargetZ = 0.0;
	double StateLeft = 0.0;
	double Alert = 0.0;
	double LengthM = 0.05;
	bool bAquatic = true;
	bool bBottom = true;
	bool bBurrowed = false;
	double CruiseSpeed = 0.1;
	double FleeSpeed = 0.5;
	double FleeDistance = 1.2;
	double MinDepth = 0.02;
	FVector BodyScale = FVector::OneVector;
	double BodyLift = 0.0;

	void PickCruiseTarget();
	bool GoodSpot(double Px, double Pz) const;
	void Place();
};
