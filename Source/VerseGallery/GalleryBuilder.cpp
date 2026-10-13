#include "GalleryBuilder.h"
#include "BackgroundMusicComponent.h"
#include "GalleryBook.h"
#include "GalleryIntroTourComponent.h"
#include "SkyboxRotatorComponent.h"
#include "VoiceOverIntroComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogGalleryBuilder, Log, All);

namespace
{
	const FName GeneratedTag(TEXT("GalleryGenerated"));

	// Unity's skybox shader multiplies by _Tint * 2 in gamma space (4.59 in linear).
	constexpr float UnitySkyTintScale = 4.59479f;

	using FJsonArray = TArray<TSharedPtr<FJsonValue>>;

	const FJsonArray& GetArr(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		static const FJsonArray Empty;
		return Obj.IsValid() && Obj->HasTypedField<EJson::Array>(Field) ? Obj->GetArrayField(Field) : Empty;
	}

	TSharedPtr<FJsonObject> GetObj(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		if (Obj.IsValid() && Obj->HasTypedField<EJson::Object>(Field))
		{
			return Obj->GetObjectField(Field);
		}
		return nullptr;
	}

	double GetNum(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, double Default = 0.0)
	{
		return Obj.IsValid() && Obj->HasTypedField<EJson::Number>(Field) ? Obj->GetNumberField(Field) : Default;
	}

	bool GetBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, bool Default = false)
	{
		return Obj.IsValid() && Obj->HasTypedField<EJson::Boolean>(Field) ? Obj->GetBoolField(Field) : Default;
	}

	FString GetStr(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		return Obj.IsValid() && Obj->HasTypedField<EJson::String>(Field) ? Obj->GetStringField(Field) : FString();
	}

	FVector ReadVec(const FJsonArray& A)
	{
		return A.Num() >= 3 ? FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()) : FVector::ZeroVector;
	}

	FLinearColor ReadColor(const FJsonArray& A)
	{
		if (A.Num() < 3)
		{
			return FLinearColor::White;
		}
		return FLinearColor(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber(), A.Num() > 3 ? A[3]->AsNumber() : 1.0);
	}

	void ReadVectors(const FJsonArray& A, TArray<FVector>& Out)
	{
		Out.Reset(A.Num() / 3);
		for (int32 i = 0; i + 2 < A.Num(); i += 3)
		{
			Out.Emplace(A[i]->AsNumber(), A[i + 1]->AsNumber(), A[i + 2]->AsNumber());
		}
	}

	void ReadInts(const FJsonArray& A, TArray<int32>& Out, int32 Offset = 0)
	{
		Out.Reserve(Out.Num() + A.Num());
		for (const TSharedPtr<FJsonValue>& V : A)
		{
			Out.Add(static_cast<int32>(V->AsNumber()) + Offset);
		}
	}

	template <typename T>
	T* LoadAsset(const FString& Path)
	{
		return LoadObject<T>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	FString GalleryAssetPath(const TCHAR* Folder, const FString& Name)
	{
		return FString::Printf(TEXT("/Game/Gallery/%s/%s.%s"), Folder, *Name, *Name);
	}

	// Generated components are transient, so the editor can drop them (undo, leaving PIE,
	// reinstancing) while bSceneBuilt stays set.
	bool HasGeneratedComponents(const AActor* Actor)
	{
		TInlineComponentArray<UActorComponent*> Components(Actor);
		for (const UActorComponent* Comp : Components)
		{
			if (IsValid(Comp) && Comp->IsRegistered() && Comp->ComponentHasTag(GeneratedTag))
			{
				return true;
			}
		}
		return false;
	}
}

AGalleryBuilder::AGalleryBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

