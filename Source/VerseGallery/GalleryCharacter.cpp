#include "GalleryCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

AGalleryCharacter::AGalleryCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	Tags.Add(TEXT("Player"));

	GetCapsuleComponent()->InitCapsuleSize(40.f, 96.f);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	Camera->bUsePawnControlRotation = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Unity Starter Assets: move 4 m/s, sprint 6 m/s, jump 1.2 m, gravity 15 m/s^2
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->GravityScale = 1500.f / 980.f;
	Movement->JumpZVelocity = FMath::Sqrt(2.f * 1500.f * 120.f);
	Movement->AirControl = 0.35f;
	Movement->BrakingDecelerationWalking = 2000.f;
}

void AGalleryCharacter::EnsureInputAssets()
{
	if (Mapping)
	{
		return;
	}

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};

	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	GamepadLookAction = MakeAction(TEXT("IA_GamepadLook"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
	SprintAction = MakeAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean);

	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Gallery"));

	auto Swizzle = [this]()
	{
		UInputModifierSwizzleAxis* Modifier = NewObject<UInputModifierSwizzleAxis>(this);
		Modifier->Order = EInputAxisSwizzle::YXZ;
		return Modifier;
	};
	auto Negate = [this](bool bX, bool bY)
	{
		UInputModifierNegate* Modifier = NewObject<UInputModifierNegate>(this);
		Modifier->bX = bX;
		Modifier->bY = bY;
		Modifier->bZ = false;
		return Modifier;
	};

	Mapping->MapKey(MoveAction, EKeys::W).Modifiers.Add(Swizzle());
	{
		FEnhancedActionKeyMapping& S = Mapping->MapKey(MoveAction, EKeys::S);
		S.Modifiers.Add(Swizzle());
		S.Modifiers.Add(Negate(true, true));
	}
	Mapping->MapKey(MoveAction, EKeys::A).Modifiers.Add(Negate(true, true));
	Mapping->MapKey(MoveAction, EKeys::D);
	Mapping->MapKey(MoveAction, EKeys::Gamepad_Left2D).Modifiers.Add(NewObject<UInputModifierDeadZone>(this));

	Mapping->MapKey(LookAction, EKeys::Mouse2D).Modifiers.Add(Negate(false, true));
	{
		FEnhancedActionKeyMapping& Stick = Mapping->MapKey(GamepadLookAction, EKeys::Gamepad_Right2D);
		Stick.Modifiers.Add(NewObject<UInputModifierDeadZone>(this));
		Stick.Modifiers.Add(Negate(false, true));
	}

	Mapping->MapKey(JumpAction, EKeys::SpaceBar);
	Mapping->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);

	Mapping->MapKey(SprintAction, EKeys::LeftShift);
	Mapping->MapKey(SprintAction, EKeys::Gamepad_LeftThumbstick);
}

void AGalleryCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	EnsureInputAssets();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(Mapping);
			Subsystem->AddMappingContext(Mapping, 0);
		}
	}
}

void AGalleryCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	EnsureInputAssets();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("[GalleryCharacter] Enhanced Input component missing. Check Config/DefaultInput.ini."));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGalleryCharacter::Move);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGalleryCharacter::Look);
	Input->BindAction(GamepadLookAction, ETriggerEvent::Triggered, this, &AGalleryCharacter::GamepadLook);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	Input->BindAction(SprintAction, ETriggerEvent::Started, this, &AGalleryCharacter::StartSprint);
	Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &AGalleryCharacter::StopSprint);
}

void AGalleryCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}

	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix YawMatrix(Yaw);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Axis.X);
}

void AGalleryCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AGalleryCharacter::GamepadLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const float Scale = GamepadLookRate * GetWorld()->GetDeltaSeconds();
	AddControllerYawInput(Axis.X * Scale);
	AddControllerPitchInput(Axis.Y * Scale);
}

void AGalleryCharacter::StartSprint()
{
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void AGalleryCharacter::StopSprint()
{
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}
