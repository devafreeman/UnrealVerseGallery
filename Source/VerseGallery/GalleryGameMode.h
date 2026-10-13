#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GalleryGameMode.generated.h"

class AGalleryBuilder;

/** Uses the gallery character/controller and spawns the player where the Unity player started. */
UCLASS()
class VERSEGALLERY_API AGalleryGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGalleryGameMode();

	virtual void RestartPlayer(AController* NewPlayer) override;

private:
	AGalleryBuilder* FindOrSpawnBuilder();
};
