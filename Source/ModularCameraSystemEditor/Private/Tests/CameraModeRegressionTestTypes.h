// Copyright Solessfir. All Rights Reserved.

#pragma once

#include "Camera/CameraMode.h"
#include "Camera/CameraMode_ThirdPerson.h"
#include "Camera/CameraAssistInterface.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "CameraModeRegressionTestTypes.generated.h"

class UActorComponent;

UCLASS(Abstract, Transient, NotBlueprintable, NotBlueprintType, HideDropdown)
class UCameraModeRegressionTestBase : public UCameraMode
{
	GENERATED_BODY()

public:
	UCameraModeRegressionTestBase();

	virtual void OnActivation_Implementation() override
	{
		++ActivationCount;
		View.Rotation = FRotator::ZeroRotator;
		View.ControlRotation = FRotator::ZeroRotator;
		if (OnActivate)
		{
			OnActivate();
		}
	}

	virtual void OnDeactivation_Implementation() override
	{
		++DeactivationCount;
		if (OnDeactivate)
		{
			OnDeactivate();
		}
	}

	int32 ActivationCount = 0;
	int32 DeactivationCount = 0;
	TFunction<void()> OnActivate;
	TFunction<void()> OnUpdate;
	TFunction<void()> OnDeactivate;
	FVector FixedLocation = FVector::ZeroVector;
	FRotator FixedRotation = FRotator(10.f, 20.f, 0.f);
	FRotator FixedControlRotation = FRotator(30.f, 80.f, 0.f);
	float FixedFieldOfView = 60.f;

protected:
	virtual void UpdateView_Implementation(float DeltaTime) override;
};

UCLASS(Transient, NotBlueprintable, NotBlueprintType, HideDropdown)
class UCameraModeRegressionTestA : public UCameraModeRegressionTestBase
{
	GENERATED_BODY()
};

UCLASS(Transient, NotBlueprintable, NotBlueprintType, HideDropdown)
class UCameraModeRegressionTestB : public UCameraModeRegressionTestBase
{
	GENERATED_BODY()

public:
	UCameraModeRegressionTestB();
};

UCLASS(Transient, NotBlueprintable, NotBlueprintType, HideDropdown)
class UCameraModeRegressionTestC : public UCameraModeRegressionTestBase
{
	GENERATED_BODY()

public:
	UCameraModeRegressionTestC();
};

UCLASS(Transient, NotBlueprintable, NotBlueprintType, HideDropdown)
class UCameraModeRegressionTestLag : public UCameraMode_ThirdPerson
{
	GENERATED_BODY()

public:
	UCameraModeRegressionTestLag();

	FVector GetCurrentPivot() const { return GetPivotLocation() + CurrentCrouchOffset; }
	FVector GetCrouchOffset() const { return CurrentCrouchOffset; }
	void SetCrouchTarget(const FVector& Offset) { SetTargetCrouchOffset(Offset); }
	void UpdateCrouch(float DeltaTime) { UpdateCrouchOffset(DeltaTime); }
	void SetMaximumPitch(float Pitch) { ViewPitchMax = Pitch; }
	void SetZoomRange(float MinScale, float MaxScale)
	{
		MinZoomDistanceScale = MinScale;
		MaxZoomDistanceScale = MaxScale;
	}

	void EnableLag()
	{
		bEnableCameraLag = true;
		bEnableCameraRotationLag = true;
	}

	void EnableSubsteppedLag(float MaxTimeStep)
	{
		EnableLag();
		bUseCameraLagSubstepping = true;
		CameraLagMaxTimeStep = MaxTimeStep;
		CameraLagSpeed = 10.f;
		CameraRotationLagSpeed = 10.f;
	}

	void ApplyLag(float DeltaTime, const FVector& PivotLocation, FRotator& Rotation, FVector& Location)
	{
		ApplyCameraLag(DeltaTime, PivotLocation, Rotation, Location);
	}

	void NotifyPenetration(bool bIsPenetrating, const TArray<UObject*>& Recipients)
	{
		SetPenetrationNotificationState(bIsPenetrating, Recipients);
	}
};

UCLASS(Transient, NotBlueprintable, NotBlueprintType, HideDropdown)
class UCameraActivationRegressionTestReceiver : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void OnCameraActivated(UActorComponent* Component, bool bReset);

	UFUNCTION()
	void OnCameraDeactivated(UActorComponent* Component);

	int32 ActivationCount = 0;
	int32 DeactivationCount = 0;
	float BlendWeight = 0.f;
	FGameplayTag BlendTag;
	FVector ViewLocation = FVector::ZeroVector;
	float ViewFieldOfView = 0.f;
};

UCLASS(Transient, NotBlueprintable, NotBlueprintType, HideDropdown)
class ACameraAssistRegressionTestActor : public AActor, public ICameraAssistInterface
{
	GENERATED_BODY()

public:
	virtual AActor* GetCameraPreventPenetrationTarget_Implementation() const override { return PenetrationTarget; }
	virtual void GetIgnoredActorsForCameraPenetration_Implementation(TArray<AActor*>& OutActors) const override
	{
		++IgnoreQueryCount;
		for (AActor* Actor : IgnoredActors)
		{
			OutActors.Add(Actor);
		}
	}

	virtual void OnCameraPenetratingTarget_Implementation() override
	{
		++EnterCount;
		if (OnEnter)
		{
			OnEnter();
		}
	}
	virtual void OnCameraStoppedPenetratingTarget_Implementation() override
	{
		++ExitCount;
		if (OnExit)
		{
			OnExit();
		}
	}

	int32 EnterCount = 0;
	int32 ExitCount = 0;
	mutable int32 IgnoreQueryCount = 0;
	TFunction<void()> OnEnter;
	TFunction<void()> OnExit;

	UPROPERTY()
	TObjectPtr<AActor> PenetrationTarget;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> IgnoredActors;
};

UCLASS(Transient, NotBlueprintable, NotBlueprintType, HideDropdown)
class ACameraRegressionTestMeshlessCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACameraRegressionTestMeshlessCharacter(const FObjectInitializer& ObjectInitializer);
};
