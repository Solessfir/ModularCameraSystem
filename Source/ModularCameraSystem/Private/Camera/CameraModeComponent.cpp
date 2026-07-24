// Copyright Solessfir. All Rights Reserved.

#include "Camera/CameraModeComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraMode.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraModeComponent)

void UCameraModeComponent::OnRegister()
{
	Super::OnRegister();

	if (!CameraModeStack)
	{
		CameraModeStack = NewObject<UCameraModeStack>(this);
		check(CameraModeStack);
	}
}

void UCameraModeComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	check(CameraModeStack);

	UpdateCameraModes();

	FCameraModeView CameraModeView;
	if (!CameraModeStack->EvaluateStack(DeltaTime, CameraModeView))
	{
		// A missing default mode (or an explicitly deactivated stack) must not produce the
		// zero-initialized FCameraModeView at world origin. Preserve normal camera behavior.
		Super::GetCameraView(DeltaTime, DesiredView);
		DesiredView.FOV += FieldOfViewOffset;
		FieldOfViewOffset = 0.f;
		return;
	}

	// Keep player controller in sync with the latest view.
	if (const APawn* TargetPawn = Cast<APawn>(GetTargetActor()))
	{
		if (APlayerController* PC = TargetPawn->GetController<APlayerController>())
		{
			PC->SetControlRotation(CameraModeView.ControlRotation);
		}
	}

	// Apply any offset that was added to the field of view.
	CameraModeView.FieldOfView += FieldOfViewOffset;
	FieldOfViewOffset = 0.f;

	// Keep camera component in sync with the latest view.
	SetWorldLocationAndRotation(CameraModeView.Location, CameraModeView.Rotation);
	FieldOfView = CameraModeView.FieldOfView;

	// Let UCameraComponent populate the complete FMinimalViewInfo. Because the component transform
	// and FOV were set above, this retains the mode result while also honoring additive offsets,
	// XR, first-person parameters, ortho settings, overscan, aspect constraints and motion vectors.
	Super::GetCameraView(DeltaTime, DesiredView);
}

UCameraModeComponent* UCameraModeComponent::FindCameraComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UCameraModeComponent>() : nullptr;
}

AActor* UCameraModeComponent::GetTargetActor() const
{
	return GetOwner();
}

void UCameraModeComponent::UpdateCameraModes()
{
	check(CameraModeStack);

	if (CameraModeStack->IsStackActivate())
	{
		if (const TSubclassOf<UCameraMode> CameraMode = DetermineCameraMode())
		{
			CameraModeStack->PushCameraMode(CameraMode);
		}
	}
}

TSubclassOf<UCameraMode> UCameraModeComponent::DetermineCameraMode_Implementation() const
{
	return CameraModeOverrideStack.Num() > 0 ? CameraModeOverrideStack.Last() : DefaultCameraMode;
}

void UCameraModeComponent::PushCameraMode(const TSubclassOf<UCameraMode> CameraModeClass)
{
	if (CameraModeClass)
	{
		CameraModeOverrideStack.Add(CameraModeClass);
	}
}

void UCameraModeComponent::PopCameraMode(const TSubclassOf<UCameraMode> CameraModeClass)
{
	CameraModeOverrideStack.RemoveSingle(CameraModeClass);
}

TSubclassOf<UCameraMode> UCameraModeComponent::GetCameraModeOverride() const
{
	return CameraModeOverrideStack.Num() > 0 ? CameraModeOverrideStack.Last() : nullptr;
}

void UCameraModeComponent::AddFieldOfViewOffset(const float FovOffset)
{
	FieldOfViewOffset += FovOffset;
}

void UCameraModeComponent::DrawDebug(UCanvas* Canvas) const
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	DisplayDebugManager.SetFont(GEngine->GetSmallFont());
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("Camera Mode Component: %s"), *GetNameSafe(GetTargetActor())));

	DisplayDebugManager.SetDrawColor(FColor::White);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   Location: %s"), *GetComponentLocation().ToCompactString()));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   Rotation: %s"), *GetComponentRotation().ToCompactString()));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   FOV: %.1f"), FieldOfView));

	check(CameraModeStack);
	CameraModeStack->DrawDebug(Canvas);
}

void UCameraModeComponent::GetBlendInfo(float& OutWeightOfTopLayer, FGameplayTag& OutTagOfTopLayer) const
{
	check(CameraModeStack);
	CameraModeStack->GetBlendInfo(OutWeightOfTopLayer, OutTagOfTopLayer);
}

UCameraMode* UCameraModeComponent::GetActiveCameraMode() const
{
	return CameraModeStack ? CameraModeStack->GetActiveCameraMode() : nullptr;
}
