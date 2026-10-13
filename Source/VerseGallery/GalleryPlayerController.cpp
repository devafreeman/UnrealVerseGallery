#include "GalleryPlayerController.h"
#include "GalleryBook.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

void AGalleryPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;

	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!IsLocalController() || !Viewport)
	{
		return;
	}

	SAssignNew(BookPanel, SGalleryBookPanel)
		.OnVisitWebsite(FSimpleDelegate::CreateUObject(this, &AGalleryPlayerController::VisitCurrentWebsite))
		.OnClose(FSimpleDelegate::CreateUObject(this, &AGalleryPlayerController::HideBook));

	BookPanel->SetVisibility(EVisibility::Collapsed);
	Viewport->AddViewportWidgetContent(BookPanel.ToSharedRef(), 10);
}

void AGalleryPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BookPanel.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(BookPanel.ToSharedRef());
		}
		BookPanel.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void AGalleryPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (CurrentBook.IsValid() && (WasInputKeyJustPressed(EKeys::Q) || WasInputKeyJustPressed(EKeys::Escape)))
	{
		HideBook();
	}
}

void AGalleryPlayerController::ShowBook(AGalleryBook* Book)
{
	if (!Book || !BookPanel.IsValid())
	{
		return;
	}

	CurrentBook = Book;

	TArray<UTexture2D*> Shots;
	for (UTexture2D* Texture : Book->Screenshots)
	{
		if (Texture)
		{
			Shots.Add(Texture);
		}
	}

	BookPanel->SetBook(Book->BookTitle, Book->BookAuthor, Book->BookDescription, Book->WebsiteUrl, Shots);
	BookPanel->SetVisibility(EVisibility::SelfHitTestInvisible);

	bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	SetIgnoreLookInput(true);
}

void AGalleryPlayerController::HideBookIfShowing(AGalleryBook* Book)
{
	if (CurrentBook.Get() == Book)
	{
		HideBook();
	}
}

void AGalleryPlayerController::HideBook()
{
	CurrentBook.Reset();

	if (BookPanel.IsValid())
	{
		BookPanel->SetVisibility(EVisibility::Collapsed);
	}

	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	ResetIgnoreLookInput();
}

void AGalleryPlayerController::VisitCurrentWebsite()
{
	if (const AGalleryBook* Book = CurrentBook.Get())
	{
		Book->OpenWebsite();
	}
}
