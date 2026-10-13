#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "GalleryBuilder.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;

/**
 * Rebuilds the Unity "my scene" gallery from GalleryData/gallery.json:
 * room meshes, podiums, books, posters, collision, lights, sky, book triggers and audio.
 *
 * Textures, sounds and base materials are the assets created by Content/Python/gallery_setup.py
 * under /Game/Gallery. The scene JSON is also cached on the actor so packaged builds don't need the file.
 */
UCLASS(BlueprintType)
class VERSEGALLERY_API AGalleryBuilder : public AActor
{
	GENERATED_BODY()

public:
	AGalleryBuilder();

	/** Scene data file, relative to the project folder. */
	UPROPERTY(EditAnywhere, Category = "Gallery")
	FString DataFile = TEXT("GalleryData/gallery.json");

	/** Unity lamp intensity -> Unreal candelas (ceiling lamps and spot lights). */
	UPROPERTY(EditAnywhere, Category = "Gallery|Lighting")
	float LocalLightScale = 4.f;

	/** How much of the lamps' Unity color to keep (0 = white, 1 = full). Unity's baked lighting made the room read white despite purple lamps. */
	UPROPERTY(EditAnywhere, Category = "Gallery|Lighting", meta = (ClampMin = "0", ClampMax = "1"))
	float LocalLightSaturation = 0.25f;

	/** Stretches the lamps' reach past Unity's range so their light gets to the walls. */
	UPROPERTY(EditAnywhere, Category = "Gallery|Lighting")
	float LocalLightRangeScale = 2.5f;

	/** Unity light intensity -> Unreal lux (directional). */
	UPROPERTY(EditAnywhere, Category = "Gallery|Lighting")
	float DirectionalLightScale = 3.14159265f;

	UPROPERTY(EditAnywhere, Category = "Gallery|Lighting")
	float SkyLightIntensity = 1.f;

	/** Fixed camera exposure instead of Unreal's auto exposure (Unity has none). Each -1 halves the brightness. */
	UPROPERTY(EditAnywhere, Category = "Gallery|Lighting")
	float Exposure = 0.f;

	/** Even fill light on every surface, like Unity's ambient lighting (0 = none, 1 = full sky light). */
	UPROPERTY(EditAnywhere, Category = "Gallery|Lighting", meta = (ClampMin = "0", ClampMax = "1"))
	float AmbientFill = 0.5f;

	/** Radius of the sky sphere in centimeters. */
	UPROPERTY(EditAnywhere, Category = "Gallery|Sky")
	float SkyRadius = 200000.f;

	/** Re-reads the data file and rebuilds everything. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Gallery")
	void Rebuild();

	/** Where the Unity player started (world space, capsule center). */
	bool GetPlayerStart(FVector& OutLocation, FRotator& OutRotation);

	UTexture2D* LoadGalleryTexture(const FString& Name);

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	bool LoadData();
	void ClearGenerated();
	void BuildScene();
	void BuildMaterials();
	void BuildMeshes();
	void BuildColliders();
	void BuildLights();
	void BuildSky();
	void BuildExposure();
	void BuildRuntime();

	template <typename T>
	T* MakeGenerated(const FString& Name);

	UPROPERTY()
	FString CachedJson;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DefaultMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkyMaterial;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTexture2D>> TextureCache;

	TSharedPtr<FJsonObject> Data;
	TMap<FString, FString> TextureFiles;
	bool bSceneBuilt = false;
};
