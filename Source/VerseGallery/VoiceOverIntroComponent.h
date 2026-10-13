#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VoiceOverIntroComponent.generated.h"

class UAudioComponent;
class USoundBase;

/** Plays a voice-over clip once, a short delay after play starts (port of VoiceOverIntro.cs). */
UCLASS(ClassGroup = (Gallery), meta = (BlueprintSpawnableComponent))
class VERSEGALLERY_API UVoiceOverIntroComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over")
	TObjectPtr<USoundBase> VoiceClip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Volume = 1.f;

	/** Seconds to wait after play starts before the voice-over begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over", meta = (ClampMin = "0.0"))
	float DelaySeconds = 2.f;

	/** Seconds into the clip, or a negative value if it hasn't started yet. */
	float GetPlaybackSeconds() const;

	bool IsPlaying() const;
	bool HasFinished() const;

	/** Cancels a pending start, or fades out the clip if it's already playing. */
	void StopVoice(float FadeSeconds);

protected:
	virtual void BeginPlay() override;

private:
	void PlayVoice();

	FTimerHandle DelayHandle;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Audio;

	double StartedAt = -1.0;
};
