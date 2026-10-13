#include "VoiceOverIntroComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void UVoiceOverIntroComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!VoiceClip)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceOverIntro] No voice clip assigned."));
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(DelayHandle, this, &UVoiceOverIntroComponent::PlayVoice, FMath::Max(0.01f, DelaySeconds), false);
}

void UVoiceOverIntroComponent::PlayVoice()
{
	// Kept alive after it finishes so the intro tour can tell the clip has ended.
	Audio = UGameplayStatics::SpawnSound2D(this, VoiceClip, Volume, 1.f, 0.f, nullptr, false, false);
	StartedAt = Audio ? GetWorld()->GetAudioTimeSeconds() : -1.0;
}

float UVoiceOverIntroComponent::GetPlaybackSeconds() const
{
	if (StartedAt < 0.0 || !GetWorld())
	{
		return -1.f;
	}
	return static_cast<float>(GetWorld()->GetAudioTimeSeconds() - StartedAt);
}

bool UVoiceOverIntroComponent::IsPlaying() const
{
	return Audio && Audio->IsPlaying();
}

bool UVoiceOverIntroComponent::HasFinished() const
{
	return StartedAt >= 0.0 && !IsPlaying();
}

void UVoiceOverIntroComponent::StopVoice(float FadeSeconds)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DelayHandle);
	}
	if (IsPlaying())
	{
		Audio->FadeOut(FMath::Max(0.f, FadeSeconds), 0.f);
	}
}
