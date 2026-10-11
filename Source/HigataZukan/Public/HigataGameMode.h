#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HigataGameMode.generated.h"

/**
 * The project's default game mode. For now it only proves the tide works: on Play it loads the Tokyo station
 * and shows the current level and the next high and low water on screen (JST).
 */
UCLASS()
class HIGATAZUKAN_API AHigataGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};
