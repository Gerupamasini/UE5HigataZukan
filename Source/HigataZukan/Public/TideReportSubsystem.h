#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "TideReportSubsystem.generated.h"

/**
 * Proves the tide works in any level: when a game or Play-in-Editor world begins play, loads the Tokyo
 * station and shows the current level and the next high and low waters on screen (JST).
 * Independent of the level's game mode, so the stock template maps show it too.
 */
UCLASS()
class HIGATAZUKAN_API UTideReportSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};
