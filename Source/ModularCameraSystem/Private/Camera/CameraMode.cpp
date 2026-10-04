// Copyright Solessfir. All Rights Reserved.

#include "Camera/CameraMode.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "GameFramework/Character.h"
#include "Camera/CameraModeComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraMode)

namespace CameraMode_Statics
{
	constexpr float DefaultFOV = 90.f;
	constexpr float DefaultPitchMin = -75.f;
	constexpr float DefaultPitchMax = 65.f;
}

FCameraModeView::FCameraModeView()
	: Location(FVector::ZeroVector)
	, Rotation(FRotator::ZeroRotator)
	, ControlRotation(FRotator::ZeroRotator)
	, FieldOfView(CameraMode_Statics::DefaultFOV)
{
}

void FCameraModeView::Blend(const FCameraModeView& Other, const float OtherWeight)
{
	if (OtherWeight <= 0.f)
	{
		return;
	}

	if (OtherWeight >= 1.f)
	{
		*this = Other;
		return;
	}

	Location = FMath::Lerp(Location, Other.Location, OtherWeight);

	const FRotator DeltaRotation = (Other.Rotation - Rotation).GetNormalized();
	Rotation = Rotation + OtherWeight * DeltaRotation;

	const FRotator DeltaControlRotation = (Other.ControlRotation - ControlRotation).GetNormalized();
	ControlRotation = ControlRotation + (OtherWeight * DeltaControlRotation);

	FieldOfView = FMath::Lerp(FieldOfView, Other.FieldOfView, OtherWeight);
}

UCameraMode::UCameraMode()
{
	FieldOfView = CameraMode_Statics::DefaultFOV;
	ViewPitchMin = CameraMode_Statics::DefaultPitchMin;
	ViewPitchMax = CameraMode_Statics::DefaultPitchMax;

	BlendTime = 0.5f;
	BlendFunction = ECameraModeBlendFunction::EaseOut;
	BlendExponent = 4.f;
	BlendAlpha = 1.f;
	BlendWeight = 1.f;

	bResetInterpolation = false;
}

UCameraModeComponent* UCameraMode::GetCameraModeComponent() const
{
	return CastChecked<UCameraModeComponent>(GetOuter());
}

UWorld* UCameraMode::GetWorld() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? nullptr : GetOuter()->GetWorld();
}

AActor* UCameraMode::GetTargetActor() const
{
	const UCameraModeComponent* CameraComponent = GetCameraModeComponent();
	return CameraComponent->GetTargetActor();
}

const FCameraModeView& UCameraMode::GetCameraModeView() const
{
	return View;
}

FVector UCameraMode::GetPivotLocation_Implementation() const
{
	const AActor* TargetActor = GetTargetActor();
	check(TargetActor);

	if (const APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		// Height adjustments for characters to account for crouching.
		if (const ACharacter* TargetCharacter = Cast<ACharacter>(TargetPawn))
		{
			const ACharacter* TargetCharacterCDO = TargetCharacter->GetClass()->GetDefaultObject<ACharacter>();
			check(TargetCharacterCDO);

			const UCapsuleComponent* CapsuleComp = TargetCharacter->GetCapsuleComponent();
			check(CapsuleComp);

			const UCapsuleComponent* CapsuleCompCDO = TargetCharacterCDO->GetCapsuleComponent();
			check(CapsuleCompCDO);

			const float DefaultHalfHeight = CapsuleCompCDO->GetUnscaledCapsuleHalfHeight();
			const float ActualHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();
			const float HeightAdjustment = (DefaultHalfHeight - ActualHalfHeight) + TargetCharacterCDO->BaseEyeHeight;

			return TargetCharacter->GetActorLocation() + FVector::UpVector * HeightAdjustment;
		}

		return TargetPawn->GetPawnViewLocation();
	}

	return TargetActor->GetActorLocation();
}

FRotator UCameraMode::GetPivotRotation_Implementation() const
{
	const AActor* TargetActor = GetTargetActor();
	check(TargetActor);

	if (const APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		return TargetPawn->GetViewRotation();
	}

	return TargetActor->GetActorRotation();
}

