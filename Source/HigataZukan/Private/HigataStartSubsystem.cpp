#include "HigataStartSubsystem.h"

#include "Engine/World.h"
#include "HigataFlat.h"

bool UHigataStartSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHigataStartSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	InWorld.SpawnActor<AHigataFlat>();
}
