#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HigataMap.h"
#include "HigataFlat.generated.h"

class ADirectionalLight;
class AHigataWalker;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;

/**
 * One field map, built at runtime from the data files: the ground from the height grid, the sea at the
 * station's real tide level, and the sun at the real position for the map's latitude and longitude.
 * Spawned by UHigataStartSubsystem when play begins, in whatever level is open; the template level's meshes
 * are hidden, its sky and light are reused.
 *
 * Ground colours come from the substrate grid; the ground darkens and turns glossy below the recent high-water
 * mark. Materials come from tools/unreal/make_materials.py (plain colours if they are missing).
 * The player walks (AHigataWalker). Keys: T / G move the clock an hour, R resets.
 */
UCLASS()
class HIGATAZUKAN_API AHigataFlat : public AActor
{
	GENERATED_BODY()

public:
	AHigataFlat();

	virtual void Tick(float DeltaSeconds) override;

	/** The map to build; set before BeginPlay. */
	FString MapId = TEXT("hashirimizu");

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> Ground;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> Surroundings;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> Sea;

	UPROPERTY(Transient)
	TObjectPtr<ADirectionalLight> SunLight;

	/** The player on foot, spawned in place of the level's default pawn. */
	UPROPERTY(Transient)
	TObjectPtr<AHigataWalker> Walker;

	/** The generated ground material's instance (null with the plain fallback); its WetLevelM follows the tide. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GroundMaterial;

	FHigataMap Map;
	bool bReady = false;
	bool bPlacedPlayer = false;
	/** True when the materials made by tools/unreal/make_materials.py were found. */
	bool bGeneratedMaterials = false;
	/** Hours added to the real clock with T / G. */
	int32 ClockOffsetHours = 0;

	void HideTemplateMeshes();
	void BuildGround();
	void BuildSurroundings();
	void BuildSea();
	void FindOrSpawnSun();
	void PlacePlayer();
	void HandleClockKeys();
	UMaterialInterface* MakePlainMaterial(const FLinearColor& Color);
	/** Highest recent water, decaying 5 cm an hour as the flat drains (the web build's wet band rule). */
	double WetLevelAt(const FDateTime& Utc) const;
};