void UCameraMode::UpdateCameraMode(const float DeltaTime)
{
	const FRotator PreviousRotation = bHasPendingRotationReference ? PendingRotationReference : View.Rotation;
	const FRotator PreviousControlRotation = bHasPendingRotationReference ? PendingControlRotationReference : View.ControlRotation;
	bHasPendingRotationReference = false;
	UpdateView(DeltaTime);
	// Keep each mode on a continuous angle branch so stack reordering cannot change the blend's path.
	View.Rotation = PreviousRotation + (View.Rotation - PreviousRotation).GetNormalized();
	View.ControlRotation = PreviousControlRotation + (View.ControlRotation - PreviousControlRotation).GetNormalized();
	UpdateBlending(DeltaTime);
}

void UCameraMode::ResetInterpolation()
{
	bResetInterpolation = true;
}

float UCameraMode::GetBlendTime() const
{
	return BlendTime;
}

float UCameraMode::GetBlendWeight() const
{
	return BlendWeight;
}

void UCameraMode::UpdateView_Implementation(const float DeltaTime)
{
	const FVector PivotLocation = GetPivotLocation();
	FRotator PivotRotation = GetPivotRotation();

	PivotRotation.Pitch = FMath::ClampAngle(PivotRotation.Pitch, ViewPitchMin, ViewPitchMax);

	View.Location = PivotLocation;
	View.Rotation = PivotRotation;
	View.ControlRotation = View.Rotation;
	View.FieldOfView = FieldOfView;
}

void UCameraMode::SetBlendWeight(const float Weight)
{
	BlendWeight = FMath::Clamp(Weight, 0.f, 1.f);

	// Since we're setting the blend weight directly, we need to calculate the blend alpha to account for the blend function.
	const float InvExponent = BlendExponent > 0.f ? 1.f / BlendExponent : 1.f;

	switch (BlendFunction)
	{
		case ECameraModeBlendFunction::Linear:
			BlendAlpha = BlendWeight;
			break;

		case ECameraModeBlendFunction::EaseIn:
			BlendAlpha = FMath::InterpEaseIn(0.f, 1.f, BlendWeight, InvExponent);
			break;

		case ECameraModeBlendFunction::EaseOut:
			BlendAlpha = FMath::InterpEaseOut(0.f, 1.f, BlendWeight, InvExponent);
			break;

		case ECameraModeBlendFunction::EaseInOut:
			BlendAlpha = FMath::InterpEaseInOut(0.f, 1.f, BlendWeight, InvExponent);
			break;

		default:
			checkf(false, TEXT("SetBlendWeight: Invalid BlendFunction [%d]"), static_cast<uint8>(BlendFunction));
			break;
	}
}

FGameplayTag UCameraMode::GetCameraTypeTag() const
{
	return CameraTypeTag;
}

void UCameraMode::UpdateBlending(const float DeltaTime)
{
	if (BlendTime > 0.f)
	{
		BlendAlpha += DeltaTime / BlendTime;
		BlendAlpha = FMath::Min(BlendAlpha, 1.f);
	}
	else
	{
		BlendAlpha = 1.f;
	}

	const float Exponent = BlendExponent > 0.f ? BlendExponent : 1.f;

	switch (BlendFunction)
	{
		case ECameraModeBlendFunction::Linear:
			BlendWeight = BlendAlpha;
			break;

		case ECameraModeBlendFunction::EaseIn:
			BlendWeight = FMath::InterpEaseIn(0.f, 1.f, BlendAlpha, Exponent);
			break;

		case ECameraModeBlendFunction::EaseOut:
			BlendWeight = FMath::InterpEaseOut(0.f, 1.f, BlendAlpha, Exponent);
			break;

		case ECameraModeBlendFunction::EaseInOut:
			BlendWeight = FMath::InterpEaseInOut(0.f, 1.f, BlendAlpha, Exponent);
			break;

		default:
			checkf(false, TEXT("UpdateBlending: Invalid BlendFunction [%d]"), static_cast<uint8>(BlendFunction));
			break;
	}

	if (bResetInterpolation)
	{
		BlendAlpha = 1.f;
		BlendWeight = 1.f;
		bResetInterpolation = false;
	}
}

void UCameraMode::DrawDebug(UCanvas* Canvas) const
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	DisplayDebugManager.SetDrawColor(FColor::White);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("    Camera Mode: %s, Blend Weight: %.2f"), *GetName(), BlendWeight));
}

