#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GalleryBook.generated.h"

class USphereComponent;
class UTexture2D;

/** Invisible trigger at a podium. Shows that podium's book panel when the player walks in (port of GalleryBook.cs). */
UCLASS()
class VERSEGALLERY_API AGalleryBook : public AActor
{
	GENERATED_BODY()

public:
	AGalleryBook();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Book Info")
	FString BookTitle = TEXT("Untitled Volume");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Book Info", meta = (MultiLine = true))
	FString BookAuthor = TEXT("Unknown Author");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Book Info", meta = (MultiLine = true))
	FString BookDescription = TEXT("An aged leather-bound volume from the gallery collection.");

	/** Full URL opened when Visit Website is clicked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Website")
	FString WebsiteUrl = TEXT("https://www.gutenberg.org/");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screenshots")
	TArray<TObjectPtr<UTexture2D>> Screenshots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	float InteractRadius = 260.f;

	UFUNCTION(BlueprintCallable, Category = "Trigger")
	void SetInteractRadius(float Radius);

	UFUNCTION(BlueprintCallable, Category = "Website")
	void OpenWebsite() const;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Trigger")
	TObjectPtr<USphereComponent> Trigger;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