template <typename T>
T* AGalleryBuilder::MakeGenerated(const FString& Name)
{
	FString Clean;
	for (const TCHAR Ch : Name)
	{
		Clean.AppendChar(FChar::IsAlnum(Ch) ? Ch : TEXT('_'));
	}
	Clean = Clean.Right(60);
	if (Clean.IsEmpty())
	{
		Clean = TEXT("Generated");
	}

	T* Comp = NewObject<T>(this, MakeUniqueObjectName(this, T::StaticClass(), FName(*Clean)), RF_Transient);
	Comp->ComponentTags.Add(GeneratedTag);
	if constexpr (std::is_base_of_v<USceneComponent, T>)
	{
		Comp->SetupAttachment(RootComponent);
	}
	return Comp;
}

void AGalleryBuilder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!bSceneBuilt || !HasGeneratedComponents(this))
	{
		BuildScene();
	}
}

void AGalleryBuilder::BeginPlay()
{
	Super::BeginPlay();
	if (!bSceneBuilt || !HasGeneratedComponents(this))
	{
		BuildScene();
	}
	BuildRuntime();
}

void AGalleryBuilder::Rebuild()
{
	Data.Reset();
	TextureCache.Reset();
	BuildScene();
}

bool AGalleryBuilder::LoadData()
{
	if (Data.IsValid())
	{
		return true;
	}

	FString Json;
	const FString Path = FPaths::ProjectDir() / DataFile;
	if (FFileHelper::LoadFileToString(Json, *Path))
	{
		CachedJson = Json;
	}
	else if (!CachedJson.IsEmpty())
	{
		Json = CachedJson;
	}
	else
	{
		UE_LOG(LogGalleryBuilder, Warning, TEXT("Gallery data not found at %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Parsed;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
	{
		UE_LOG(LogGalleryBuilder, Error, TEXT("Could not parse gallery data (%s)"), *Path);
		return false;
	}

	Data = Parsed;
	TextureFiles.Reset();
	for (const TSharedPtr<FJsonValue>& V : GetArr(Data, TEXT("textures")))
	{
		const TSharedPtr<FJsonObject> Tex = V->AsObject();
		TextureFiles.Add(GetStr(Tex, TEXT("name")), GetStr(Tex, TEXT("file")));
	}
	return true;
}

void AGalleryBuilder::ClearGenerated()
{
	TInlineComponentArray<UActorComponent*> Components(this);
	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp->ComponentHasTag(GeneratedTag))
		{
			Comp->DestroyComponent();
		}
	}
	Materials.Reset();
	SkyMaterial = nullptr;
	bSceneBuilt = false;
}

void AGalleryBuilder::BuildScene()
{
	ClearGenerated();
	if (!LoadData())
	{
		return;
	}

	BuildMaterials();
	BuildMeshes();
	BuildColliders();
	BuildSky();
	BuildLights();
	BuildExposure();
	bSceneBuilt = true;
}

void AGalleryBuilder::BuildExposure()
{
	UPostProcessComponent* PostProcess = MakeGenerated<UPostProcessComponent>(TEXT("Exposure"));
	PostProcess->bUnbound = true;
	FPostProcessSettings& Settings = PostProcess->Settings;
	Settings.bOverride_AutoExposureMethod = true;
	Settings.AutoExposureMethod = AEM_Manual;
	Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Settings.AutoExposureApplyPhysicalCameraExposure = false;
	// Manual exposure scales by 1 / (1.2 * 2^-Bias); log2(1.2) makes Exposure 0 a 1:1 match with Unity.
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = Exposure + 0.263f;
	Settings.bOverride_LocalExposureHighlightContrastScale = true;
	Settings.LocalExposureHighlightContrastScale = 1.f;
	Settings.bOverride_LocalExposureShadowContrastScale = true;
	Settings.LocalExposureShadowContrastScale = 1.f;
	Settings.bOverride_LocalExposureDetailStrength = true;
	Settings.LocalExposureDetailStrength = 1.f;

	// Unity's default pipeline has no filmic tone curve, which fades the poster colors here.
	Settings.bOverride_ToneCurveAmount = true;
	Settings.ToneCurveAmount = 0.f;
	Settings.bOverride_BlueCorrection = true;
	Settings.BlueCorrection = 0.f;
	Settings.bOverride_ExpandGamut = true;
	Settings.ExpandGamut = 0.f;
	Settings.bOverride_MotionBlurAmount = true;
	Settings.MotionBlurAmount = 0.f;
	Settings.bOverride_BloomIntensity = true;
	Settings.BloomIntensity = 0.f;

	Settings.bOverride_LumenSkylightLeaking = true;
	Settings.LumenSkylightLeaking = AmbientFill;
	PostProcess->RegisterComponent();
}

UTexture2D* AGalleryBuilder::LoadGalleryTexture(const FString& Name)
{
	if (Name.IsEmpty())
	{
		return nullptr;
	}
	if (const TObjectPtr<UTexture2D>* Found = TextureCache.Find(Name))
	{
		return Found->Get();
	}

	UTexture2D* Texture = LoadAsset<UTexture2D>(GalleryAssetPath(TEXT("Textures"), Name));
	if (!Texture)
	{
		// Setup script not run yet: load the raw image straight from GalleryData.
		if (const FString* File = TextureFiles.Find(Name))
		{
			Texture = FImageUtils::ImportFileAsTexture2D(FPaths::ProjectDir() / FPaths::GetPath(DataFile) / *File);
		}
	}

	TextureCache.Add(Name, Texture);
	return Texture;
}

void AGalleryBuilder::BuildMaterials()
{
	UMaterialInterface* Lit = LoadAsset<UMaterialInterface>(TEXT("/Game/Gallery/Materials/M_GalleryLit.M_GalleryLit"));
	UMaterialInterface* Unlit = LoadAsset<UMaterialInterface>(TEXT("/Game/Gallery/Materials/M_GalleryUnlit.M_GalleryUnlit"));
	UMaterialInterface* Translucent = LoadAsset<UMaterialInterface>(TEXT("/Game/Gallery/Materials/M_GalleryTranslucent.M_GalleryTranslucent"));

	DefaultMaterial = Lit ? Lit : LoadAsset<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Lit)
	{
		UE_LOG(LogGalleryBuilder, Warning, TEXT("Gallery materials not found. Run Content/Python/gallery_setup.py (see README). Using plain colors for now."));
	}

	for (const TSharedPtr<FJsonValue>& V : GetArr(Data, TEXT("materials")))
	{
		const TSharedPtr<FJsonObject> M = V->AsObject();

		UMaterialInterface* Parent = DefaultMaterial;
		if (GetBool(M, TEXT("unlit")) && Unlit)
		{
			Parent = Unlit;
		}
		else if (GetBool(M, TEXT("transparent")) && Translucent)
		{
			Parent = Translucent;
		}

		if (!Parent)
		{
			Materials.Add(nullptr);
			continue;
		}

		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Parent, this);
		const FLinearColor Color = ReadColor(GetArr(M, TEXT("color")));
		Instance->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Instance->SetVectorParameterValue(TEXT("Color"), Color);
		Instance->SetScalarParameterValue(TEXT("Metallic"), static_cast<float>(GetNum(M, TEXT("metallic"))));
		Instance->SetScalarParameterValue(TEXT("Roughness"), static_cast<float>(GetNum(M, TEXT("roughness"), 0.5)));
		const FJsonArray& Emissive = GetArr(M, TEXT("emissive"));
		Instance->SetVectorParameterValue(TEXT("Emissive"), Emissive.Num() >= 3 ? ReadColor(Emissive) : FLinearColor::Black);

		if (UTexture2D* BaseMap = LoadGalleryTexture(GetStr(M, TEXT("texture"))))
		{
			Instance->SetTextureParameterValue(TEXT("BaseMap"), BaseMap);
		}
		if (UTexture2D* EmissiveMap = LoadGalleryTexture(GetStr(M, TEXT("emissiveTexture"))))
		{
			Instance->SetTextureParameterValue(TEXT("EmissiveMap"), EmissiveMap);
		}

		Materials.Add(Instance);
	}
}

