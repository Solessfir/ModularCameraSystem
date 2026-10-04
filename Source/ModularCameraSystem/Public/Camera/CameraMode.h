// Copyright Solessfir. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "CameraMode.generated.h"

class AActor;
class UCanvas;
class UCameraModeComponent;

// Blend function used for transitioning between camera modes.
UENUM(BlueprintType)
enum class ECameraModeBlendFunction : uint8
{
	// Does a simple linear interpolation.
	Linear,

	// Immediately accelerates, but smoothly decelerates into the target. Ease amount controlled by the exponent.
	EaseIn,

	// Smoothly accelerates but does not decelerate into the target. Ease amount controlled by the exponent.
	EaseOut,

	// Smoothly accelerates and decelerates. Ease amount controlled by the exponent.
	EaseInOut,

	COUNT UMETA(Hidden)
};

// View data produced by a camera mode that is used to blend camera modes.
struct MODULARCAMERASYSTEM_API FCameraModeView
{
	FCameraModeView();

	void Blend(const FCameraModeView& Other, const float OtherWeight);

	FVector Location;

	FRotator Rotation;

	FRotator ControlRotation;

	float FieldOfView;
};

/**
 * Base class for all camera modes. Create Blueprint subclasses to author modes in-editor
 * Override Get Pivot Location / Get Pivot Rotation / Update View in Blueprint for fully custom modes.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class MODULARCAMERASYSTEM_API UCameraMode : public UObject
{
	GENERATED_BODY()

	friend class UCameraModeStack;

public:
	UCameraMode();

	UFUNCTION(BlueprintPure, Category = "Camera")
	UCameraModeComponent* GetCameraModeComponent() const;

	virtual UWorld* GetWorld() const override;

	UFUNCTION(BlueprintPure, Category = "Camera")
	AActor* GetTargetActor() const;

	const FCameraModeView& GetCameraModeView() const;

	// Called when this camera mode is activated on the camera mode stack.
	UFUNCTION(BlueprintNativeEvent, Category = "Camera")
	void OnActivation();
	virtual void OnActivation_Implementation() {}

	// Called when this camera mode is deactivated on the camera mode stack.
	UFUNCTION(BlueprintNativeEvent, Category = "Camera")
	void OnDeactivation();
	virtual void OnDeactivation_Implementation() {}

	void UpdateCameraMode(const float DeltaTime);

	// Forces the next blend update to snap instantly instead of interpolating.
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ResetInterpolation();

	UFUNCTION(BlueprintPure, Category = "Camera")
	float GetBlendTime() const;

	UFUNCTION(BlueprintPure, Category = "Camera")
	float GetBlendWeight() const;

	void SetBlendWeight(const float Weight);

	UFUNCTION(BlueprintPure, Category = "Camera")
	FGameplayTag GetCameraTypeTag() const;

	virtual void DrawDebug(UCanvas* Canvas) const;

protected:
	// World-space pivot the camera is based on (eye height for characters by default).
	UFUNCTION(BlueprintNativeEvent, Category = "Camera")
	FVector GetPivotLocation() const;
	virtual FVector GetPivotLocation_Implementation() const;

	// World-space rotation the camera is based on (pawn view rotation by default).
	UFUNCTION(BlueprintNativeEvent, Category = "Camera")
	FRotator GetPivotRotation() const;
	virtual FRotator GetPivotRotation_Implementation() const;

	// Compute View location / rotation / FOV for this frame.
	UFUNCTION(BlueprintNativeEvent, Category = "Camera")
	void UpdateView(float DeltaTime);
	virtual void UpdateView_Implementation(float DeltaTime);

	virtual void UpdateBlending(const float DeltaTime);

	// A tag that can be queried by gameplay code that cares when a kind of camera mode is active
	// without having to ask about a specific mode (e.g., when aiming down sights).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blending")
	FGameplayTag CameraTypeTag;

	// View output produced by the camera mode.
	FCameraModeView View;

	// Keep the first blend on the current angle branch without replacing activation-authored view values.
	FRotator PendingRotationReference;
	FRotator PendingControlRotationReference;
	bool bHasPendingRotationReference = false;
	bool bIsActiveOnStack = false;

	// The horizontal field of view (in degrees).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (UIMin = 5.f, UIMax = 170.f, ClampMin = 5.f, ClampMax = 170.f), Category = "View")
	float FieldOfView;

	// Minimum view pitch (in degrees).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (UIMin = -89.9f, UIMax = 89.9f, ClampMin = -89.9f, ClampMax = 89.9f), Category = "View")
	float ViewPitchMin;

	// Maximum view pitch (in degrees).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (UIMin = -89.9f, UIMax = 89.9f, ClampMin = -89.9f, ClampMax = 89.9f), Category = "View")
	float ViewPitchMax;

	// How long it takes to blend in this mode.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (ClampMin = 0.f, UIMin = 0.f, UIMax = 5.f), Category = "Blending")
	float BlendTime;

	// Function used for blending.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blending")
	ECameraModeBlendFunction BlendFunction;

	// Exponent used by blend functions to control the shape of the curve.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (ClampMin = 0.f, UIMin = 0.f, UIMax = 10.f), Category = "Blending")
	float BlendExponent;

	// Linear blend alpha used to determine the blend weight.
	float BlendAlpha;

	// Blend weight calculated using the blend alpha and function.
	float BlendWeight;

	// If true, skips all interpolation and puts the camera in the ideal location. Automatically set to false next frame.
	UPROPERTY(Transient)
	uint32 bResetInterpolation : 1;

	// Helpers for Blueprint Update View implementations to write into the mode's view.
	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "NewLocation", BlueprintProtected = true), Category = "Camera|View")
	void SetViewLocation(const FVector& NewLocation);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "NewRotation",BlueprintProtected = true), Category = "Camera|View")
	void SetViewRotation(const FRotator& NewRotation);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "NewControlRotation",BlueprintProtected = true), Category = "Camera|View")
	void SetViewControlRotation(const FRotator& NewControlRotation);

	UFUNCTION(BlueprintCallable, Meta = (BlueprintProtected = true), Category = "Camera|View")
	void SetViewFieldOfView(const float NewFieldOfView);

	UFUNCTION(BlueprintPure, Meta = (BlueprintProtected = true), Category = "Camera|View")
	FVector GetViewLocation() const;

	UFUNCTION(BlueprintPure, Meta = (BlueprintProtected = true), Category = "Camera|View")
	FRotator GetViewRotation() const;

	UFUNCTION(BlueprintPure, Meta = (BlueprintProtected = true), Category = "Camera|View")
	float GetViewFieldOfView() const;
};

/**
 * Stack used for blending camera modes.
 * PushCameraMode moves a mode to top and blends it in over BlendTime.
 */
