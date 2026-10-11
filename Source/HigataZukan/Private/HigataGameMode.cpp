#include "HigataGameMode.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "TideSubsystem.h"

namespace
{
	const FTimespan Jst = FTimespan::FromHours(9);

	void Show(const FString& Text, const FColor& Color)
	{
		UE_LOG(LogTemp, Display, TEXT("[Higata] %s"), *Text);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 60.f, Color, Text);
		}
	}
}

void AHigataGameMode::BeginPlay()
{
	Super::BeginPlay();

	UGameInstance* GameInstance = GetGameInstance();
	UTideSubsystem* Tide = GameInstance ? GameInstance->GetSubsystem<UTideSubsystem>() : nullptr;
	if (!Tide || !Tide->LoadStation(TEXT("jma_tokyo")))
	{
		Show(TEXT("Tide: could not load Content/Data/tide/stations/jma_tokyo.json (see the Output Log)"), FColor::Red);
		return;
	}

	const FDateTime NowUtc = FDateTime::UtcNow();
	const TArray<FTideExtremum> Next = Tide->GetExtrema(NowUtc, NowUtc + FTimespan::FromHours(25));

	// On-screen messages stack upwards, so add the summary line last to keep it on top.
	for (int32 I = FMath::Min(Next.Num(), 4) - 1; I >= 0; --I)
	{
		const FTideExtremum& E = Next[I];
		Show(FString::Printf(TEXT("  next %s water  %s JST  %+.2f m"), E.bHigh ? TEXT("high") : TEXT("low "),
			*(E.Time + Jst).ToString(TEXT("%m/%d %H:%M")), E.LevelM), FColor::White);
	}
	Show(FString::Printf(TEXT("Tide OK - Tokyo now (%s JST): %+.2f m from mean sea level, %s"),
		*(NowUtc + Jst).ToString(TEXT("%m/%d %H:%M")), Tide->GetLevelAt(NowUtc),
		Tide->GetRateAt(NowUtc) >= 0 ? TEXT("rising") : TEXT("falling")), FColor::Cyan);
}