void AGalleryBuilder::BuildMeshes()
{
	const TArray<FLinearColor> NoColors;
	const TArray<FProcMeshTangent> NoTangents;

	for (const TSharedPtr<FJsonValue>& MeshValue : GetArr(Data, TEXT("meshes")))
	{
		const TSharedPtr<FJsonObject> Mesh = MeshValue->AsObject();
		if (!Mesh.IsValid())
		{
			continue;
		}

		UProceduralMeshComponent* Comp = MakeGenerated<UProceduralMeshComponent>(GetStr(Mesh, TEXT("name")));
		Comp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Comp->SetCastShadow(GetBool(Mesh, TEXT("castShadow"), true));
		Comp->RegisterComponent();

		int32 SectionIndex = 0;
		for (const TSharedPtr<FJsonValue>& SectionValue : GetArr(Mesh, TEXT("sections")))
		{
			const TSharedPtr<FJsonObject> Section = SectionValue->AsObject();

			TArray<FVector> Vertices;
			TArray<FVector> Normals;
			TArray<FVector2D> UVs;
			TArray<int32> Triangles;
			ReadVectors(GetArr(Section, TEXT("v")), Vertices);
			ReadVectors(GetArr(Section, TEXT("n")), Normals);
			ReadInts(GetArr(Section, TEXT("t")), Triangles);
			// Unity's winding is front-facing the other way round; the two-sided material would otherwise flip every normal.
			for (int32 i = 0; i + 2 < Triangles.Num(); i += 3)
			{
				Swap(Triangles[i + 1], Triangles[i + 2]);
			}

			const FJsonArray& UVArray = GetArr(Section, TEXT("uv"));
			UVs.Reserve(UVArray.Num() / 2);
			for (int32 i = 0; i + 1 < UVArray.Num(); i += 2)
			{
				UVs.Emplace(UVArray[i]->AsNumber(), UVArray[i + 1]->AsNumber());
			}

			Comp->CreateMeshSection_LinearColor(SectionIndex, Vertices, Triangles, Normals, UVs, NoColors, NoTangents, false);

			const int32 MaterialIndex = static_cast<int32>(GetNum(Section, TEXT("mat"), -1));
			UMaterialInterface* Material = Materials.IsValidIndex(MaterialIndex) && Materials[MaterialIndex] ? Materials[MaterialIndex].Get() : DefaultMaterial.Get();
			Comp->SetMaterial(SectionIndex, Material);
			++SectionIndex;
		}
	}
}

