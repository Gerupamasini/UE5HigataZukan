#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HigataWalker.generated.h"

class AHigataFlat;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * The player on foot: eye height 1.6 m, walking pace on mud, slower the deeper the water (waders reach 1 m).
 * Input is Enhanced Input built in code (no assets): keyboard + mouse and gamepad.
 *   Move  WASD / left stick      Look  mouse / right stick      Run  Shift / left stick click
 *   Fly   F / Y (top face)       Up/down while flying  E, Q / right, left trigger
 *   Net   left mouse button / X (left face)
 */
UCLASS()
class HIGATAZUKAN_API AHigataWalker : public ACharacter
{
	GENERATED_BODY()

public:
	AHigataWalker();

	virtual void Tick(float DeltaSeconds) override;

	/** The sea level in metres (set every frame by AHigataFlat). */
	void SetWaterLevelM(double InLevelM) { WaterLevelM = InLevelM; }

	/** Water above the feet in metres (0 on dry ground). */
	double GetWaterDepthM() const;

	bool IsFlying() const;

	/** The field the net swings in. */
	void SetFlat(AHigataFlat* InFlat);

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Mapping;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookMouseAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookStickAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RunAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlyAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> UpDownAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> NetAction;

	TWeakObjectPtr<AHigataFlat> Flat;

	double WaterLevelM = -100.0;
	bool bRunning = false;

	void BuildInput();
	void Move(const FInputActionValue& Value);
	void LookMouse(const FInputActionValue& Value);
	void LookStick(const FInputActionValue& Value);
	void StartRun(const FInputActionValue& Value);
	void StopRun(const FInputActionValue& Value);
	void ToggleFly(const FInputActionValue& Value);
	void UpDown(const FInputActionValue& Value);
	void SwingNet(const FInputActionValue& Value);
};
