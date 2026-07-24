// Copyright Solessfir. All Rights Reserved.

#pragma once

#include "Camera/CameraMode.h"
#include "CameraMode_FirstPerson.generated.h"

/**
 * Basic First-Person camera mode.
 */
UCLASS(Blueprintable)
class MODULARCAMERASYSTEM_API UCameraMode_FirstPerson : public UCameraMode
{
	GENERATED_BODY()

protected:
	virtual FVector GetPivotLocation_Implementation() const override;

	// Socket/bone on the character mesh the camera pivots to (e.g. head).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "First Person")
	FName HeadSocketName = FName("head");
};
