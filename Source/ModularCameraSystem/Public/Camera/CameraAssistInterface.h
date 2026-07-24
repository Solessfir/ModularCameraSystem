// Copyright Solessfir. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CameraAssistInterface.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class UCameraAssistInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Optional Interface to customize penetration behavior.
 * Implement **Camera Assist Interface** on the Owning Pawn, its Controller, or a custom target returned via
 * `GetCameraPreventPenetrationTarget` if you need to customize penetration behavior.
 */
class ICameraAssistInterface
{
	GENERATED_BODY()

public:
	// Focal target for penetration avoidance. Return null to keep using the view target.
	// Override when the view target isn't the root actor you need to keep in frame.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Camera")
	AActor* GetCameraPreventPenetrationTarget() const;

	// Actors the camera is allowed to penetrate (always ignored by penetration traces).
	// Useful for the pawn, a vehicle, a collection of view targets, etc.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Camera")
	void GetIgnoredActorsForCameraPenetration(TArray<AActor*>& OutActorsAllowPenetration) const;

	// Called (once) when the camera starts penetrating the focal target - i.e. this actor itself,
	// unless Get Camera Prevent Penetration Target redirects it. Typically: hide own mesh via GetMesh().
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Camera")
	void OnCameraPenetratingTarget();

	// Called (once) when the camera stops penetrating, after a prior On Camera Penetrating Target.
	// Typically: show the mesh hidden in that call.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Camera")
	void OnCameraStoppedPenetratingTarget();
};
