#include "SkyboxRotatorComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

USkyboxRotatorComponent::USkyboxRotatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USkyboxRotatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!SkyMaterial)
	{
		return;
	}

	Rotation = FMath::Fmod(Rotation + DegreesPerSecond * DeltaTime, 360.f);
	SkyMaterial->SetScalarParameterValue(TEXT("Rotation"), FMath::DegreesToRadians(Rotation));
}
