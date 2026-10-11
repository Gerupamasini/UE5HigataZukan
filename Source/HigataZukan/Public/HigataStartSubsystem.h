#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "HigataStartSubsystem.generated.h"

/**
 * Builds the field when a game or Play-in-Editor world begins play: spawns an AHigataFlat for the Hashirimizu map
 * in whatever level is open, so no level asset is needed yet.
 */
UCLASS()
class HIGATAZUKAN_API UHigataStartSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};
