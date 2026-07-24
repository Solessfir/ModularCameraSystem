// Copyright Solessfir. All Rights Reserved.

#include "Camera/CameraMode_ThirdPerson.h"
#include "Engine/Canvas.h"
#include "GameFramework/CameraBlockingVolume.h"
#include "Camera/CameraAssistInterface.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Character.h"
#include "Math/RotationMatrix.h"
#include "VisualLogger/VisualLogger.h"
#include "WorldCollision.h"
#include "ModularCameraSystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraMode_ThirdPerson)

namespace CameraMode_ThirdPerson_Statics
{
	static const FName NAME_IgnoreCameraCollision = TEXT("IgnoreCameraCollision");
}

UCameraMode_ThirdPerson::UCameraMode_ThirdPerson()
	: PenetrationAvoidanceFeelers
	{
		FCameraPenetrationAvoidanceFeeler(FRotator(+00.f, +00.f, 0.f), 1.00f, 1.00f, 15.f, 0),
		FCameraPenetrationAvoidanceFeeler(FRotator(+00.f, +16.f, 0.f), 0.75f, 0.75f, 00.f, 3),
		FCameraPenetrationAvoidanceFeeler(FRotator(+00.f, -16.f, 0.f), 0.75f, 0.75f, 00.f, 3),
		FCameraPenetrationAvoidanceFeeler(FRotator(+00.f, +32.f, 0.f), 0.50f, 0.50f, 00.f, 5),
		FCameraPenetrationAvoidanceFeeler(FRotator(+00.f, -32.f, 0.f), 0.50f, 0.50f, 00.f, 5),
		FCameraPenetrationAvoidanceFeeler(FRotator(+20.f, +00.f, 0.f), 1.00f, 1.00f, 00.f, 4),
		FCameraPenetrationAvoidanceFeeler(FRotator(-20.f, +00.f, 0.f), 0.50f, 0.50f, 00.f, 4),
	}
{
	FRichCurve& CurveX = *TargetOffsetX.GetRichCurve();
	const FKeyHandle X0 = CurveX.AddKey(-90.f, -375.f);
	CurveX.SetKeyInterpMode(X0, RCIM_Cubic);
	CurveX.GetKey(X0).TangentMode = RCTM_User;
	CurveX.GetKey(X0).ArriveTangent = CurveX.GetKey(X0).LeaveTangent = 0.75f;

	const FKeyHandle X1 = CurveX.AddKey(-40.f, -325.f);
	CurveX.SetKeyInterpMode(X1, RCIM_Cubic);
	CurveX.GetKey(X1).TangentMode = RCTM_User;
	CurveX.GetKey(X1).ArriveTangent = CurveX.GetKey(X1).LeaveTangent = 1.25f;

	const FKeyHandle X2 = CurveX.AddKey(90.f, -250.f);
	CurveX.SetKeyInterpMode(X2, RCIM_Cubic);

	FRichCurve& CurveZ = *TargetOffsetZ.GetRichCurve();
	const FKeyHandle Z0 = CurveZ.AddKey(-90.f, 60.f);
	CurveZ.SetKeyInterpMode(Z0, RCIM_Cubic);
	CurveZ.GetKey(Z0).TangentMode = RCTM_User;
	CurveZ.GetKey(Z0).ArriveTangent = CurveZ.GetKey(Z0).LeaveTangent = -0.4f;

	const FKeyHandle Z1 = CurveZ.AddKey(0.f, -10.f);
	CurveZ.SetKeyInterpMode(Z1, RCIM_Cubic);
	CurveZ.GetKey(Z1).LeaveTangent = -1.25f;
}

void UCameraMode_ThirdPerson::OnActivation_Implementation()
{
	Super::OnActivation_Implementation();
	ResetPenetrationState();

	// Seed lag from the current pivot, so the first frame after activating never lags in from a
	// stale (or zeroed) transform left over from the last time this mode was active.
	PreviousDesiredRotation = GetPivotRotation();
	PreviousDesiredLocation = GetPivotLocation();
}

void UCameraMode_ThirdPerson::OnDeactivation_Implementation()
{
	ResetPenetrationState();
	Super::OnDeactivation_Implementation();
}

