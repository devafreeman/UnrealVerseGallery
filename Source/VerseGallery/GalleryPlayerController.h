#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SGalleryBookPanel.h"
#include "GalleryPlayerController.generated.h"

class AGalleryBook;

/** Owns the book panel and switches the mouse between camera look and UI clicks. */
UCLASS()
class VERSEGALLERY_API AGalleryPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	void ShowBook(AGalleryBook* Book);
	void HideBookIfShowing(AGalleryBook* Book);
	void HideBook();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	void VisitCurrentWebsite();

	TSharedPtr<SGalleryBookPanel> BookPanel;
	TWeakObjectPtr<AGalleryBook> CurrentBook;
};
