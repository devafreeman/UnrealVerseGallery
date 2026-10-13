#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GalleryCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/** First-person walker matching the Unity Starter Assets controller (WASD, mouse look, Shift sprint, Space jump). */
UCLASS()
class VERSEGALLERY_API AGalleryCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGalleryCharacter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float WalkSpeed = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SprintSpeed = 600.f;

	/** Gamepad right-stick look speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float GamepadLookRate = 60.f;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Mapping;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> GamepadLookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SprintAction;

	void EnsureInputAssets();

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void GamepadLook(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
};
