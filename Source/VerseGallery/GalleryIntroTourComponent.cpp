#include "GalleryIntroTourComponent.h"
#include "VoiceOverIntroComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogGalleryIntroTour, Log, All);

UGalleryIntroTourComponent::UGalleryIntroTourComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UGalleryIntroTourComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The player pawn may not be possessed yet on the first frames.
	if (State == ETourState::WaitingForPlayer && !TryStart())
	{
		return;
	}
	if (State != ETourState::Running)
	{
		return;
	}

	if (bAllowSkip && SkipPressed())
	{
		Finish(true);
		return;
	}

	Clock += DeltaTime;
	const float Time = TourTime();
	const float End = HandBackAtSeconds > 0.f ? HandBackAtSeconds : TNumericLimits<float>::Max();
	if (Time >= End)
	{
		Finish(false);
		return;
	}

	const int32 Index = ShotIndexAt(Time);
	if (Index != ShotIndex)
	{
		BeginShot(Index, Time);
	}
	MoveCamera();
}

bool UGalleryIntroTourComponent::TryStart()
{
	UWorld* World = GetWorld();
	if (!World || Shots.Num() == 0)
	{
		State = ETourState::Done;
		SetComponentTickEnabled(false);
		return false;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return false;
	}

	FVector EyeLocation = Pawn->GetActorLocation();
	const FRotator EyeRotation = PC->GetControlRotation();
	if (const UCameraComponent* PawnCamera = Pawn->FindComponentByClass<UCameraComponent>())
	{
		EyeLocation = PawnCamera->GetComponentLocation();
	}

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	Camera = World->SpawnActor<ACameraActor>(EyeLocation, EyeRotation, Params);
	if (!Camera)
	{
		UE_LOG(LogGalleryIntroTour, Warning, TEXT("Could not spawn the tour camera; skipping the intro tour."));
		State = ETourState::Done;
		SetComponentTickEnabled(false);
		return false;
	}

	// Unity's field of view is vertical; Unreal's is horizontal.
	FVector2D ViewportSize(16.f, 9.f);
	if (UGameViewportClient* Viewport = World->GetGameViewport())
	{
		FVector2D Size;
		Viewport->GetViewportSize(Size);
		if (Size.X > 0.f && Size.Y > 0.f)
		{
			ViewportSize = Size;
		}
	}
	const float Aspect = ViewportSize.X / ViewportSize.Y;
	const float HalfVertical = FMath::DegreesToRadians(VerticalFieldOfView * 0.5f);
	UCameraComponent* CameraComponent = Camera->GetCameraComponent();
	CameraComponent->bConstrainAspectRatio = false;
	CameraComponent->SetFieldOfView(FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(HalfVertical) * Aspect)));

	Pawn->DisableInput(PC);
	PC->SetViewTargetWithBlend(Camera, 0.f);

	PlayerController = PC;
	bSkipKeyWasDown = PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::Enter);
	State = ETourState::Running;
	return true;
}

float UGalleryIntroTourComponent::TourTime()
{
	if (Voice)
	{
		if (Voice->IsPlaying())
		{
			bVoiceStarted = true;
			return FMath::Max(0.f, Voice->GetPlaybackSeconds());
		}
		if (bVoiceStarted || Voice->HasFinished())
		{
			return TNumericLimits<float>::Max();
		}
		if (Clock < MaxWaitForVoice)
		{
			return 0.f;
		}
	}
	return Clock;
}

int32 UGalleryIntroTourComponent::ShotIndexAt(float Time) const
{
	int32 Index = INDEX_NONE;
	for (int32 i = 0; i < Shots.Num(); ++i)
	{
		if (Index == INDEX_NONE || Shots[i].StartTime <= Time)
		{
			Index = i;
		}
		if (Shots[i].StartTime > Time)
		{
			break;
		}
	}
	return Index;
}

void UGalleryIntroTourComponent::BeginShot(int32 Index, float Time)
{
	ShotIndex = Index;
	ShotStartedAt = GetWorld()->GetTimeSeconds();
	FromLocation = Camera->GetActorLocation();
	FromRotation = Camera->GetActorQuat();
	UE_LOG(LogGalleryIntroTour, Log, TEXT("%.1fs -> %s"), Time, *Shots[Index].Label);
}

void UGalleryIntroTourComponent::MoveCamera()
{
	if (!Camera || !Shots.IsValidIndex(ShotIndex))
	{
		return;
	}

	const FGalleryTourShot& Shot = Shots[ShotIndex];
	const float Held = static_cast<float>(GetWorld()->GetTimeSeconds() - ShotStartedAt);
	const float Alpha = MoveSeconds > 0.f ? FMath::SmoothStep(0.f, 1.f, Held / MoveSeconds) : 1.f;
	const FVector Target = Shot.Location + Shot.Rotation.Vector() * (PushInSpeed * Held);

	Camera->SetActorLocationAndRotation(
		FMath::Lerp(FromLocation, Target, Alpha),
		FQuat::Slerp(FromRotation, Shot.Rotation.Quaternion(), Alpha));
}

void UGalleryIntroTourComponent::Finish(bool bSkipped)
{
	State = ETourState::Done;
	SetComponentTickEnabled(false);

	if (APlayerController* PC = PlayerController.Get())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(Pawn, BlendBackSeconds, VTBlend_EaseInOut, 2.f);
			Pawn->EnableInput(PC);
		}
	}

	// The camera has to outlive the blend back to the player.
	if (Camera)
	{
		Camera->SetLifeSpan(BlendBackSeconds + 0.5f);
	}

	if (bSkipped && Voice)
	{
		Voice->StopVoice(SkipFadeSeconds);
	}

	UE_LOG(LogGalleryIntroTour, Log, TEXT("Intro tour %s; player has control."), bSkipped ? TEXT("skipped") : TEXT("finished"));
}

bool UGalleryIntroTourComponent::SkipPressed()
{
	const APlayerController* PC = PlayerController.Get();
	if (!PC)
	{
		return false;
	}

	// Edge-detected here rather than WasInputKeyJustPressed, which depends on tick order with the controller.
	const bool bDown = PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::Enter);
	const bool bPressed = bDown && !bSkipKeyWasDown;
	bSkipKeyWasDown = bDown;
	return bPressed;
}

void UGalleryIntroTourComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(Camera))
	{
		Camera->Destroy();
	}
	Camera = nullptr;
	Super::EndPlay(EndPlayReason);
}