void UCameraMode::SetViewLocation(const FVector& NewLocation)
{
	View.Location = NewLocation;
}

void UCameraMode::SetViewRotation(const FRotator& NewRotation)
{
	View.Rotation = NewRotation;
}

void UCameraMode::SetViewControlRotation(const FRotator& NewControlRotation)
{
	View.ControlRotation = NewControlRotation;
}

void UCameraMode::SetViewFieldOfView(const float NewFieldOfView)
{
	View.FieldOfView = NewFieldOfView;
}

FVector UCameraMode::GetViewLocation() const
{
	return View.Location;
}

FRotator UCameraMode::GetViewRotation() const
{
	return View.Rotation;
}

float UCameraMode::GetViewFieldOfView() const
{
	return View.FieldOfView;
}

UCameraModeStack::UCameraModeStack()
{
	bIsActive = true;
}

void UCameraModeStack::ActivateStack()
{
	if (!bIsActive)
	{
		bIsActive = true;
		++StackRevision;

		const TArray<TObjectPtr<UCameraMode>> Modes = CameraModeStack;
		for (UCameraMode* CameraMode : Modes)
		{
			if (!bIsActive)
			{
				break;
			}
			if (CameraModeStack.Contains(CameraMode))
			{
				ActivateCameraMode(CameraMode);
			}
		}
	}
}

void UCameraModeStack::DeactivateStack()
{
	if (bIsActive)
	{
		bIsActive = false;
		++StackRevision;

		const TArray<TObjectPtr<UCameraMode>> Modes = CameraModeStack;
		for (UCameraMode* CameraMode : Modes)
		{
			if (bIsActive)
			{
				break;
			}
			DeactivateCameraMode(CameraMode);
		}
	}
}

bool UCameraModeStack::IsStackActivate() const
{
	return bIsActive;
}

void UCameraModeStack::ClearStack()
{
	if (CameraModeStack.IsEmpty())
	{
		return;
	}

	TArray<TObjectPtr<UCameraMode>> RemovedModes = MoveTemp(CameraModeStack);
	bHasEvaluatedView = false;
	++StackRevision;
	for (UCameraMode* CameraMode : RemovedModes)
	{
		if (!bIsActive || !CameraModeStack.Contains(CameraMode))
		{
			DeactivateCameraMode(CameraMode);
		}
	}
}

void UCameraModeStack::PushCameraMode(const TSubclassOf<UCameraMode> CameraModeClass)
{
	if (!CameraModeClass)
	{
		return;
	}

	UCameraMode* CameraMode = GetCameraModeInstance(CameraModeClass);
	check(CameraMode);

	int32 StackSize = CameraModeStack.Num();

	if (StackSize > 0 && CameraModeStack[0] == CameraMode)
	{
		// Already top of stack.
		return;
	}

	// See if it's already in the stack and remove it. Figure out how much it was contributing to the stack.
	int32 ExistingStackIndex = INDEX_NONE;
	float ExistingStackContribution = 1.f;

	for (int32 StackIndex = 0; StackIndex < StackSize; ++StackIndex)
	{
		if (CameraModeStack[StackIndex] == CameraMode)
		{
			ExistingStackIndex = StackIndex;
			ExistingStackContribution *= CameraMode->GetBlendWeight();
			break;
		}
		ExistingStackContribution *= 1.f - CameraModeStack[StackIndex]->GetBlendWeight();
	}

	if (ExistingStackIndex != INDEX_NONE)
	{
		// Preserve the other layers' contributions when this mode moves above them.
		float RemainingContribution = 1.f;
		for (int32 StackIndex = 0; StackIndex < ExistingStackIndex; ++StackIndex)
		{
			UCameraMode* OtherMode = CameraModeStack[StackIndex];
			const float PreviousWeight = OtherMode->GetBlendWeight();
			const float RemainingWithoutMode = RemainingContribution - ExistingStackContribution;
			const float NewWeight = RemainingWithoutMode > 0.f
				? PreviousWeight * RemainingContribution / RemainingWithoutMode
				: 0.f;
			OtherMode->SetBlendWeight(NewWeight);
			RemainingContribution *= 1.f - PreviousWeight;
		}

		CameraModeStack.RemoveAt(ExistingStackIndex);
		StackSize--;
	}
	else
	{
		ExistingStackContribution = 0.f;
		CameraMode->bResetInterpolation = false;
		CameraMode->bHasPendingRotationReference = false;
		if (StackSize > 0)
		{
			FCameraModeView CurrentView;
			BlendStack(CurrentView);
			CameraMode->PendingRotationReference = CurrentView.Rotation;
			CameraMode->PendingControlRotationReference = CurrentView.ControlRotation;
			CameraMode->bHasPendingRotationReference = true;
		}
	}

	// Decide what initial weight to start with.
	const bool bShouldBlend = CameraMode->GetBlendTime() > 0.f && StackSize > 0;
	const float NewBlendWeight = bShouldBlend ? ExistingStackContribution : 1.f;

	CameraMode->SetBlendWeight(NewBlendWeight);
	if (!bShouldBlend)
	{
		// Nothing below to blend from (empty stack, or zero blend time) - snap instead of ramping.
		CameraMode->ResetInterpolation();
	}

	// Add new entry to top of stack.
	CameraModeStack.Insert(CameraMode, 0);
	++StackRevision;

	// Make sure stack bottom is always weighted 100%.
	CameraModeStack.Last()->SetBlendWeight(1.f);

	// Let the camera mode know if it's being added to the stack.
	if (bIsActive && ExistingStackIndex == INDEX_NONE)
	{
		ActivateCameraMode(CameraMode);
	}
}