UCLASS()
class MODULARCAMERASYSTEM_API UCameraModeStack : public UObject
{
	GENERATED_BODY()

public:
	UCameraModeStack();

	void ActivateStack();

	void DeactivateStack();

	void ClearStack();

	bool IsStackActivate() const;

	void PushCameraMode(const TSubclassOf<UCameraMode> CameraModeClass);

	bool EvaluateStack(const float DeltaTime, FCameraModeView& OutCameraModeView);

	void DrawDebug(UCanvas* Canvas) const;

	// Gets the tag associated with the top layer and the blend weight of it.
	void GetBlendInfo(float& OutWeightOfTopLayer, FGameplayTag& OutTagOfTopLayer) const;

	// The currently dominant (top-of-stack) camera mode instance, if any
	// E.g., to cast and call mode-specific functions like Camera Mode Third Person's Add Zoom Input.
	UCameraMode* GetActiveCameraMode() const;

protected:
	UCameraMode* GetCameraModeInstance(const TSubclassOf<UCameraMode> CameraModeClass);

	bool UpdateStack(const float DeltaTime);

	void ActivateCameraMode(UCameraMode* CameraMode);
	void DeactivateCameraMode(UCameraMode* CameraMode);

	void BlendStack(FCameraModeView& OutCameraModeView) const;

	bool bIsActive;
	bool bHasEvaluatedView = false;
	uint32 StackRevision = 0;

	UPROPERTY()
	TArray<TObjectPtr<UCameraMode>> CameraModeInstances;

	UPROPERTY()
	TArray<TObjectPtr<UCameraMode>> CameraModeStack;
};
