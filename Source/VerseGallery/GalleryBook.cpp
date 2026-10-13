#include "GalleryBook.h"
#include "GalleryPlayerController.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformProcess.h"

AGalleryBook::AGalleryBook()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	RootComponent = Trigger;
	Trigger->InitSphereRadius(InteractRadius);
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetGenerateOverlapEvents(true);
}

void AGalleryBook::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AGalleryBook::HandleBeginOverlap);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &AGalleryBook::HandleEndOverlap);
}

void AGalleryBook::SetInteractRadius(float Radius)
{
	InteractRadius = FMath::Max(50.f, Radius);
	Trigger->SetSphereRadius(InteractRadius, true);
}

static AGalleryPlayerController* GetGalleryController(AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn ? Cast<AGalleryPlayerController>(Pawn->GetController()) : nullptr;
}

void AGalleryBook::HandleBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (AGalleryPlayerController* PC = GetGalleryController(OtherActor))
	{
		PC->ShowBook(this);
	}
}

void AGalleryBook::HandleEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	if (AGalleryPlayerController* PC = GetGalleryController(OtherActor))
	{
		PC->HideBookIfShowing(this);
	}
}

void AGalleryBook::OpenWebsite() const
{
	FString Url = WebsiteUrl.TrimStartAndEnd();
	if (Url.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GalleryBook] No URL set on '%s'."), *BookTitle);
		return;
	}

	if (!Url.StartsWith(TEXT("http://")) && !Url.StartsWith(TEXT("https://")))
	{
		Url = TEXT("https://") + Url;
	}

	FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}
