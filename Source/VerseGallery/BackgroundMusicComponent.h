#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BackgroundMusicComponent.generated.h"

class USoundBase;
class UAudioComponent;

/** Plays looping gallery background music for the whole play session (port of BackgroundMusic.cs). */
UCLASS(ClassGroup = (Gallery), meta = (BlueprintSpawnableComponent))
class VERSEGALLERY_API UBackgroundMusicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	TObjectPtr<USoundBase> Music;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Volume = 0.4f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Playing;
};
