// Copyright Solessfir. All Rights Reserved.

#pragma once

#include "Camera/CameraMode.h"
#include "Curves/CurveFloat.h"
#include "Camera/CameraPenetrationAvoidanceFeeler.h"
#include "CameraMode_ThirdPerson.generated.h"

/**
 * Basic Third-Person camera mode with pitch-driven offset curve and wall-penetration avoidance.
 */
UCLASS(Abstract, Blueprintable)
class MODULARCAMERASYSTEM_API UCameraMode_ThirdPerson : public UCameraMode
{
	GENERATED_BODY()

public:
	UCameraMode_ThirdPerson();

	virtual void DrawDebug(UCanvas* Canvas) const override;

	virtual void OnActivation_Implementation() override;
	virtual void OnDeactivation_Implementation() override;

	// Scroll zoom: call with the mouse wheel axis value each tick.
	UFUNCTION(BlueprintCallable, Category = "Zoom")
	void AddZoomInput(float Delta);

	// Current smoothed zoom distance scale (1 = curve's authored distance, <1 = zoomed in, >1 = zoomed out).
	UFUNCTION(BlueprintPure, Category = "Zoom")
	float GetZoomDistanceScale() const { return ZoomDistanceScale; }

protected:
	virtual void UpdateView_Implementation(float DeltaTime) override;

	// Smooths the pivot's rotation/location toward where the camera actually ends up looking from/at -
	// lags the target, not the final camera position, so free-rotating the camera around the target has no lag.
	void ApplyCameraLag(float DeltaTime, const FVector& PivotLocation, FRotator& DesiredRotation, FVector& DesiredLocation);

	// Applies the pitch-driven Target Offset X/Y/Z curves (scaled by the current zoom) relative to
	// the (possibly lagged) desired rotation/location.
	void ApplyTargetOffsetFromRotation(const FRotator& DesiredRotation, FVector& DesiredLocation) const;

	void UpdateForTarget();

	void UpdatePreventPenetration(const float DeltaTime);

	void SetPenetrationNotificationState(bool bIsPenetrating, const TArray<UObject*>& AssistRecipients);

	void ResetPenetrationState();

	void PreventCameraPenetration(const AActor& ViewTarget, const FVector& SafeLoc, FVector& CameraLoc, const float& DeltaTime, float& DistBlockedPct, bool bSingleRayOnly);

	void SetTargetCrouchOffset(const FVector& NewTargetOffset);

	void UpdateCrouchOffset(const float DeltaTime);

	// Local-space offset from the target, evaluated per-axis using the view pitch.

	// Controls forward/back camera pos.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	FRuntimeFloatCurve TargetOffsetX;

	// Controls left/right camera pos.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	FRuntimeFloatCurve TargetOffsetY;

	// Controls up/down camera pos.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Third Person")
	FRuntimeFloatCurve TargetOffsetZ;

	// Alters the speed that a crouch offset is blended in or out.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ClampMin = 0.f, UIMin = 0.f, UIMax = 20.f), Category = "Third Person")
	float CrouchOffsetBlendMultiplier = 5.f;

	// Closest the zoom can bring the camera in (fraction of the curve's authored distance).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ClampMin = 0.05, UIMin = 0.05, UIMax = 1.0), Category = "Zoom")
	float MinZoomDistanceScale = 0.3f;

	// Farthest the zoom can push the camera out (multiple of the curve's authored distance).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ClampMin = 0.05, UIMin = 1.0, UIMax = 5.0), Category = "Zoom")
	float MaxZoomDistanceScale = 1.5f;

	// How much Add Zoom Input's Delta changes the target zoom scale per call (e.g. per scroll tick).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ClampMin = 0.f, UIMin = 0.f, UIMax = 0.5f), Category = "Zoom")
	float ZoomStepSize = 0.1f;

	// How quickly the camera distance catches up to the target zoom scale. 0 = snaps instantly.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ClampMin = 0.f, UIMin = 0.f, UIMax = 50.f), Category = "Zoom")
	float ZoomInterpSpeed = 10.f;

	// Current smoothed zoom distance scale, applied to the Target Offset curve output every frame.
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Zoom")
	float ZoomDistanceScale = 1.f;

	// Zoom scale Zoom Distance Scale is smoothing toward - set by Add Zoom Input.
	float TargetZoomDistanceScale = 1.f;

