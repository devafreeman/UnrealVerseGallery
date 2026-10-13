#include "SGalleryBookPanel.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace GalleryPanelStyle
{
	const FLinearColor Background(0.07f, 0.055f, 0.04f, 0.93f);
	const FLinearColor Gold(0.96f, 0.85f, 0.6f, 1.f);
	const FLinearColor Muted(0.78f, 0.72f, 0.62f, 1.f);
	const FLinearColor TextColor(0.95f, 0.93f, 0.9f, 1.f);
	const FLinearColor Link(0.55f, 0.75f, 1.f, 1.f);
	constexpr int32 ShotColumns = 4;
	constexpr float ShotWidth = 118.f;
	constexpr float ShotHeight = 74.f;
}

void SGalleryBookPanel::Construct(const FArguments& InArgs)
{
	using namespace GalleryPanelStyle;

	OnVisitWebsite = InArgs._OnVisitWebsite;
	OnClose = InArgs._OnClose;

	ChildSlot
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Center)
	.Padding(FMargin(0.f, 0.f, 60.f, 0.f))
	[
		SNew(SBox)
		.WidthOverride(560.f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Background)
			.Padding(FMargin(24.f))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(TitleText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
					.ColorAndOpacity(Gold)
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 14.f)
				[
					SAssignNew(AuthorText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 15))
					.ColorAndOpacity(Muted)
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(DescriptionText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					.ColorAndOpacity(TextColor)
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 6.f)
				[
					SAssignNew(ScreenshotsLabel, STextBlock)
					.Text(FText::FromString(TEXT("Screenshots")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
					.ColorAndOpacity(Muted)
				]

				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(ScreenshotGrid, SUniformGridPanel)
					.SlotPadding(FMargin(4.f))
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 10.f)
				[
					SAssignNew(UrlText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
					.ColorAndOpacity(Link)
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
					[
						SAssignNew(VisitButton, SButton)
						.ContentPadding(FMargin(16.f, 8.f))
						.OnClicked_Lambda([this]()
						{
							OnVisitWebsite.ExecuteIfBound();
							return FReply::Handled();
						})
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Visit Website")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
						]
					]

					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.ContentPadding(FMargin(16.f, 8.f))
						.OnClicked_Lambda([this]()
						{
							OnClose.ExecuteIfBound();
							return FReply::Handled();
						})
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("Close")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
						]
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Click Visit Website to open this page. Press Q to close.")))
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 11))
					.ColorAndOpacity(Muted)
					.AutoWrapText(true)
				]
			]
		]
	];
}

void SGalleryBookPanel::SetBook(const FString& Title, const FString& Author, const FString& Description, const FString& Url, const TArray<UTexture2D*>& Screenshots)
{
	using namespace GalleryPanelStyle;

	TitleText->SetText(FText::FromString(Title));
	AuthorText->SetText(FText::FromString(Author));
	DescriptionText->SetText(FText::FromString(Description));
	UrlText->SetText(FText::FromString(Url.IsEmpty() ? TEXT("(no link set)") : Url));
	VisitButton->SetEnabled(!Url.TrimStartAndEnd().IsEmpty());

	ScreenshotGrid->ClearChildren();
	ScreenshotBrushes.Reset();

	int32 Index = 0;
	for (UTexture2D* Texture : Screenshots)
	{
		if (!Texture)
		{
			continue;
		}

		TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
		Brush->SetResourceObject(Texture);
		Brush->ImageSize = FVector2D(ShotWidth, ShotHeight);
		ScreenshotBrushes.Add(Brush);

		ScreenshotGrid->AddSlot(Index % ShotColumns, Index / ShotColumns)
		[
			SNew(SBox)
			.WidthOverride(ShotWidth)
			.HeightOverride(ShotHeight)
			[
				SNew(SImage).Image(Brush.Get())
			]
		];
		++Index;
	}

	const EVisibility ShotsVisibility = Index > 0 ? EVisibility::Visible : EVisibility::Collapsed;
	ScreenshotsLabel->SetVisibility(ShotsVisibility);
	ScreenshotGrid->SetVisibility(ShotsVisibility);
}
