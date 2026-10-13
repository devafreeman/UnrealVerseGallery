#include "BackgroundMusicComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"

void UBackgroundMusicComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!Music)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BackgroundMusic] No music assigned."));
		return;
	}

	if (USoundWave* Wave = Cast<USoundWave>(Music))
	{
		Wave->bLooping = true;
	}

	Playing = UGameplayStatics::SpawnSound2D(this, Music, Volume, 1.f, 0.f, nullptr, true, false);
}