void UCameraMode_ThirdPerson::UpdateView_Implementation(const float DeltaTime)
{
	UpdateForTarget();
	UpdateCrouchOffset(DeltaTime);

	const FVector PivotLocation = GetPivotLocation() + CurrentCrouchOffset;
	FRotator PivotRotation = GetPivotRotation();

	PivotRotation.Pitch = FMath::ClampAngle(PivotRotation.Pitch, ViewPitchMin, ViewPitchMax);

	View.Location = PivotLocation;
	View.Rotation = PivotRotation;
	View.ControlRotation = View.Rotation;
	View.FieldOfView = FieldOfView;

	// Lags the pivot, not the final offset position - free look has no lag, only target movement does.
	FRotator DesiredRotation = PivotRotation;
	FVector DesiredLocation = PivotLocation;
	ApplyCameraLag(DeltaTime, PivotLocation, DesiredRotation, DesiredLocation);
	View.Rotation = DesiredRotation;

	ZoomDistanceScale = FMath::FInterpTo(ZoomDistanceScale, TargetZoomDistanceScale, DeltaTime, ZoomInterpSpeed);

	ApplyTargetOffsetFromRotation(DesiredRotation, DesiredLocation);
	View.Location = DesiredLocation;

	// Adjust final desired camera location to prevent any penetration.
	UpdatePreventPenetration(DeltaTime);
}

void UCameraMode_ThirdPerson::ApplyCameraLag(const float DeltaTime, const FVector& PivotLocation, FRotator& DesiredRotation, FVector& DesiredLocation)
{
	if (bEnableCameraRotationLag)
	{
		if (bUseCameraLagSubstepping && DeltaTime > CameraLagMaxTimeStep && CameraRotationLagSpeed > 0.f)
		{
			const FRotator RotationStep = (DesiredRotation - PreviousDesiredRotation).GetNormalized() * (1.f / DeltaTime);
			FRotator StepTarget = PreviousDesiredRotation;
			float RemainingTime = DeltaTime;
			while (RemainingTime > UE_KINDA_SMALL_NUMBER)
			{
				const float StepTime = FMath::Min(CameraLagMaxTimeStep, RemainingTime);
				StepTarget += RotationStep * StepTime;
				RemainingTime -= StepTime;

				// Interpolate pitch/yaw separately so we never introduce unwanted roll.
				DesiredRotation.Pitch = FMath::QInterpTo(FQuat(FRotator(PreviousDesiredRotation.Pitch, 0.f, 0.f)), FQuat(FRotator(StepTarget.Pitch, 0.f, 0.f)), StepTime, CameraRotationLagSpeed).Rotator().Pitch;
				DesiredRotation.Yaw = FMath::QInterpTo(FQuat(FRotator(0.f, PreviousDesiredRotation.Yaw, 0.f)), FQuat(FRotator(0.f, StepTarget.Yaw, 0.f)), StepTime, CameraRotationLagSpeed).Rotator().Yaw;
				PreviousDesiredRotation = DesiredRotation;
			}
		}
		else
		{
			DesiredRotation.Pitch = FMath::QInterpTo(FQuat(FRotator(PreviousDesiredRotation.Pitch, 0.f, 0.f)), FQuat(FRotator(DesiredRotation.Pitch, 0.f, 0.f)), DeltaTime, CameraRotationLagSpeed).Rotator().Pitch;
			DesiredRotation.Yaw = FMath::QInterpTo(FQuat(FRotator(0.f, PreviousDesiredRotation.Yaw, 0.f)), FQuat(FRotator(0.f, DesiredRotation.Yaw, 0.f)), DeltaTime, CameraRotationLagSpeed).Rotator().Yaw;
		}

		PreviousDesiredRotation = DesiredRotation;
	}

	if (bEnableCameraLag)
	{
		if (bUseCameraLagSubstepping && DeltaTime > CameraLagMaxTimeStep && CameraLagSpeed > 0.f)
		{
			const FVector LocationStep = (DesiredLocation - PreviousDesiredLocation) * (1.f / DeltaTime);
			FVector StepTarget = PreviousDesiredLocation;
			float RemainingTime = DeltaTime;
			while (RemainingTime > UE_KINDA_SMALL_NUMBER)
			{
				const float StepTime = FMath::Min(CameraLagMaxTimeStep, RemainingTime);
				StepTarget += LocationStep * StepTime;
				RemainingTime -= StepTime;

				DesiredLocation = FMath::VInterpTo(PreviousDesiredLocation, StepTarget, StepTime, CameraLagSpeed);
				PreviousDesiredLocation = DesiredLocation;
			}
		}
		else
		{
			DesiredLocation = FMath::VInterpTo(PreviousDesiredLocation, DesiredLocation, DeltaTime, CameraLagSpeed);
		}

		bool bClampedDistance = false;
		if (CameraLagMaxDistance > 0.f)
		{
			const FVector OffsetFromPivot = DesiredLocation - PivotLocation;
			if (OffsetFromPivot.SizeSquared() > FMath::Square(CameraLagMaxDistance))
			{
				DesiredLocation = PivotLocation + OffsetFromPivot.GetClampedToMaxSize(CameraLagMaxDistance);
				bClampedDistance = true;
			}
		}

		#if ENABLE_DRAW_DEBUG
		if (FModularCameraSystemModule::IsShowDebugEnabled())
		{
			DrawDebugSphere(GetWorld(), PivotLocation, 5.f, 8, FColor::Green);
			DrawDebugSphere(GetWorld(), DesiredLocation, 5.f, 8, FColor::Yellow);

			const FVector ToPivot = (PivotLocation - DesiredLocation) * 0.5f;
			const FColor LineColor = bClampedDistance ? FColor::Red : FColor::Green;
			DrawDebugDirectionalArrow(GetWorld(), DesiredLocation, DesiredLocation + ToPivot, 7.5f, LineColor);
			DrawDebugDirectionalArrow(GetWorld(), DesiredLocation + ToPivot, PivotLocation, 7.5f, LineColor);
		}
		#endif

		PreviousDesiredLocation = DesiredLocation;
	}
}

