#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GalleryIntroTourComponent.generated.h"

class ACameraActor;
class APlayerController;
class UVoiceOverIntroComponent;

USTRUCT(BlueprintType)
struct FGalleryTourShot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Tour")
	FString Label;

	/** Camera position in world space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Tour")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Tour")
	FRotator Rotation = FRotator::ZeroRotator;

	/** Seconds into the voice-over when the camera starts moving to this shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Tour")
	float StartTime = 0.f;
};

/**
 * Intro "video" when play starts (port of GalleryIntroTour.cs): the player is frozen while a camera
 * glides from podium to podium in sync with the voice-over, then the view blends back to the player
 * and controls are restored. Space or Enter skips it.
 */
UCLASS(ClassGroup = (Gallery), meta = (BlueprintSpawnableComponent))
class VERSEGALLERY_API UGalleryIntroTourComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGalleryIntroTourComponent();

	/** Shots in voice-over order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Tour")
	TArray<FGalleryTourShot> Shots;

	/** Voice-over the shot times are measured against. Without one the tour runs on its own clock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Tour")
	TObjectPtr<UVoiceOverIntroComponent> Voice;

	/** Seconds into the voice-over when control returns to the player. 0 = when the voice-over ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Tour", meta = (ClampMin = "0.0"))
	float HandBackAtSeconds = 0.f;

	/** Seconds the camera takes to glide from one podium to the next. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Tour|Camera", meta = (ClampMin = "0.0"))
	float MoveSeconds = 2.5f;

	/** Slow forward drift while holding on a shot, in cm per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Tour|Camera")
	float PushInSpeed = 6.f;

	/** Vertical field of view in degrees, as in Unity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Tour|Camera", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float VerticalFieldOfView = 50.f;

	/** Seconds to ease from the tour camera back to the player's view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Tour|Camera", meta = (ClampMin = "0.0"))
	float BlendBackSeconds = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Tour|Skip")
	bool bAllowSkip = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Tour|Skip", meta = (ClampMin = "0.0"))
	float SkipFadeSeconds = 0.75f;

	/** If the voice-over hasn't started after this many seconds, the tour runs on its own clock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Tour", meta = (ClampMin = "0.0"))
	float MaxWaitForVoice = 6.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	enum class ETourState : uint8
	{
		WaitingForPlayer,
		Running,
		Done
	};

	bool TryStart();
	float TourTime();
	int32 ShotIndexAt(float Time) const;
	void BeginShot(int32 Index, float Time);
	void MoveCamera();
	void Finish(bool bSkipped);
	bool SkipPressed();

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	TWeakObjectPtr<APlayerController> PlayerController;

	ETourState State = ETourState::WaitingForPlayer;
	int32 ShotIndex = INDEX_NONE;
	double ShotStartedAt = 0.0;
	FVector FromLocation = FVector::ZeroVector;
	FQuat FromRotation = FQuat::Identity;
	float Clock = 0.f;
	bool bVoiceStarted = false;
	bool bSkipKeyWasDown = false;
};
