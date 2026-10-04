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
	if (!CameraModeStack)
	{
		CameraModeStack = NewObject<UCameraModeStack>(this);
		check(CameraModeStack);
	}

	// Registration can auto-activate and query the camera from activation callbacks.
	Super::OnRegister();

	if (IsActive() && IsRegistered())
	{
		CameraModeStack->ActivateStack();
	}
	else
	{
		CameraModeStack->DeactivateStack();
	}
}

void UCameraModeComponent::OnUnregister()
{
	Super::OnUnregister();

	// Unregistration can run during GC, when Blueprint mode hooks are unsafe.
	if (CameraModeStack && !HasAnyFlags(RF_BeginDestroyed) && !IsUnreachable())
	{
		CameraModeStack->DeactivateStack();
	}
}

void UCameraModeComponent::Activate(bool bReset)
{
	Super::Activate(bReset);
	if (CameraModeStack && IsActive() && IsRegistered())
	{
		CameraModeStack->ActivateStack();
	}
}

void UCameraModeComponent::Deactivate()
{
	Super::Deactivate();
	if (CameraModeStack && !IsActive())
	{
		CameraModeStack->DeactivateStack();
	}
}

void UCameraModeComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	check(CameraModeStack);

	if (!bHasSavedCameraView)
	{
		SavedRelativeLocation = GetRelativeLocation();
		SavedRelativeRotation = GetRelativeRotation();
		SavedFieldOfView = FieldOfView;
	}

	// Activation delegates can query the view before Activate or Deactivate returns.
	if (IsActive() && IsRegistered())
	{
		CameraModeStack->ActivateStack();
	}
	else
	{
		CameraModeStack->DeactivateStack();
	}

	UpdateCameraModes();

	FCameraModeView CameraModeView;
	if (!CameraModeStack->EvaluateStack(DeltaTime, CameraModeView))
	{
		if (bHasSavedCameraView)
		{
			SetRelativeLocationAndRotation(SavedRelativeLocation, SavedRelativeRotation);
			FieldOfView = SavedFieldOfView;
			bHasSavedCameraView = false;
		}

		const float BaseFieldOfView = FieldOfView;
		FieldOfView += FieldOfViewOffset;
		FieldOfViewOffset = 0.f;
		Super::GetCameraView(DeltaTime, DesiredView);
		FieldOfView = BaseFieldOfView;
		return;
	}
	bHasSavedCameraView = true;

	// Keep player controller in sync with the latest view.
	if (const APawn* TargetPawn = Cast<APawn>(GetTargetActor()))
	{
		if (APlayerController* PC = TargetPawn->GetController<APlayerController>())
		{
			PC->SetControlRotation(CameraModeView.ControlRotation.GetNormalized());
		}
	}

	// Apply any offset that was added to the field of view.
	CameraModeView.FieldOfView += FieldOfViewOffset;
	FieldOfViewOffset = 0.f;

	// Keep camera component in sync with the latest view.
	SetWorldLocationAndRotation(CameraModeView.Location, CameraModeView.Rotation);
	FieldOfView = CameraModeView.FieldOfView;

	// Modes own the view rotation; preserve the base camera's other settings and XR handling.
	const bool bSavedUsePawnControlRotation = bUsePawnControlRotation;
	if (Cast<APawn>(GetOwner()))
	{
		bUsePawnControlRotation = false;
	}
	Super::GetCameraView(DeltaTime, DesiredView);
	bUsePawnControlRotation = bSavedUsePawnControlRotation;
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
		else
		{
			CameraModeStack->ClearStack();
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
