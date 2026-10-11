#include "HigataWalker.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace HigataWalkerLocal
{
	constexpr float CapsuleRadiusCm = 30.f;
	constexpr float CapsuleHalfHeightCm = 88.f;
	constexpr float EyeHeightCm = 160.f;
	/** Walking on wet sand and mud is slow; a brisk pace on the firm upper flat. */
	constexpr float WalkCmPerS = 140.f;
	constexpr float RunCmPerS = 320.f;
	constexpr float FlyCmPerS = 1500.f;
	/** Waders reach about 1 m; at 1 m of water the pace is down to a third. */
	constexpr double WaderDepthM = 1.0;
	constexpr double StickDegPerS = 150.0;

	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}

	template <typename TModifier>
	void AddModifier(FEnhancedActionKeyMapping& Mapping, UObject* Outer)
	{
		Mapping.Modifiers.Add(NewObject<TModifier>(Outer));
	}
}

namespace HWL = HigataWalkerLocal;

AHigataWalker::AHigataWalker()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(HWL::CapsuleRadiusCm, HWL::CapsuleHalfHeightCm);
	BaseEyeHeight = HWL::EyeHeightCm - HWL::CapsuleHalfHeightCm;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.0, 0.0, HWL::EyeHeightCm - HWL::CapsuleHalfHeightCm));
	Camera->bUsePawnControlRotation = true;

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->MaxWalkSpeed = HWL::WalkCmPerS;
	Movement->MaxFlySpeed = HWL::FlyCmPerS;
	Movement->BrakingDecelerationFlying = 4000.f;
	Movement->MaxStepHeight = 30.f;
	Movement->SetWalkableFloorAngle(50.f);
}

void AHigataWalker::BuildInput()
{
	using namespace HigataWalkerLocal;
	Mapping = NewObject<UInputMappingContext>(this, TEXT("HigataWalkerMapping"));
	MoveAction = MakeAction(this, TEXT("Move"), EInputActionValueType::Axis2D);
	LookMouseAction = MakeAction(this, TEXT("LookMouse"), EInputActionValueType::Axis2D);
	LookStickAction = MakeAction(this, TEXT("LookStick"), EInputActionValueType::Axis2D);
	RunAction = MakeAction(this, TEXT("Run"), EInputActionValueType::Boolean);
	FlyAction = MakeAction(this, TEXT("Fly"), EInputActionValueType::Boolean);
	UpDownAction = MakeAction(this, TEXT("UpDown"), EInputActionValueType::Axis1D);

	// Move: (x = right, y = forward). A key gives +x; swizzle puts it on y, negate flips it.
	AddModifier<UInputModifierSwizzleAxis>(Mapping->MapKey(MoveAction, EKeys::W), this);
	{
		FEnhancedActionKeyMapping& S = Mapping->MapKey(MoveAction, EKeys::S);
		AddModifier<UInputModifierSwizzleAxis>(S, this);
		AddModifier<UInputModifierNegate>(S, this);
	}
	AddModifier<UInputModifierNegate>(Mapping->MapKey(MoveAction, EKeys::A), this);
	Mapping->MapKey(MoveAction, EKeys::D);
	AddModifier<UInputModifierDeadZone>(Mapping->MapKey(MoveAction, EKeys::Gamepad_Left2D), this);

	Mapping->MapKey(LookMouseAction, EKeys::Mouse2D);
	AddModifier<UInputModifierDeadZone>(Mapping->MapKey(LookStickAction, EKeys::Gamepad_Right2D), this);

	Mapping->MapKey(RunAction, EKeys::LeftShift);
	Mapping->MapKey(RunAction, EKeys::Gamepad_LeftThumbstick);
	Mapping->MapKey(FlyAction, EKeys::F);
	Mapping->MapKey(FlyAction, EKeys::Gamepad_FaceButton_Top);

	Mapping->MapKey(UpDownAction, EKeys::E);
	AddModifier<UInputModifierNegate>(Mapping->MapKey(UpDownAction, EKeys::Q), this);
	Mapping->MapKey(UpDownAction, EKeys::Gamepad_RightTriggerAxis);
	AddModifier<UInputModifierNegate>(Mapping->MapKey(UpDownAction, EKeys::Gamepad_LeftTriggerAxis), this);
}

void AHigataWalker::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!Mapping)
	{
		BuildInput();
	}
	if (const APlayerController* Pc = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Pc->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(Mapping, 0);
		}
	}
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("[Higata] the input component is not an Enhanced Input component (see Config/DefaultInput.ini)"));
		return;
	}
	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHigataWalker::Move);
	Input->BindAction(LookMouseAction, ETriggerEvent::Triggered, this, &AHigataWalker::LookMouse);
	Input->BindAction(LookStickAction, ETriggerEvent::Triggered, this, &AHigataWalker::LookStick);
	Input->BindAction(RunAction, ETriggerEvent::Started, this, &AHigataWalker::StartRun);
	Input->BindAction(RunAction, ETriggerEvent::Completed, this, &AHigataWalker::StopRun);
	Input->BindAction(FlyAction, ETriggerEvent::Started, this, &AHigataWalker::ToggleFly);
	Input->BindAction(UpDownAction, ETriggerEvent::Triggered, this, &AHigataWalker::UpDown);
}

void AHigataWalker::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}
	const FRotator Yaw(0.0, Controller->GetControlRotation().Yaw, 0.0);
	const FRotationMatrix Frame(Yaw);
	AddMovementInput(Frame.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(Frame.GetUnitAxis(EAxis::Y), Axis.X);
}

void AHigataWalker::LookMouse(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void AHigataWalker::LookStick(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const double Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016;
	AddControllerYawInput(Axis.X * HWL::StickDegPerS * Dt);
	AddControllerPitchInput(-Axis.Y * HWL::StickDegPerS * Dt);
}

void AHigataWalker::StartRun(const FInputActionValue& Value)
{
	bRunning = true;
}

void AHigataWalker::StopRun(const FInputActionValue& Value)
{
	bRunning = false;
}

bool AHigataWalker::IsFlying() const
{
	return GetCharacterMovement()->MovementMode == MOVE_Flying;
}

void AHigataWalker::ToggleFly(const FInputActionValue& Value)
{
	GetCharacterMovement()->SetMovementMode(IsFlying() ? MOVE_Falling : MOVE_Flying);
}

void AHigataWalker::UpDown(const FInputActionValue& Value)
{
	if (IsFlying())
	{
		AddMovementInput(FVector::UpVector, Value.Get<float>());
	}
}

double AHigataWalker::GetWaterDepthM() const
{
	const double FeetM = (GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) / 100.0;
	return FMath::Max(0.0, WaterLevelM - FeetM);
}

void AHigataWalker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Wading: the pace falls off with the water's depth, to a third at the top of the waders.
	const double Depth = GetWaterDepthM();
	const double Drag = FMath::Clamp(1.0 - (Depth / HWL::WaderDepthM) * (2.0 / 3.0), 1.0 / 3.0, 1.0);
	GetCharacterMovement()->MaxWalkSpeed = static_cast<float>((bRunning ? HWL::RunCmPerS : HWL::WalkCmPerS) * Drag);
}