void UCameraMode_ThirdPerson::ApplyTargetOffsetFromRotation(const FRotator& DesiredRotation, FVector& DesiredLocation) const
{
	// Read curves off the CDO (not `this`) so BP-default edits made while PIE is running take effect immediately, without needing a PIE restart.
	const UCameraMode_ThirdPerson* Defaults = GetClass()->GetDefaultObject<UCameraMode_ThirdPerson>();

	FVector TargetOffset;
	TargetOffset.X = Defaults->TargetOffsetX.GetRichCurveConst()->Eval(DesiredRotation.Pitch);
	TargetOffset.Y = Defaults->TargetOffsetY.GetRichCurveConst()->Eval(DesiredRotation.Pitch);
	TargetOffset.Z = Defaults->TargetOffsetZ.GetRichCurveConst()->Eval(DesiredRotation.Pitch);
	DesiredLocation += DesiredRotation.RotateVector(TargetOffset) * ZoomDistanceScale;
}

void UCameraMode_ThirdPerson::AddZoomInput(const float Delta)
{
	TargetZoomDistanceScale = FMath::Clamp(TargetZoomDistanceScale - Delta * ZoomStepSize, MinZoomDistanceScale, MaxZoomDistanceScale);
}

void UCameraMode_ThirdPerson::UpdateForTarget()
{
	if (const ACharacter* TargetCharacter = Cast<ACharacter>(GetTargetActor()))
	{
		if (TargetCharacter->IsCrouched())
		{
			const ACharacter* TargetCharacterCDO = TargetCharacter->GetClass()->GetDefaultObject<ACharacter>();
			const float CrouchedHeightAdjustment = TargetCharacterCDO->CrouchedEyeHeight - TargetCharacterCDO->BaseEyeHeight;
			SetTargetCrouchOffset(FVector(0.f, 0.f, CrouchedHeightAdjustment));
			return;
		}
	}

	SetTargetCrouchOffset(FVector::ZeroVector);
}

