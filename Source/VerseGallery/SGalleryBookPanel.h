#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;
class SButton;
class SUniformGridPanel;
class UTexture2D;
struct FSlateBrush;

/** On-screen book panel: title, author, description, screenshots, Visit Website and Close (port of BookInteractionUI.cs). */
class VERSEGALLERY_API SGalleryBookPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGalleryBookPanel) {}
		SLATE_EVENT(FSimpleDelegate, OnVisitWebsite)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetBook(const FString& Title, const FString& Author, const FString& Description, const FString& Url, const TArray<UTexture2D*>& Screenshots);

private:
	FSimpleDelegate OnVisitWebsite;
	FSimpleDelegate OnClose;

	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> AuthorText;
	TSharedPtr<STextBlock> DescriptionText;
	TSharedPtr<STextBlock> UrlText;
	TSharedPtr<STextBlock> ScreenshotsLabel;
	TSharedPtr<SUniformGridPanel> ScreenshotGrid;
	TSharedPtr<SButton> VisitButton;
	TArray<TSharedPtr<FSlateBrush>> ScreenshotBrushes;
};
