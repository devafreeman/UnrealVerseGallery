#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkyboxRotatorComponent.generated.h"

class UMaterialInstanceDynamic;

/** Slowly rotates the sky while the game is playing (port of SkyboxRotator.cs). */
UCLASS(ClassGroup = (Gallery), meta = (BlueprintSpawnableComponent))
class VERSEGALLERY_API USkyboxRotatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkyboxRotatorComponent();

	/** Degrees per second the sky spins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky")
	float DegreesPerSecond = 2.5f;

	/** Current rotation in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky")
	float Rotation = 0.f;

	/** Sky material instance with a "Rotation" (radians) scalar parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky")
	TObjectPtr<UMaterialInstanceDynamic> SkyMaterial;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