	// If true, the camera's target location smooths toward the pivot instead of snapping to it every frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Lag")
	bool bEnableCameraLag = false;

	// If true, the camera's target rotation smooths toward the pivot instead of snapping to it every frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Lag")
	bool bEnableCameraRotationLag = false;

	// Sub-steps the lag interpolation so it holds up at low/fluctuating frame rates, at a small extra cost.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Camera Lag")
	bool bUseCameraLagSubstepping = false;

	// How quickly the camera's location catches up to the pivot. Lower = more lag, 0 = no lag (snaps instantly).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (EditCondition = "bEnableCameraLag", ClampMin = 0.f, ClampMax = 1000.f, UIMin = 0.f, UIMax = 1000.f), Category = "Camera Lag")
	float CameraLagSpeed = 10.f;

	// How quickly the camera's rotation catches up to the pivot. Lower = more lag, 0 = no lag (snaps instantly).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (EditCondition = "bEnableCameraRotationLag", ClampMin = 0.f, ClampMax = 1000.f, UIMin = 0.f, UIMax = 1000.f), Category = "Camera Lag")
	float CameraRotationLagSpeed = 10.f;

	// Largest single sub-step used when Use Camera Lag Substepping is on.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Meta = (EditCondition = "bUseCameraLagSubstepping", ClampMin = 0.005f, ClampMax = 0.5f, UIMin = 0.005f, UIMax = 0.5f), Category = "Camera Lag")
	float CameraLagMaxTimeStep = 1.f / 60.f;

	// Caps how far the lagged location may fall behind the pivot. 0 = uncapped.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (EditCondition = "bEnableCameraLag", ClampMin = 0.f, UIMin = 0.f), Category = "Camera Lag")
	float CameraLagMaxDistance = 0.f;

public:
	// Seconds to blend the camera back out once no longer blocked.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ClampMin = 0.f, UIMin = 0.f, UIMax = 2.f), Category = "Collision")
	float PenetrationBlendInTime = 0.1f;

	// Seconds to blend the camera in when newly blocked.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ClampMin = 0.f, UIMin = 0.f, UIMax = 2.f), Category = "Collision")
	float PenetrationBlendOutTime = 0.15f;

	// If true, does collision checks to keep the camera out of the world.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	bool bPreventPenetration = true;

	// If true, try to detect nearby walls and move the camera in anticipation. Helps prevent popping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	bool bDoPredictiveAvoidance = true;

	// Collision channel the penetration-avoidance feelers sweep against.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Camera;

	// Runs feeler 0's sweep off the game thread; result up to 1 frame stale. Predictive feelers (1+) always run async.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	bool bRunAsyncCollision = false;

	// How far to keep the camera from a blocking surface once pushed in.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ClampMin = 0.f, UIMin = 0.f, UIMax = 50.f), Category = "Collision")
	float CollisionPushOutDistance = 2.f;

	// When the camera's distance is pushed into this percentage of its full distance due to penetration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f), Category = "Collision")
	float ReportPenetrationPercent = 0.f;

	// Feeler rays used to place the camera. Index 0 = main collision check, 1+ = predictive.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collision")
	TArray<FCameraPenetrationAvoidanceFeeler> PenetrationAvoidanceFeelers;

	UPROPERTY(Transient)
	float AimLineToDesiredPosBlockedPct;

	UPROPERTY(Transient)
	TArray<TObjectPtr<const AActor>> DebugActorsHitDuringCameraPenetration;

	// Kept unconditionally so this public class has the same layout in editor and game modules.
	mutable float LastDrawDebugTime = -MAX_FLT;

protected:
	FVector InitialCrouchOffset = FVector::ZeroVector;

	FVector TargetCrouchOffset = FVector::ZeroVector;

	float CrouchOffsetBlendPct = 1.f;

	FVector CurrentCrouchOffset = FVector::ZeroVector;

	bool bWasPenetratingTarget = false;

	TArray<TWeakObjectPtr<UObject>> PenetrationNotificationRecipients;

	// Previous frame's lag target - seeded on activation, so the first frame never lags from stale/zeroed data.
	FVector PreviousDesiredLocation = FVector::ZeroVector;
	FRotator PreviousDesiredRotation = FRotator::ZeroRotator;
};
