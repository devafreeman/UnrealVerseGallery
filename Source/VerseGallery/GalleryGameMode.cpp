#include "GalleryGameMode.h"
#include "GalleryBuilder.h"
#include "GalleryCharacter.h"
#include "GalleryPlayerController.h"
#include "EngineUtils.h"
#include "Engine/PlayerStartPIE.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"

AGalleryGameMode::AGalleryGameMode()
{
	DefaultPawnClass = AGalleryCharacter::StaticClass();
	PlayerControllerClass = AGalleryPlayerController::StaticClass();
}

AGalleryBuilder* AGalleryGameMode::FindOrSpawnBuilder()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TActorIterator<AGalleryBuilder> It(World);
	if (It)
	{
		return *It;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AGalleryBuilder>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
}

void AGalleryGameMode::RestartPlayer(AController* NewPlayer)
{
	AGalleryBuilder* Builder = FindOrSpawnBuilder();

	// A PlayerStart placed by hand in the level wins over the Unity spawn point. The editor's temporary
	// "play from camera" start doesn't count: the intro tour has to begin and end at the Unity spawn point.
	bool bHasPlayerStart = false;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (!It->IsA<APlayerStartPIE>())
		{
			bHasPlayerStart = true;
			break;
		}
	}

	FVector Location;
	FRotator Rotation;
	if (!bHasPlayerStart && Builder && Builder->GetPlayerStart(Location, Rotation))
	{
		RestartPlayerAtTransform(NewPlayer, FTransform(Rotation, Location));
		if (NewPlayer)
		{
			NewPlayer->SetControlRotation(Rotation);
		}
		return;
	}

	Super::RestartPlayer(NewPlayer);
}