bool UCameraModeStack::EvaluateStack(const float DeltaTime, FCameraModeView& OutCameraModeView)
{
	if (!bIsActive || CameraModeStack.IsEmpty())
	{
		return false;
	}
	// Activation callbacks can query the stack before lower layers have activated.
	for (const UCameraMode* CameraMode : CameraModeStack)
	{
		if (!CameraMode->bIsActiveOnStack)
		{
			return false;
		}
	}

	if (!UpdateStack(DeltaTime) || !bIsActive || CameraModeStack.IsEmpty())
	{
		return false;
	}
	if (!bHasEvaluatedView)
	{
		// Modes pushed before the first evaluation have no updated lower view to seed their angle branch.
		FRotator RotationReference = CameraModeStack.Last()->View.Rotation;
		FRotator ControlRotationReference = CameraModeStack.Last()->View.ControlRotation;
		for (int32 StackIndex = CameraModeStack.Num() - 2; StackIndex >= 0; --StackIndex)
		{
			UCameraMode* CameraMode = CameraModeStack[StackIndex];
			CameraMode->View.Rotation = RotationReference + (CameraMode->View.Rotation - RotationReference).GetNormalized();
			CameraMode->View.ControlRotation = ControlRotationReference + (CameraMode->View.ControlRotation - ControlRotationReference).GetNormalized();
			const float Weight = CameraMode->GetBlendWeight();
			RotationReference += (CameraMode->View.Rotation - RotationReference) * Weight;
			ControlRotationReference += (CameraMode->View.ControlRotation - ControlRotationReference) * Weight;
		}
	}
	BlendStack(OutCameraModeView);
	bHasEvaluatedView = true;
	return true;
}

UCameraMode* UCameraModeStack::GetCameraModeInstance(const TSubclassOf<UCameraMode> CameraModeClass)
{
	check(CameraModeClass);

	// First see if we already created one.
	for (UCameraMode* CameraMode : CameraModeInstances)
	{
		if (CameraMode && CameraMode->GetClass() == CameraModeClass)
		{
			return CameraMode;
		}
	}

	// Not found, so we need to create it.
	UCameraMode* NewCameraMode = NewObject<UCameraMode>(GetOuter(), CameraModeClass, NAME_None, RF_NoFlags);
	check(NewCameraMode);

	CameraModeInstances.Add(NewCameraMode);
	return NewCameraMode;
}

void UCameraModeStack::ActivateCameraMode(UCameraMode* CameraMode)
{
	check(CameraMode);
	if (!CameraMode->bIsActiveOnStack)
	{
		CameraMode->bIsActiveOnStack = true;
		CameraMode->OnActivation();
	}
}