void AGalleryBuilder::BuildColliders()
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;

	for (const TSharedPtr<FJsonValue>& ColliderValue : GetArr(Data, TEXT("colliders")))
	{
		const TSharedPtr<FJsonObject> Collider = ColliderValue->AsObject();
		TArray<FVector> ColliderVerts;
		ReadVectors(GetArr(Collider, TEXT("v")), ColliderVerts);
		ReadInts(GetArr(Collider, TEXT("t")), Triangles, Vertices.Num());
		Vertices.Append(ColliderVerts);
	}

	if (Vertices.Num() == 0)
	{
		return;
	}

	UProceduralMeshComponent* Comp = MakeGenerated<UProceduralMeshComponent>(TEXT("Collision"));
	Comp->bUseComplexAsSimpleCollision = true;
	Comp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Comp->SetVisibility(false);
	Comp->SetHiddenInGame(true);
	Comp->SetCastShadow(false);
	Comp->RegisterComponent();
	Comp->CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
}

void AGalleryBuilder::BuildLights()
{
	const bool bHasAtmosphere = FindComponentByClass<USkyAtmosphereComponent>() != nullptr;

	for (const TSharedPtr<FJsonValue>& LightValue : GetArr(Data, TEXT("lights")))
	{
		const TSharedPtr<FJsonObject> Light = LightValue->AsObject();
		const FString Type = GetStr(Light, TEXT("type"));
		const FString Name = GetStr(Light, TEXT("name"));
		const FLinearColor Color = ReadColor(GetArr(Light, TEXT("color")));
		const FLinearColor LampColor = FMath::Lerp(FLinearColor::White, Color, LocalLightSaturation);
		const float Intensity = static_cast<float>(GetNum(Light, TEXT("intensity"), 1.0));
		const float Range = static_cast<float>(GetNum(Light, TEXT("range"), 1000.0));
		const bool bShadows = GetBool(Light, TEXT("shadows"));
		const FVector Position = ReadVec(GetArr(Light, TEXT("pos")));
		const FRotator Rotation = ReadVec(GetArr(Light, TEXT("dir"))).GetSafeNormal().Rotation();

		if (Type == TEXT("directional"))
		{
			UDirectionalLightComponent* Comp = MakeGenerated<UDirectionalLightComponent>(Name);
			Comp->SetMobility(EComponentMobility::Movable);
			Comp->SetRelativeRotation(Rotation);
			Comp->SetIntensity(Intensity * DirectionalLightScale);
			Comp->SetLightColor(Color);
			Comp->SetCastShadows(bShadows);
			Comp->SetAtmosphereSunLight(bHasAtmosphere);
			Comp->RegisterComponent();
		}
		else if (Type == TEXT("spot"))
		{
			USpotLightComponent* Comp = MakeGenerated<USpotLightComponent>(Name);
			Comp->SetMobility(EComponentMobility::Movable);
			Comp->SetRelativeLocation(Position);
			Comp->SetRelativeRotation(Rotation);
			Comp->SetIntensityUnits(ELightUnits::Candelas);
			Comp->SetIntensity(Intensity * LocalLightScale);
			Comp->SetLightColor(LampColor);
			Comp->SetAttenuationRadius(Range);
			Comp->SetOuterConeAngle(static_cast<float>(GetNum(Light, TEXT("spotAngle"), 60.0)) * 0.5f);
			Comp->SetInnerConeAngle(static_cast<float>(GetNum(Light, TEXT("innerSpotAngle"), 40.0)) * 0.5f);
			Comp->SetCastShadows(bShadows);
			Comp->RegisterComponent();
		}
		else if (Type == TEXT("point"))
		{
			UPointLightComponent* Comp = MakeGenerated<UPointLightComponent>(Name);
			Comp->SetMobility(EComponentMobility::Movable);
			Comp->SetRelativeLocation(Position);
			Comp->SetIntensityUnits(ELightUnits::Candelas);
			Comp->SetIntensity(Intensity * LocalLightScale);
			Comp->SetLightColor(LampColor);
			Comp->SetAttenuationRadius(Range * LocalLightRangeScale);
			Comp->SetCastShadows(bShadows);
			Comp->RegisterComponent();
		}
	}
}

