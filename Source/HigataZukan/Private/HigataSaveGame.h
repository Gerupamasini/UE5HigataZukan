#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "HigataSaveGame.generated.h"

/** One species in the encyclopedia: how many were caught, the biggest, and when the first one was. */
USTRUCT()
struct HIGATAZUKAN_API FHigataZukanEntry
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Count = 0;

	UPROPERTY()
	float BestLengthMm = 0.f;

	UPROPERTY()
	FDateTime FirstCaughtUtc;
};

/**
 * The Steam edition's save (slot "Higata"): its own format, not shared with the web or mobile builds (decided
 * 2026-10-11). Grows with the port; Version marks changes that need a migration.
 */
UCLASS()
class HIGATAZUKAN_API UHigataSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 1;

	/** Species id -> its encyclopedia entry. */
	UPROPERTY()
	TMap<FString, FHigataZukanEntry> Zukan;
};
