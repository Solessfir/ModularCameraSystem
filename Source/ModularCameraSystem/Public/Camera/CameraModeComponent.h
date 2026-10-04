// Copyright Solessfir. All Rights Reserved.

#pragma once

#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "CameraModeComponent.generated.h"

class UCanvas;
class UCameraMode;
class UCameraModeStack;
struct FMinimalViewInfo;

/**
 * Drop-in replacement for UCameraComponent. Drives a stack of UCameraMode instances
 * (TPP / FPP / ability overrides) and blends between them.
 *
 * Blueprint usage:
 *   1. Add this component to your pawn/character (or create a BP subclass).
 *   2. Set Default Camera Mode to a UCameraMode Blueprint (e.g. third-person).
 *   3. Call Push Camera Mode / Pop Camera Mode for temporary overrides (ADS, abilities, etc.) -
 *      multiple overrides can be active at once, the most recently pushed one wins.
 *   4. Optionally override Determine Camera Mode for fully custom selection logic.
 */
UCLASS(ClassGroup = "Camera", Meta = (BlueprintSpawnableComponent))
class MODULARCAMERASYSTEM_API UCameraModeComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;

	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;

	// Returns the target actor that the camera is looking at.
	UFUNCTION(BlueprintPure, Category = "Camera")
	virtual AActor* GetTargetActor() const;

	virtual void DrawDebug(UCanvas* Canvas) const;

	// Returns the camera component if one exists on the specified actor.
	UFUNCTION(BlueprintPure, Category = "Camera")
	static UCameraModeComponent* FindCameraComponent(const AActor* Actor);

	// Pushes a temporary camera mode override (ADS, ability cam) - most recently pushed one wins.
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void PushCameraMode(TSubclassOf<UCameraMode> CameraModeClass);

	// Removes a specific override pushed via Push Camera Mode - by class, not push order,
	// so it's safe to pop out of order (e.g. Ability A ends before Ability B, which pushed after it).
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void PopCameraMode(TSubclassOf<UCameraMode> CameraModeClass);

	// Currently active override (top of the stack), if any.
	UFUNCTION(BlueprintPure, Category = "Camera")
	TSubclassOf<UCameraMode> GetCameraModeOverride() const;

	// Picks the active camera mode class each frame; default: override stack top, else Default Camera Mode.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Camera")
	TSubclassOf<UCameraMode> DetermineCameraMode() const;
	virtual TSubclassOf<UCameraMode> DetermineCameraMode_Implementation() const;

	// Add an offset to the field of view. The offset is only for one frame, it gets cleared once it is applied.
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void AddFieldOfViewOffset(float FovOffset);

	// Gets the tag associated with the top layer and the blend weight of it.
	UFUNCTION(BlueprintCallable, Meta = (DisplayName = "Get Blend Info"), Category = "Camera")
	void GetBlendInfo(float& OutWeightOfTopLayer, FGameplayTag& OutTagOfTopLayer) const;

	// The currently dominant (top-of-stack) camera mode instance, if any - cast this to a specific
	// mode class (e.g. Camera Mode Third Person) to call mode-specific functions like Add Zoom Input.
	UFUNCTION(BlueprintPure, Category = "Camera")
	UCameraMode* GetActiveCameraMode() const;

	// C++ convenience: casts the active camera mode to T.
	template <typename T>
	T* GetActiveCameraModeAs() const { return Cast<T>(GetActiveCameraMode()); }

	// Camera mode used when no override is active.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	TSubclassOf<UCameraMode> DefaultCameraMode;

protected:
	virtual void UpdateCameraModes();

	// Temporary override stack pushed by Push Camera Mode (abilities, ADS, etc.) - last entry wins.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Camera")
	TArray<TSubclassOf<UCameraMode>> CameraModeOverrideStack;

	// Stack used to blend the camera modes.
	UPROPERTY()
	TObjectPtr<UCameraModeStack> CameraModeStack;

	// Offset applied to the field of view. The offset is only for one frame, it gets cleared once it is applied.
	float FieldOfViewOffset = 0.f;

	// Restore the component's original view when camera modes stop driving it.
	FVector SavedRelativeLocation = FVector::ZeroVector;
	FRotator SavedRelativeRotation = FRotator::ZeroRotator;
	float SavedFieldOfView = 0.f;
	bool bHasSavedCameraView = false;
};