void AGalleryBuilder::BuildSky()
{
	const TSharedPtr<FJsonObject> Sky = GetObj(Data, TEXT("sky"));
	UMaterialInterface* SkyBase = LoadAsset<UMaterialInterface>(TEXT("/Game/Gallery/Materials/M_GallerySky.M_GallerySky"));
	UStaticMesh* Sphere = LoadAsset<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	if (Sky.IsValid() && SkyBase && Sphere)
	{
		UStaticMeshComponent* Comp = MakeGenerated<UStaticMeshComponent>(TEXT("SkySphere"));
		Comp->SetStaticMesh(Sphere);
		Comp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Comp->SetCastShadow(false);
		Comp->bAffectDistanceFieldLighting = false;
		Comp->bAffectDynamicIndirectLighting = false;
		Comp->SetRelativeLocation(ReadVec(GetArr(GetObj(Data, TEXT("player")), TEXT("pos"))));
		Comp->SetRelativeScale3D(FVector(SkyRadius / 50.f));

		SkyMaterial = UMaterialInstanceDynamic::Create(SkyBase, this);
		const FString SkyName = FPaths::GetBaseFilename(GetStr(Sky, TEXT("file")));
		if (UTexture* SkyTexture = LoadAsset<UTexture>(GalleryAssetPath(TEXT("Sky"), SkyName)))
		{
			SkyMaterial->SetTextureParameterValue(TEXT("SkyTexture"), SkyTexture);
		}
		FLinearColor Tint = ReadColor(GetArr(Sky, TEXT("tint"))) * (UnitySkyTintScale * static_cast<float>(GetNum(Sky, TEXT("exposure"), 1.0)));
		Tint.A = 1.f;
		SkyMaterial->SetVectorParameterValue(TEXT("Tint"), Tint);
		SkyMaterial->SetScalarParameterValue(TEXT("Rotation"), FMath::DegreesToRadians(static_cast<float>(GetNum(Sky, TEXT("rotation")))));
		Comp->SetMaterial(0, SkyMaterial);
		Comp->RegisterComponent();
	}
	else
	{
		MakeGenerated<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"))->RegisterComponent();
	}

	USkyLightComponent* SkyLight = MakeGenerated<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;
	SkyLight->Intensity = SkyLightIntensity;
	SkyLight->RegisterComponent();
}