void UCameraMode_ThirdPerson::DrawDebug(UCanvas* Canvas) const
{
	Super::DrawDebug(Canvas);

	#if ENABLE_DRAW_DEBUG
	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	for (int32 Index = 0; Index < DebugActorsHitDuringCameraPenetration.Num(); ++Index)
	{
		DisplayDebugManager.DrawString(FString::Printf(TEXT("HitActorDuringPenetration[%d]: %s"), Index, *DebugActorsHitDuringCameraPenetration[Index]->GetName()));
	}

	LastDrawDebugTime = GetWorld()->GetTimeSeconds();
	#endif
}

void UCameraMode_ThirdPerson::UpdatePreventPenetration(const float DeltaTime)
{
	if (!bPreventPenetration)
	{
		ResetPenetrationState();
		return;
	}

	AActor* TargetActor = GetTargetActor();
	if (!TargetActor)
	{
		ResetPenetrationState();
		return;
	}

	const APawn* TargetPawn = Cast<APawn>(TargetActor);
	AController* TargetController = TargetPawn ? TargetPawn->GetController() : nullptr;

	const bool bControllerHasAssist = TargetController && TargetController->Implements<UCameraAssistInterface>();
	const bool bActorHasAssist = TargetActor && TargetActor->Implements<UCameraAssistInterface>();

	AActor* PreventPenetrationActor = TargetActor;
	if (bActorHasAssist)
	{
		if (AActor* CustomPPTarget = ICameraAssistInterface::Execute_GetCameraPreventPenetrationTarget(TargetActor))
		{
			PreventPenetrationActor = CustomPPTarget;
		}
	}

	const bool bPPActorHasAssist = PreventPenetrationActor && PreventPenetrationActor != TargetActor && PreventPenetrationActor->Implements<UCameraAssistInterface>();
	if (!PreventPenetrationActor)
	{
		ResetPenetrationState();
		return;
	}

	TArray<UObject*> AssistRecipients;
	if (bControllerHasAssist)
	{
		AssistRecipients.Add(TargetController);
	}

	if (bActorHasAssist)
	{
		AssistRecipients.Add(TargetActor);
	}

	if (bPPActorHasAssist)
	{
		AssistRecipients.Add(PreventPenetrationActor);
	}

	// Sweep from the pivot (always collision-free by construction) instead of an approximated safe point.
	const FVector SafeLocation = GetPivotLocation() + CurrentCrouchOffset;

	const bool bSingleRayPenetrationCheck = !bDoPredictiveAvoidance;
	PreventCameraPenetration(*PreventPenetrationActor, SafeLocation, View.Location, DeltaTime, AimLineToDesiredPosBlockedPct, bSingleRayPenetrationCheck);

	const bool bIsPenetrating = AimLineToDesiredPosBlockedPct < ReportPenetrationPercent;
	SetPenetrationNotificationState(bIsPenetrating, AssistRecipients);
}

void UCameraMode_ThirdPerson::SetPenetrationNotificationState(const bool bIsPenetrating, const TArray<UObject*>& AssistRecipients)
{
	if (bIsPenetrating == bWasPenetratingTarget)
	{
		return;
	}

	bWasPenetratingTarget = bIsPenetrating;
	if (bIsPenetrating)
	{
		PenetrationNotificationRecipients.Reset();
		for (UObject* Recipient : AssistRecipients)
		{
			if (Recipient && Recipient->Implements<UCameraAssistInterface>())
			{
				PenetrationNotificationRecipients.AddUnique(TWeakObjectPtr<UObject>(Recipient));
				ICameraAssistInterface::Execute_OnCameraPenetratingTarget(Recipient);
			}
		}
		return;
	}

	TArray<TWeakObjectPtr<UObject>> RecipientsToNotify = MoveTemp(PenetrationNotificationRecipients);
	for (const TWeakObjectPtr<UObject>& WeakRecipient : RecipientsToNotify)
	{
		if (UObject* Recipient = WeakRecipient.Get(); Recipient && Recipient->Implements<UCameraAssistInterface>())
		{
			ICameraAssistInterface::Execute_OnCameraStoppedPenetratingTarget(Recipient);
		}
	}
}