void UCameraModeStack::DeactivateCameraMode(UCameraMode* CameraMode)
{
	check(CameraMode);
	if (CameraMode->bIsActiveOnStack)
	{
		CameraMode->bIsActiveOnStack = false;
		CameraMode->OnDeactivation();
	}
}

bool UCameraModeStack::UpdateStack(const float DeltaTime)
{
	const int32 StackSize = CameraModeStack.Num();
	if (StackSize <= 0)
	{
		return false;
	}

	const uint32 UpdateRevision = StackRevision;
	int32 RemoveCount = 0;
	int32 RemoveIndex = INDEX_NONE;

	for (int32 StackIndex = 0; StackIndex < StackSize; ++StackIndex)
	{
		UCameraMode* CameraMode = CameraModeStack[StackIndex];
		check(CameraMode);

		CameraMode->UpdateCameraMode(DeltaTime);
		if (StackRevision != UpdateRevision)
		{
			return false;
		}

		if (CameraMode->GetBlendWeight() >= 1.f)
		{
			// Everything below this mode is now irrelevant and can be removed.
			RemoveIndex = (StackIndex + 1);
			RemoveCount = (StackSize - RemoveIndex);
			break;
		}
	}

	if (RemoveCount > 0)
	{
		TArray<TObjectPtr<UCameraMode>> RemovedModes;
		RemovedModes.Append(CameraModeStack.GetData() + RemoveIndex, RemoveCount);
		CameraModeStack.RemoveAt(RemoveIndex, RemoveCount);
		const uint32 RemovalRevision = ++StackRevision;
		for (UCameraMode* CameraMode : RemovedModes)
		{
			if (!bIsActive || !CameraModeStack.Contains(CameraMode))
			{
				DeactivateCameraMode(CameraMode);
			}
		}
		return StackRevision == RemovalRevision;
	}
	return true;
}

void UCameraModeStack::BlendStack(FCameraModeView& OutCameraModeView) const
{
	const int32 StackSize = CameraModeStack.Num();
	if (StackSize <= 0)
	{
		return;
	}

	// Start at the bottom and blend up the stack.
	const UCameraMode* CameraMode = CameraModeStack[StackSize - 1];
	check(CameraMode);

	OutCameraModeView = CameraMode->GetCameraModeView();

	for (int32 StackIndex = StackSize - 2; StackIndex >= 0; --StackIndex)
	{
		CameraMode = CameraModeStack[StackIndex];
		check(CameraMode);

		const FCameraModeView& ModeView = CameraMode->GetCameraModeView();
		const float Weight = CameraMode->GetBlendWeight();
		const FRotator Rotation = OutCameraModeView.Rotation + (ModeView.Rotation - OutCameraModeView.Rotation) * Weight;
		const FRotator ControlRotation = OutCameraModeView.ControlRotation + (ModeView.ControlRotation - OutCameraModeView.ControlRotation) * Weight;
		OutCameraModeView.Blend(ModeView, Weight);
		OutCameraModeView.Rotation = Rotation;
		OutCameraModeView.ControlRotation = ControlRotation;
	}
}

void UCameraModeStack::DrawDebug(UCanvas* Canvas) const
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	DisplayDebugManager.SetDrawColor(FColor::Green);
	DisplayDebugManager.DrawString(FString(TEXT("Camera Modes:")));

	for (const UCameraMode* CameraMode : CameraModeStack)
	{
		check(CameraMode);
		CameraMode->DrawDebug(Canvas);
	}
}

void UCameraModeStack::GetBlendInfo(float& OutWeightOfTopLayer, FGameplayTag& OutTagOfTopLayer) const
{
	if (CameraModeStack.Num() == 0)
	{
		OutWeightOfTopLayer = 1.f;
		OutTagOfTopLayer = FGameplayTag();
		return;
	}

	// Index 0 is the most recently pushed mode - BlendStack applies it last, so it's the dominant ("top") layer.
	const UCameraMode* TopEntry = CameraModeStack[0];
	check(TopEntry);
	OutWeightOfTopLayer = TopEntry->GetBlendWeight();
	OutTagOfTopLayer = TopEntry->GetCameraTypeTag();
}

UCameraMode* UCameraModeStack::GetActiveCameraMode() const
{
	return CameraModeStack.Num() > 0 ? CameraModeStack[0] : nullptr;
}