void AGalleryBuilder::BuildRuntime()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !LoadData())
	{
		return;
	}

	const FTransform& ActorXf = GetActorTransform();

	for (const TSharedPtr<FJsonValue>& BookValue : GetArr(Data, TEXT("books")))
	{
		const TSharedPtr<FJsonObject> BookData = BookValue->AsObject();

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Location = ActorXf.TransformPosition(ReadVec(GetArr(BookData, TEXT("pos"))));

		AGalleryBook* Book = World->SpawnActor<AGalleryBook>(Location, FRotator::ZeroRotator, Params);
		if (!Book)
		{
			continue;
		}

		Book->BookTitle = GetStr(BookData, TEXT("title"));
		Book->BookAuthor = GetStr(BookData, TEXT("author"));
		Book->BookDescription = GetStr(BookData, TEXT("description"));
		Book->WebsiteUrl = GetStr(BookData, TEXT("url"));
		for (const TSharedPtr<FJsonValue>& Shot : GetArr(BookData, TEXT("screenshots")))
		{
			if (UTexture2D* Texture = LoadGalleryTexture(Shot->AsString()))
			{
				Book->Screenshots.Add(Texture);
			}
		}
		Book->SetInteractRadius(static_cast<float>(GetNum(BookData, TEXT("radius"), 260.0)));
	}

	if (const TSharedPtr<FJsonObject> Music = GetObj(Data, TEXT("music")))
	{
		if (USoundBase* Sound = LoadAsset<USoundBase>(GalleryAssetPath(TEXT("Audio"), GetStr(Music, TEXT("name")))))
		{
			UBackgroundMusicComponent* Comp = MakeGenerated<UBackgroundMusicComponent>(TEXT("BackgroundMusic"));
			Comp->Music = Sound;
			Comp->Volume = static_cast<float>(GetNum(Music, TEXT("volume"), 0.4));
			Comp->RegisterComponent();
		}
		else
		{
			UE_LOG(LogGalleryBuilder, Warning, TEXT("Background music asset missing. Run the setup script (see README)."));
		}
	}

	UVoiceOverIntroComponent* VoiceComp = nullptr;
	if (const TSharedPtr<FJsonObject> Voice = GetObj(Data, TEXT("voice")))
	{
		if (USoundBase* Sound = LoadAsset<USoundBase>(GalleryAssetPath(TEXT("Audio"), GetStr(Voice, TEXT("name")))))
		{
			VoiceComp = MakeGenerated<UVoiceOverIntroComponent>(TEXT("VoiceOverIntro"));
			VoiceComp->VoiceClip = Sound;
			VoiceComp->Volume = static_cast<float>(GetNum(Voice, TEXT("volume"), 1.0));
			VoiceComp->DelaySeconds = static_cast<float>(GetNum(Voice, TEXT("delay"), 2.0));
			VoiceComp->RegisterComponent();
		}
		else
		{
			UE_LOG(LogGalleryBuilder, Warning, TEXT("Voice-over asset %s missing. Run the setup script (see README)."), *GetStr(Voice, TEXT("name")));
		}
	}

	if (const TSharedPtr<FJsonObject> Tour = GetObj(Data, TEXT("introTour")))
	{
		UGalleryIntroTourComponent* Comp = MakeGenerated<UGalleryIntroTourComponent>(TEXT("IntroTour"));
		Comp->Voice = VoiceComp;
		Comp->HandBackAtSeconds = static_cast<float>(GetNum(Tour, TEXT("handBack")));
		Comp->MoveSeconds = static_cast<float>(GetNum(Tour, TEXT("moveSeconds"), 2.5));
		Comp->PushInSpeed = static_cast<float>(GetNum(Tour, TEXT("pushInSpeed"), 6.0));
		Comp->VerticalFieldOfView = static_cast<float>(GetNum(Tour, TEXT("fov"), 50.0));
		Comp->bAllowSkip = GetBool(Tour, TEXT("allowSkip"), true);
		Comp->SkipFadeSeconds = static_cast<float>(GetNum(Tour, TEXT("skipFadeSeconds"), 0.75));
		Comp->MaxWaitForVoice = static_cast<float>(GetNum(Tour, TEXT("maxWaitForVoice"), 6.0));

		for (const TSharedPtr<FJsonValue>& ShotValue : GetArr(Tour, TEXT("shots")))
		{
			const TSharedPtr<FJsonObject> ShotData = ShotValue->AsObject();
			const FRotator Local(GetNum(ShotData, TEXT("pitch")), GetNum(ShotData, TEXT("yaw")), 0.0);

			FGalleryTourShot& Shot = Comp->Shots.AddDefaulted_GetRef();
			Shot.Label = GetStr(ShotData, TEXT("label"));
			Shot.Location = ActorXf.TransformPosition(ReadVec(GetArr(ShotData, TEXT("pos"))));
			Shot.Rotation = ActorXf.TransformRotation(Local.Quaternion()).Rotator();
			Shot.StartTime = static_cast<float>(GetNum(ShotData, TEXT("start")));
		}
		Comp->RegisterComponent();
	}

	const TSharedPtr<FJsonObject> Sky = GetObj(Data, TEXT("sky"));
	const float SkySpeed = static_cast<float>(GetNum(Sky, TEXT("rotateSpeed")));
	if (SkyMaterial && !FMath::IsNearlyZero(SkySpeed))
	{
		USkyboxRotatorComponent* Rotator = MakeGenerated<USkyboxRotatorComponent>(TEXT("SkyboxRotator"));
		Rotator->SkyMaterial = SkyMaterial;
		Rotator->DegreesPerSecond = SkySpeed;
		Rotator->Rotation = static_cast<float>(GetNum(Sky, TEXT("rotation")));
		Rotator->RegisterComponent();
	}
}

bool AGalleryBuilder::GetPlayerStart(FVector& OutLocation, FRotator& OutRotation)
{
	if (!LoadData())
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Player = GetObj(Data, TEXT("player"));
	if (!Player.IsValid())
	{
		return false;
	}

	// Unity stores the feet position; Unreal spawns at the capsule center.
	const FVector Feet = ReadVec(GetArr(Player, TEXT("pos")));
	OutLocation = GetActorTransform().TransformPosition(Feet) + FVector(0.f, 0.f, 100.f);
	OutRotation = FRotator(0.f, GetActorRotation().Yaw + GetNum(Player, TEXT("yaw")), 0.f);
	return true;
}