void UCameraMode_ThirdPerson::ResetPenetrationState()
{
	const TArray<UObject*> NoRecipients;
	SetPenetrationNotificationState(false, NoRecipients);

	AimLineToDesiredPosBlockedPct = 1.f;
	for (FCameraPenetrationAvoidanceFeeler& Feeler : PenetrationAvoidanceFeelers)
	{
		Feeler.AsyncTraceHandle.Invalidate();
		Feeler.AsyncTraceStart = FVector::ZeroVector;
		Feeler.AsyncTraceEnd = FVector::ZeroVector;
		Feeler.FramesUntilNextTrace = 0;

		#if ENABLE_DRAW_DEBUG
		Feeler.bLastDrawResultValid = false;
		#endif
	}

	#if ENABLE_DRAW_DEBUG
	DebugActorsHitDuringCameraPenetration.Reset();
	#endif
}

void UCameraMode_ThirdPerson::PreventCameraPenetration(const AActor& ViewTarget, const FVector& SafeLoc, FVector& CameraLoc, const float& DeltaTime, float& DistBlockedPct, bool bSingleRayOnly)
{
	#if ENABLE_DRAW_DEBUG
	DebugActorsHitDuringCameraPenetration.Reset();
	#endif

	float HardBlockedPct = DistBlockedPct;
	float SoftBlockedPct = DistBlockedPct;

	FVector BaseRay = CameraLoc - SafeLoc;
	FRotationMatrix BaseRayMatrix(BaseRay.Rotation());
	FVector BaseRayLocalUp, BaseRayLocalFwd, BaseRayLocalRight;

	BaseRayMatrix.GetScaledAxes(BaseRayLocalFwd, BaseRayLocalRight, BaseRayLocalUp);

	float DistBlockedPctThisFrame = 1.f;

	const int32 NumRaysToShoot = bSingleRayOnly ? FMath::Min(1, PenetrationAvoidanceFeelers.Num()) : PenetrationAvoidanceFeelers.Num();
	FCollisionQueryParams SphereParams(SCENE_QUERY_STAT(CameraPen), false, nullptr);

	SphereParams.AddIgnoredActor(&ViewTarget);

	// Optional assists can expand the ignore list (pawn, vehicle, extra view targets, etc.).
	auto AppendIgnoredFromAssist = [&SphereParams](const UObject* AssistObject)
	{
		if (AssistObject && AssistObject->Implements<UCameraAssistInterface>())
		{
			TArray<AActor*> IgnoredActors;
			ICameraAssistInterface::Execute_GetIgnoredActorsForCameraPenetration(AssistObject, IgnoredActors);
			for (const AActor* IgnoredActor : IgnoredActors)
			{
				if (IgnoredActor)
				{
					SphereParams.AddIgnoredActor(IgnoredActor);
				}
			}
		}
	};

	if (const APawn* TargetPawn = Cast<APawn>(GetTargetActor()))
	{
		AppendIgnoredFromAssist(TargetPawn->GetController());
	}
	AppendIgnoredFromAssist(GetTargetActor());

	FCollisionShape SphereShape = FCollisionShape::MakeSphere(0.f);
	UWorld* World = GetWorld();
	check(World);

	auto ShouldDiscardHit = [&ViewTarget](const FHitResult& CandidateHit, const FCameraPenetrationAvoidanceFeeler& Feeler)
	{
		const AActor* HitActor = CandidateHit.GetActor();
		if (HitActor && HitActor->ActorHasTag(CameraMode_ThirdPerson_Statics::NAME_IgnoreCameraCollision))
		{
			return true;
		}

		// CameraBlockingVolumes in front of the target are meant to keep the camera behind it,
		// not push a third-person camera inward while it is already behind the target.
		if (HitActor && HitActor->IsA<ACameraBlockingVolume>())
		{
			const FVector ViewTargetForwardXY = ViewTarget.GetActorForwardVector().GetSafeNormal2D();
			const FVector HitDirectionXY = (CandidateHit.Location - ViewTarget.GetActorLocation()).GetSafeNormal2D();
			if (FVector::DotProduct(ViewTargetForwardXY, HitDirectionXY) > 0.f)
			{
				return true;
			}
		}

		const float Weight = Cast<APawn>(HitActor) ? Feeler.PawnWeight : Feeler.WorldWeight;
		return Weight <= 0.f;
	};

	auto AddDiscardedHitToQuery = [](FCollisionQueryParams& QueryParams, const FHitResult& DiscardedHit)
	{
		if (const AActor* HitActor = DiscardedHit.GetActor())
		{
			QueryParams.AddIgnoredActor(HitActor);
			return true;
		}
		if (const UPrimitiveComponent* HitComponent = DiscardedHit.GetComponent())
		{
			QueryParams.AddIgnoredComponent(HitComponent);
			return true;
		}
		return false;
	};

	auto SweepForRelevantHit = [World, this, &ShouldDiscardHit, &AddDiscardedHitToQuery](
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const FCollisionShape& TraceShape,
		FCollisionQueryParams QueryParams,
		const FCameraPenetrationAvoidanceFeeler& Feeler,
		FHitResult& OutHit)
	{
		constexpr int32 MaxDiscardedHitsPerRay = 16;
		for (int32 Attempt = 0; Attempt < MaxDiscardedHitsPerRay; ++Attempt)
		{
			FHitResult CandidateHit;
			if (!World->SweepSingleByChannel(CandidateHit, TraceStart, TraceEnd, FQuat::Identity, TraceChannel, TraceShape, QueryParams))
			{
				return false;
			}

			if (!ShouldDiscardHit(CandidateHit, Feeler))
			{
				OutHit = CandidateHit;
				return true;
			}

			if (!AddDiscardedHitToQuery(QueryParams, CandidateHit))
			{
				return false;
			}
		}

		return false;
	};

	for (int32 RayIdx = 0; RayIdx < NumRaysToShoot; ++RayIdx)
	{
		FCameraPenetrationAvoidanceFeeler& Feeler = PenetrationAvoidanceFeelers[RayIdx];

		// Calc ray target - always, even if this feeler isn't tracing this tick, so debug draw below has it.
		FVector RayTarget;
		{
			FVector RotatedRay = BaseRay.RotateAngleAxis(Feeler.AdjustmentRot.Yaw, BaseRayLocalUp);
			RotatedRay = RotatedRay.RotateAngleAxis(Feeler.AdjustmentRot.Pitch, BaseRayLocalRight);
			RayTarget = SafeLoc + RotatedRay;
		}

		SphereShape.Sphere.Radius = Feeler.Extent;

		FHitResult Hit;
		bool bHit = false;
		bool bGotResult = false;
		FVector ResultStart = SafeLoc;
		FVector ResultEnd = RayTarget;

		// Feeler 0 only goes async if opted in (staleness is visible); predictive feelers always do.
		const bool bUseAsyncForThisFeeler = (RayIdx == 0) ? bRunAsyncCollision : true;
		if (bUseAsyncForThisFeeler)
		{
			// Poll pending work every frame. QueryTraceData is only valid on the frame after submission,
			// independently of the feeler's interval for scheduling the next trace.
			if (Feeler.AsyncTraceHandle.IsValid())
			{
				FTraceDatum TraceDatum;
				if (World->QueryTraceData(Feeler.AsyncTraceHandle, TraceDatum))
				{
					bGotResult = true;
					ResultStart = Feeler.AsyncTraceStart;
					ResultEnd = Feeler.AsyncTraceEnd;
					Feeler.AsyncTraceHandle.Invalidate();

					const FHitResult* BlockingHit = TraceDatum.OutHits.FindByPredicate(
						[](const FHitResult& CandidateHit) { return CandidateHit.bBlockingHit; });
					if (BlockingHit)
					{
						if (ShouldDiscardHit(*BlockingHit, Feeler))
						{
							FCollisionQueryParams FollowUpParams = SphereParams;
							if (AddDiscardedHitToQuery(FollowUpParams, *BlockingHit))
							{
								// Ignored hits are uncommon; a synchronous follow-up prevents them from
								// masking a real blocker without adding another frame of visible clipping.
								bHit = SweepForRelevantHit(ResultStart, ResultEnd, SphereShape, FollowUpParams, Feeler, Hit);
							}
						}
						else
						{
							Hit = *BlockingHit;
							bHit = true;
						}
					}
				}
				else if (!World->IsTraceHandleValid(Feeler.AsyncTraceHandle, false))
				{
					// The result expired or the request was invalid. Clear it so a replacement can be scheduled.
					Feeler.AsyncTraceHandle.Invalidate();
				}
			}
		}
		else if (Feeler.FramesUntilNextTrace <= 0)
		{
			bGotResult = true;
			bHit = SweepForRelevantHit(SafeLoc, RayTarget, SphereShape, SphereParams, Feeler, Hit);
		}

		#if ENABLE_VISUAL_LOG
		if (bGotResult)
		{
			UE_VLOG_SPHERE(GetTargetActor(), LogModularCameraSystem, Verbose, ResultStart, SphereShape.Sphere.Radius, bHit ? FColor::Red : FColor::Green, TEXT("Feeler[%d]"), RayIdx);
			UE_VLOG_SEGMENT(GetTargetActor(), LogModularCameraSystem, Verbose, ResultStart, bHit ? Hit.Location : ResultEnd, bHit ? FColor::Red : FColor::Green, TEXT(""));
			UE_VLOG(GetTargetActor(), LogModularCameraSystem, Verbose, TEXT("Feeler[%d] bHit=%d bBlockingHit=%d bStartPenetrating=%d Time=%.3f Actor=%s TraceLength=%.1f"),
				RayIdx, bHit, Hit.bBlockingHit, Hit.bStartPenetrating, Hit.Time, *GetNameSafe(Hit.GetActor()), FVector::Dist(ResultStart, ResultEnd));
		}
		#endif

		if (bGotResult)
		{
			Feeler.FramesUntilNextTrace = Feeler.TraceInterval;

			if (bHit)
			{
				const AActor* HitActor = Hit.GetActor();
				const float Weight = FMath::Clamp(Cast<APawn>(HitActor) ? Feeler.PawnWeight : Feeler.WorldWeight, 0.f, 1.f);
				const float TraceLength = FVector::Distance(ResultStart, ResultEnd);
				if (TraceLength > UE_KINDA_SMALL_NUMBER)
				{
					// Push away from the surface first, then attenuate that correction by the feeler's weight.
					float NewBlockPct = (FVector::Distance(ResultStart, Hit.Location) - CollisionPushOutDistance) / TraceLength;
					NewBlockPct += (1.f - NewBlockPct) * (1.f - Weight);
					DistBlockedPctThisFrame = FMath::Min(NewBlockPct, DistBlockedPctThisFrame);
				}

				// A relevant hit is checked again next frame instead of waiting out TraceInterval.
				Feeler.FramesUntilNextTrace = 0;

				#if ENABLE_DRAW_DEBUG
				if (HitActor)
				{
					DebugActorsHitDuringCameraPenetration.AddUnique(TObjectPtr<const AActor>(HitActor));
				}
				#endif
			}

			if (RayIdx == 0)
			{
				// Don't interpolate toward this one, snap to it (assumes ray 0 is the center/main ray).
				HardBlockedPct = DistBlockedPctThisFrame;
			}
			else
			{
				SoftBlockedPct = DistBlockedPctThisFrame;
			}

			#if ENABLE_DRAW_DEBUG
			Feeler.bLastDrawResultValid = true;
			Feeler.bLastBlocked = bHit;
			Feeler.LastStartPoint = ResultStart;
			Feeler.LastEndPoint = bHit ? Hit.Location : ResultEnd;
			#endif
		}

		if (bUseAsyncForThisFeeler && !Feeler.AsyncTraceHandle.IsValid())
		{
			if (Feeler.FramesUntilNextTrace <= 0)
			{
				Feeler.AsyncTraceStart = SafeLoc;
				Feeler.AsyncTraceEnd = RayTarget;
				Feeler.AsyncTraceHandle = World->AsyncSweepByChannel(EAsyncTraceType::Single, SafeLoc, RayTarget, FQuat::Identity, TraceChannel, SphereShape, SphereParams);
			}
			else if (!bGotResult)
			{
				--Feeler.FramesUntilNextTrace;
			}
		}
		else if (!bUseAsyncForThisFeeler && !bGotResult && Feeler.FramesUntilNextTrace > 0)
		{
			--Feeler.FramesUntilNextTrace;
		}

		// Draw every feeler every frame from its last-known result, not just the ones that happened
		// to trace this tick - TraceInterval staggers them, so gating draw on that made most feelers
		// invisible most frames.
		#if ENABLE_DRAW_DEBUG
		if (Feeler.bLastDrawResultValid && World->TimeSince(LastDrawDebugTime) < 1.f)
		{
			if (Feeler.bLastBlocked)
			{
				DrawDebugSphere(World, Feeler.LastEndPoint, Feeler.Extent, 8, FColor::Red);
				DrawDebugLine(World, Feeler.LastStartPoint, Feeler.LastEndPoint, FColor::Red);
				DrawDebugString(World, Feeler.LastStartPoint + FVector(0.f, 0.f, 10.f + RayIdx * 12.f), FString::Printf(TEXT("Feeler[%d] BLOCKED"), RayIdx), nullptr, FColor::Red, 0.f, false, 1.f);
			}
			else
			{
				DrawDebugLine(World, Feeler.LastStartPoint, Feeler.LastEndPoint, FColor::Green);
			}
		}
		#endif
	}

	if (bResetInterpolation)
	{
		DistBlockedPct = DistBlockedPctThisFrame;
	}
	else if (DistBlockedPct < DistBlockedPctThisFrame)
	{
		// Interpolate smoothly out.
		if (PenetrationBlendOutTime > DeltaTime)
		{
			DistBlockedPct = DistBlockedPct + DeltaTime / PenetrationBlendOutTime * (DistBlockedPctThisFrame - DistBlockedPct);
		}
		else
		{
			DistBlockedPct = DistBlockedPctThisFrame;
		}
	}
	else
	{
		if (DistBlockedPct > HardBlockedPct)
		{
			DistBlockedPct = HardBlockedPct;
		}
		else if (DistBlockedPct > SoftBlockedPct)
		{
			// Interpolate smoothly in.
			if (PenetrationBlendInTime > DeltaTime)
			{
				DistBlockedPct = DistBlockedPct - DeltaTime / PenetrationBlendInTime * (DistBlockedPct - SoftBlockedPct);
			}
			else
			{
				DistBlockedPct = SoftBlockedPct;
			}
		}
	}

	DistBlockedPct = FMath::Clamp<float>(DistBlockedPct, 0.f, 1.f);
	if (DistBlockedPct < (1.f - ZERO_ANIMWEIGHT_THRESH))
	{
		CameraLoc = SafeLoc + (CameraLoc - SafeLoc) * DistBlockedPct;
	}

	#if ENABLE_VISUAL_LOG
	UE_VLOG_LOCATION(GetTargetActor(), LogModularCameraSystem, Log, CameraLoc, 8.f, FColor::Cyan, TEXT("Camera (Blocked %.0f%%)"), DistBlockedPct * 100.f);
	#endif
}

void UCameraMode_ThirdPerson::SetTargetCrouchOffset(const FVector& NewTargetOffset)
{
	CrouchOffsetBlendPct = 0.f;
	InitialCrouchOffset = CurrentCrouchOffset;
	TargetCrouchOffset = NewTargetOffset;
}

void UCameraMode_ThirdPerson::UpdateCrouchOffset(const float DeltaTime)
{
	if (CrouchOffsetBlendPct < 1.f)
	{
		CrouchOffsetBlendPct = FMath::Min(CrouchOffsetBlendPct + DeltaTime * CrouchOffsetBlendMultiplier, 1.f);
		CurrentCrouchOffset = FMath::InterpEaseInOut(InitialCrouchOffset, TargetCrouchOffset, CrouchOffsetBlendPct, 1.f);
		return;
	}
	CurrentCrouchOffset = TargetCrouchOffset;
	CrouchOffsetBlendPct = 1.f;
}
