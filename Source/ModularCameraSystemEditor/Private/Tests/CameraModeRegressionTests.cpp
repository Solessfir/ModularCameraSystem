// Copyright Solessfir. All Rights Reserved.

#include "CameraModeRegressionTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraModeComponent.h"
#include "Camera/CameraMode_FirstPerson.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	struct FCameraTestWorld
	{
		FCameraTestWorld(bool bCreatePhysicsScene = false, bool bCreateWorldContext = false)
			: bHasWorldContext(bCreateWorldContext)
		{
			const UWorld::InitializationValues InitializationValues = UWorld::InitializationValues()
				.AllowAudioPlayback(false)
				.RequiresHitProxies(false)
				.CreatePhysicsScene(bCreatePhysicsScene)
				.EnableTraceCollision(bCreatePhysicsScene)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &InitializationValues);
			if (bHasWorldContext)
			{
				GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			}

			Pawn = World->SpawnActor<APawn>();
			USceneComponent* Root = NewObject<USceneComponent>(Pawn);
			Pawn->SetRootComponent(Root);
			Root->RegisterComponent();
			Pawn->SetActorLocationAndRotation(FVector(500.f, 100.f, 200.f), FRotator(0.f, 45.f, 0.f));

			Camera = NewObject<UCameraModeComponent>(Pawn);
			Camera->SetupAttachment(Root);
			Camera->RegisterComponent();
			Camera->Activate();
		}

		~FCameraTestWorld()
		{
			World->DestroyWorld(false);
			if (bHasWorldContext)
			{
				GEngine->DestroyWorldContext(World);
			}
		}

		UWorld* World;
		APawn* Pawn;
		UCameraModeComponent* Camera;
		bool bHasWorldContext;
	};

	UCameraModeComponent* AddThirdPersonCamera(AActor* Owner)
	{
		if (!Owner->GetRootComponent())
		{
			USceneComponent* Root = NewObject<USceneComponent>(Owner);
			Owner->SetRootComponent(Root);
			Root->RegisterComponent();
		}
		UCameraModeComponent* Camera = NewObject<UCameraModeComponent>(Owner);
		Camera->SetupAttachment(Owner->GetRootComponent());
		Camera->DefaultCameraMode = UCameraModeRegressionTestLag::StaticClass();
		Camera->RegisterComponent();
		Camera->Activate();
		return Camera;
	}

	AActor* AddBlockingBox(UWorld* World, const FVector& Location, const FVector& Extent)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		Box->RegisterComponent();
		Actor->SetActorLocation(Location);
		return Actor;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModePromotionContinuityTest, "ModularCameraSystem.Stack.PromotionPreservesThreeLayerView", TestFlags)

bool FCameraModePromotionContinuityTest::RunTest(const FString& Parameters)
{
	const TSubclassOf<UCameraMode> Promotions[] = { UCameraModeRegressionTestA::StaticClass(), UCameraModeRegressionTestB::StaticClass() };
	for (const TSubclassOf<UCameraMode>& Promotion : Promotions)
	{
		UCameraModeComponent* Component = NewObject<UCameraModeComponent>();
		UCameraModeStack* Stack = NewObject<UCameraModeStack>(Component);
		FCameraModeView Before;
		Stack->PushCameraMode(UCameraModeRegressionTestA::StaticClass());
		UCameraModeRegressionTestBase* ModeA = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
		ModeA->FixedRotation = FRotator::ZeroRotator;
		ModeA->FixedControlRotation = FRotator::ZeroRotator;
		Stack->EvaluateStack(0.f, Before);
		Stack->PushCameraMode(UCameraModeRegressionTestB::StaticClass());
		UCameraModeRegressionTestBase* ModeB = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
		ModeB->FixedRotation = FRotator(0.f, 170.f, 0.f);
		ModeB->FixedControlRotation = FRotator(0.f, -170.f, 0.f);
		Stack->EvaluateStack(0.25f, Before);
		Stack->PushCameraMode(UCameraModeRegressionTestC::StaticClass());
		UCameraModeRegressionTestBase* ModeC = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
		ModeC->FixedRotation = FRotator(0.f, -170.f, 0.f);
		ModeC->FixedControlRotation = FRotator(0.f, 170.f, 0.f);
		TestTrue(TEXT("Three-layer stack evaluates"), Stack->EvaluateStack(0.25f, Before));

		Stack->PushCameraMode(Promotion);
		FCameraModeView After;
		TestTrue(TEXT("Promoted stack evaluates"), Stack->EvaluateStack(0.f, After));
		TestTrue(*FString::Printf(TEXT("%s preserves position at zero delta"), *Promotion->GetName()), After.Location.Equals(Before.Location, 0.001));
		TestTrue(*FString::Printf(TEXT("%s preserves FOV at zero delta"), *Promotion->GetName()), FMath::IsNearlyEqual(After.FieldOfView, Before.FieldOfView, 0.001f));
		TestTrue(*FString::Printf(TEXT("%s preserves view rotation across the yaw boundary"), *Promotion->GetName()), After.Rotation.GetNormalized().Equals(Before.Rotation.GetNormalized(), 0.001));
		TestTrue(*FString::Printf(TEXT("%s preserves control rotation across the yaw boundary"), *Promotion->GetName()), After.ControlRotation.GetNormalized().Equals(Before.ControlRotation.GetNormalized(), 0.001));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeActivationSeedTest, "ModularCameraSystem.Stack.ActivationResetPreservesRotationSeed", TestFlags)

bool FCameraModeActivationSeedTest::RunTest(const FString& Parameters)
{
	UCameraModeComponent* Component = NewObject<UCameraModeComponent>();
	UCameraModeStack* Stack = NewObject<UCameraModeStack>(Component);
	Stack->PushCameraMode(UCameraModeRegressionTestA::StaticClass());
	UCameraModeRegressionTestBase* ModeA = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
	ModeA->FixedRotation = FRotator(0.f, 170.f, 0.f);
	ModeA->FixedControlRotation = FRotator(0.f, -170.f, 0.f);
	FCameraModeView View;
	Stack->EvaluateStack(0.f, View);
	Stack->PushCameraMode(UCameraModeRegressionTestB::StaticClass());
	UCameraModeRegressionTestBase* ModeB = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
	ModeB->FixedRotation = FRotator(0.f, -170.f, 0.f);
	ModeB->FixedControlRotation = FRotator(0.f, 170.f, 0.f);
	TestTrue(TEXT("Half-blended activated mode evaluates"), Stack->EvaluateStack(0.5f, View));
	TestTrue(TEXT("Activation resetting view rotation retains the 180-degree blend branch"), FMath::IsNearlyEqual(FMath::Abs(View.Rotation.GetNormalized().Yaw), 180.0, 0.001));
	TestTrue(TEXT("Activation resetting control rotation retains the 180-degree blend branch"), FMath::IsNearlyEqual(FMath::Abs(View.ControlRotation.GetNormalized().Yaw), 180.0, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeFirstEvaluationYawTest, "ModularCameraSystem.Stack.FirstEvaluationUsesShortestYawBlend", TestFlags)

bool FCameraModeFirstEvaluationYawTest::RunTest(const FString& Parameters)
{
	UCameraModeComponent* Component = NewObject<UCameraModeComponent>();
	UCameraModeStack* Stack = NewObject<UCameraModeStack>(Component);
	Stack->PushCameraMode(UCameraModeRegressionTestA::StaticClass());
	UCameraModeRegressionTestBase* ModeA = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
	ModeA->FixedRotation = FRotator(0.f, 170.f, 0.f);
	ModeA->FixedControlRotation = FRotator(0.f, 170.f, 0.f);
	Stack->PushCameraMode(UCameraModeRegressionTestB::StaticClass());
	UCameraModeRegressionTestBase* ModeB = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
	ModeB->FixedRotation = FRotator(0.f, -170.f, 0.f);
	ModeB->FixedControlRotation = FRotator(0.f, -170.f, 0.f);
	FCameraModeView View;
	TestTrue(TEXT("Stack evaluates after both modes are pushed before their first update"), Stack->EvaluateStack(0.5f, View));
	TestTrue(TEXT("First view yaw follows the 180-degree midpoint"), FMath::IsNearlyEqual(FMath::Abs(View.Rotation.GetNormalized().Yaw), 180.0, 0.001));
	TestTrue(TEXT("First control yaw follows the 180-degree midpoint"), FMath::IsNearlyEqual(FMath::Abs(View.ControlRotation.GetNormalized().Yaw), 180.0, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeRegistrationCallbackTest, "ModularCameraSystem.Component.FirstRegistrationActivationCanQueryCamera", TestFlags)

bool FCameraModeRegistrationCallbackTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld Fixture;
	Fixture.World->InitializeActorsForPlay(FURL());
	AActor* Owner = Fixture.World->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(Owner);
	Owner->SetRootComponent(Root);
	Root->RegisterComponent();
	Owner->SetActorLocation(FVector(10.f, 20.f, 30.f));
	UCameraModeComponent* Camera = NewObject<UCameraModeComponent>(Owner);
	Owner->AddInstanceComponent(Camera);
	Camera->SetupAttachment(Root);
	Camera->FieldOfView = 75.f;
	UCameraActivationRegressionTestReceiver* Receiver = NewObject<UCameraActivationRegressionTestReceiver>(Owner);
	Camera->OnComponentActivated.AddDynamic(Receiver, &UCameraActivationRegressionTestReceiver::OnCameraActivated);
	Camera->OnComponentDeactivated.AddDynamic(Receiver, &UCameraActivationRegressionTestReceiver::OnCameraDeactivated);
	Camera->RegisterComponent();
	TestEqual(TEXT("Initial registration invokes the activation callback"), Receiver->ActivationCount, 1);
	TestEqual(TEXT("Callback can query the empty stack's blend weight"), Receiver->BlendWeight, 1.f);
	TestFalse(TEXT("Empty stack has no blend tag"), Receiver->BlendTag.IsValid());
	TestTrue(TEXT("Callback can query the camera view during initial registration"), Receiver->ViewLocation.Equals(Owner->GetActorLocation(), 0.001));
	TestEqual(TEXT("Callback view preserves the authored field of view"), Receiver->ViewFieldOfView, 75.f);
	Camera->DefaultCameraMode = UCameraModeRegressionTestB::StaticClass();
	FMinimalViewInfo ModeView;
	Camera->GetCameraView(0.f, ModeView);
	Camera->Deactivate();
	TestEqual(TEXT("Deactivation invokes the camera-query callback"), Receiver->DeactivationCount, 1);
	TestTrue(TEXT("Deactivation callback uses the authored fallback view"), Receiver->ViewLocation.Equals(Owner->GetActorLocation(), 0.001));
	TestEqual(TEXT("Deactivation callback restores the authored FOV"), Receiver->ViewFieldOfView, 75.f);
	Camera->Activate();
	TestEqual(TEXT("Reactivation invokes the camera-query callback again"), Receiver->ActivationCount, 2);
	TestTrue(TEXT("Reactivation callback evaluates the selected camera mode"), Receiver->ViewLocation.Equals(ModeView.Location, 0.001));
	TestEqual(TEXT("Reactivation callback uses the selected mode's FOV"), Receiver->ViewFieldOfView, ModeView.FOV);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeFallbackTest, "ModularCameraSystem.Component.NullSelectionRestoresAuthoredView", TestFlags)

bool FCameraModeFallbackTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld Fixture;
	const FVector AuthoredLocation(25.f, -10.f, 40.f);
	const FRotator AuthoredRotation(-15.f, 5.f, 0.f);
	Fixture.Camera->SetRelativeLocationAndRotation(AuthoredLocation, AuthoredRotation);
	Fixture.Camera->FieldOfView = 75.f;
	FMinimalViewInfo AuthoredView;
	Fixture.Camera->GetCameraView(0.f, AuthoredView);

	Fixture.Camera->PushCameraMode(UCameraModeRegressionTestB::StaticClass());
	FMinimalViewInfo ModeView;
	Fixture.Camera->GetCameraView(0.f, ModeView);
	UCameraModeRegressionTestBase* Mode = CastChecked<UCameraModeRegressionTestBase>(Fixture.Camera->GetActiveCameraMode());
	TestFalse(TEXT("Override changes camera location"), ModeView.Location.Equals(AuthoredView.Location));

	Fixture.Camera->PopCameraMode(UCameraModeRegressionTestB::StaticClass());
	Fixture.Camera->AddFieldOfViewOffset(5.f);
	FMinimalViewInfo FallbackView;
	Fixture.Camera->GetCameraView(0.f, FallbackView);
	TestNull(TEXT("Null selection clears the active mode"), Fixture.Camera->GetActiveCameraMode());
	TestEqual(TEXT("Removed mode deactivates once"), Mode->DeactivationCount, 1);
	TestTrue(TEXT("Fallback restores authored relative location"), Fixture.Camera->GetRelativeLocation().Equals(AuthoredLocation));
	TestTrue(TEXT("Fallback restores authored relative rotation"), Fixture.Camera->GetRelativeRotation().Equals(AuthoredRotation));
	TestTrue(TEXT("Fallback restores authored world location"), FallbackView.Location.Equals(AuthoredView.Location));
	TestTrue(TEXT("Fallback restores authored world rotation"), FallbackView.Rotation.Equals(AuthoredView.Rotation));
	TestEqual(TEXT("Fallback applies the one-frame FOV offset"), FallbackView.FOV, 80.f);
	Fixture.Camera->GetCameraView(0.f, FallbackView);
	TestEqual(TEXT("Fallback consumes the FOV offset"), FallbackView.FOV, AuthoredView.FOV);
	TestEqual(TEXT("Repeated null selection does not deactivate again"), Mode->DeactivationCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeControlRotationTest, "ModularCameraSystem.Component.ModeViewAndControlRotationRemainSeparate", TestFlags)

bool FCameraModeControlRotationTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld Fixture;
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>();
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->Possess(Fixture.Pawn);
	if (!TestTrue(TEXT("Fixture has a local player controller"), Controller->IsLocalPlayerController()))
	{
		return false;
	}
	Fixture.Camera->bUsePawnControlRotation = true;
	Fixture.Camera->PushCameraMode(UCameraModeRegressionTestA::StaticClass());
	FMinimalViewInfo View;
	Fixture.Camera->GetCameraView(0.f, View);
	UCameraModeRegressionTestBase* Mode = CastChecked<UCameraModeRegressionTestBase>(Fixture.Camera->GetActiveCameraMode());
	TestTrue(TEXT("View uses the mode's camera rotation"), View.Rotation.Equals(Mode->FixedRotation));
	TestTrue(TEXT("Controller uses the mode's control rotation"), Controller->GetControlRotation().Equals(Mode->FixedControlRotation));
	TestTrue(TEXT("Use Pawn Control Rotation remains enabled"), Fixture.Camera->bUsePawnControlRotation);
	Mode->FixedControlRotation = FRotator(390.f, 250.f, -370.f);
	Fixture.Camera->GetCameraView(0.f, View);
	const FRotator ExpectedControl = Mode->FixedControlRotation.GetNormalized();
	const FRotator ActualControl = Controller->GetControlRotation();
	TestTrue(TEXT("Controller stores normalized accumulated mode angles"), FVector(ActualControl.Pitch, ActualControl.Yaw, ActualControl.Roll).Equals(FVector(ExpectedControl.Pitch, ExpectedControl.Yaw, ExpectedControl.Roll), 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeLagHistoryTest, "ModularCameraSystem.ThirdPerson.EnablingLagUsesLatestPivot", TestFlags)

bool FCameraModeLagHistoryTest::RunTest(const FString& Parameters)
{
	UCameraModeRegressionTestLag* Mode = NewObject<UCameraModeRegressionTestLag>();
	const FVector Pivot(1000.f, 500.f, 200.f);
	const FRotator PivotRotation(25.f, 90.f, 0.f);
	FVector Location = Pivot;
	FRotator Rotation = PivotRotation;
	Mode->ApplyLag(1.f / 60.f, Pivot, Rotation, Location);
	Mode->EnableLag();
	Mode->ApplyLag(1.f / 60.f, Pivot, Rotation, Location);
	TestTrue(TEXT("Enabling location lag preserves a stationary current pivot"), Location.Equals(Pivot));
	TestTrue(TEXT("Enabling rotation lag preserves a stationary current pivot rotation"), Rotation.Equals(PivotRotation));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeZoomRangeTest, "ModularCameraSystem.ThirdPerson.ZoomBoundsApplyWithoutInput", TestFlags)

bool FCameraModeZoomRangeTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld Fixture;
	Fixture.Pawn->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	UCameraModeRegressionTestLag* Mode = NewObject<UCameraModeRegressionTestLag>(Fixture.Camera);
	Mode->bPreventPenetration = false;
	const FVector Pivot = Mode->GetCurrentPivot();
	Mode->SetZoomRange(0.3f, 0.5f);
	Mode->UpdateCameraMode(1.f / 60.f);
	TestEqual(TEXT("First update clamps the initial zoom to the configured maximum without input"), Mode->GetZoomDistanceScale(), 0.5f);
	TestTrue(TEXT("First update uses the bounded scale for the camera offset"), Mode->GetCameraModeView().Location.Equals(Pivot + FVector(-150.f, 0.f, 0.f), 0.001));
	Mode->SetZoomRange(0.3f, 0.4f);
	Mode->UpdateCameraMode(1.f / 60.f);
	TestEqual(TEXT("Tightening the maximum clamps zoom without input"), Mode->GetZoomDistanceScale(), 0.4f);
	TestTrue(TEXT("Tightened maximum changes the actual camera offset"), Mode->GetCameraModeView().Location.Equals(Pivot + FVector(-120.f, 0.f, 0.f), 0.001));
	Mode->SetZoomRange(0.45f, 0.6f);
	Mode->UpdateCameraMode(1.f / 60.f);
	TestEqual(TEXT("Raising the minimum clamps zoom without input"), Mode->GetZoomDistanceScale(), 0.45f);
	TestTrue(TEXT("Raised minimum changes the actual camera offset"), Mode->GetCameraModeView().Location.Equals(Pivot + FVector(-135.f, 0.f, 0.f), 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModePenetrationRecipientsTest, "ModularCameraSystem.ThirdPerson.PenetrationRecipientChangeBalancesNotifications", TestFlags)

bool FCameraModePenetrationRecipientsTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld Fixture;
	Fixture.World->InitializeActorsForPlay(FURL());
	ACameraAssistRegressionTestActor* Owner = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	ACameraAssistRegressionTestActor* FirstTarget = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	ACameraAssistRegressionTestActor* SecondTarget = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	UCameraModeRegressionTestLag* Mode = NewObject<UCameraModeRegressionTestLag>(Fixture.Camera);
	Mode->NotifyPenetration(true, { Owner, FirstTarget });
	Mode->NotifyPenetration(true, { Owner, SecondTarget });
	TestEqual(TEXT("First target receives one enter"), FirstTarget->EnterCount, 1);
	TestEqual(TEXT("Replacing the target sends the first target an exit"), FirstTarget->ExitCount, 1);
	TestEqual(TEXT("Replacing the target sends the second target an enter"), SecondTarget->EnterCount, 1);
	TestEqual(TEXT("Retained owner receives one enter"), Owner->EnterCount, 1);
	TestEqual(TEXT("Retained owner remains penetrating during the target change"), Owner->ExitCount, 0);
	Mode->NotifyPenetration(false, {});
	TestEqual(TEXT("Clearing penetration sends the retained owner an exit"), Owner->ExitCount, 1);
	TestEqual(TEXT("Clearing penetration sends the second target an exit"), SecondTarget->ExitCount, 1);
	TestEqual(TEXT("Clearing penetration does not notify the previous target twice"), FirstTarget->ExitCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModePenetrationReentryTest, "ModularCameraSystem.ThirdPerson.PenetrationCallbackReentryKeepsLatestRecipients", TestFlags)

bool FCameraModePenetrationReentryTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld Fixture;
	Fixture.World->InitializeActorsForPlay(FURL());
	ACameraAssistRegressionTestActor* FirstTarget = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	ACameraAssistRegressionTestActor* SecondTarget = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	ACameraAssistRegressionTestActor* LatestTarget = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	UCameraModeRegressionTestLag* Mode = NewObject<UCameraModeRegressionTestLag>(Fixture.Camera);
	FirstTarget->OnEnter = [Mode, LatestTarget, bDidReenter = false]() mutable
	{
		if (!bDidReenter)
		{
			bDidReenter = true;
			Mode->NotifyPenetration(true, { LatestTarget });
		}
	};
	Mode->NotifyPenetration(true, { FirstTarget, SecondTarget });
	TestEqual(TEXT("First target receives its enter"), FirstTarget->EnterCount, 1);
	TestEqual(TEXT("Reentrant replacement exits the first target"), FirstTarget->ExitCount, 1);
	TestEqual(TEXT("Outer notification does not enter its obsolete second target"), SecondTarget->EnterCount, 0);
	TestEqual(TEXT("Reentrant replacement enters the latest target"), LatestTarget->EnterCount, 1);
	Mode->NotifyPenetration(false, {});
	TestEqual(TEXT("Clearing penetration exits the latest target"), LatestTarget->ExitCount, 1);
	TestEqual(TEXT("Obsolete second target never receives an exit"), SecondTarget->ExitCount, 0);
	TestEqual(TEXT("First target is not exited twice"), FirstTarget->ExitCount, 1);
	FirstTarget->OnEnter = nullptr;
	FirstTarget->OnExit = [Mode]()
	{
		Mode->NotifyPenetration(false, {});
	};
	Mode->NotifyPenetration(true, { FirstTarget, SecondTarget });
	Mode->NotifyPenetration(false, {});
	TestEqual(TEXT("First target receives its second-round enter"), FirstTarget->EnterCount, 2);
	TestEqual(TEXT("Exit reentry exits the first target once per round"), FirstTarget->ExitCount, 2);
	TestEqual(TEXT("Second target receives its second-round enter"), SecondTarget->EnterCount, 1);
	TestEqual(TEXT("Exit reentry does not strand the second target"), SecondTarget->ExitCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeCrouchedCeilingTest, "ModularCameraSystem.ThirdPerson.CrouchedCeilingKeepsCameraOutsideCollision", TestFlags)

bool FCameraModeCrouchedCeilingTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld Fixture(true);
	Fixture.World->InitializeActorsForPlay(FURL());
	ACharacter* Character = Fixture.World->SpawnActor<ACharacter>();
	Character->GetCapsuleComponent()->SetCapsuleSize(34.f, 40.f);
	Character->SetActorLocation(FVector(0.f, 0.f, 40.f));
	Character->bIsCrouched = true;
	AddBlockingBox(Fixture.World, FVector(0.f, 0.f, 210.f), FVector(1000.f, 1000.f, 100.f));
	UCameraModeComponent* Camera = AddThirdPersonCamera(Character);
	FMinimalViewInfo View;
	Camera->GetCameraView(1.f, View);
	const UCameraModeRegressionTestLag* Mode = CastChecked<UCameraModeRegressionTestLag>(Camera->GetActiveCameraMode());
	const FCollisionShape CameraSphere = FCollisionShape::MakeSphere(Mode->PenetrationAvoidanceFeelers[0].Extent);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Character);
	TestTrue(TEXT("Crouched pivot reproduces the 120-unit ceiling overlap"), FMath::IsNearlyEqual(Mode->GetCurrentPivot().Z, 120.0));
	TestTrue(TEXT("Physics query detects the ceiling at the old pivot"), Fixture.World->OverlapBlockingTestByChannel(Mode->GetCurrentPivot(), FQuat::Identity, ECC_Camera, CameraSphere, QueryParams));
	TestFalse(TEXT("Camera's main collision sphere finishes outside the ceiling"), Fixture.World->OverlapBlockingTestByChannel(View.Location, FQuat::Identity, ECC_Camera, CameraSphere, QueryParams));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeCustomTargetIgnoreTest, "ModularCameraSystem.ThirdPerson.CustomTargetIgnoredActorsAffectCollision", TestFlags)

bool FCameraModeCustomTargetIgnoreTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld Fixture(true);
	Fixture.World->InitializeActorsForPlay(FURL());
	ACameraAssistRegressionTestActor* Owner = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	ACameraAssistRegressionTestActor* CustomTarget = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	UCapsuleComponent* TargetCapsule = NewObject<UCapsuleComponent>(CustomTarget);
	CustomTarget->SetRootComponent(TargetCapsule);
	TargetCapsule->SetCapsuleSize(34.f, 88.f);
	TargetCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TargetCapsule->RegisterComponent();
	Owner->PenetrationTarget = CustomTarget;
	AActor* Wall = AddBlockingBox(Fixture.World, FVector(-150.f, 0.f, 0.f), FVector(10.f, 100.f, 100.f));
	TestTrue(TEXT("Physics query detects the blocking wall"), Fixture.World->OverlapBlockingTestByChannel(Wall->GetActorLocation(), FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(15.f)));
	UCameraModeComponent* Camera = AddThirdPersonCamera(Owner);
	FMinimalViewInfo BlockedView;
	Camera->GetCameraView(1.f, BlockedView);
	TestTrue(TEXT("Wall initially pushes the camera in"), BlockedView.Location.X > -150.f);
	TestTrue(TEXT("Custom penetration target's ignore list is queried"), CustomTarget->IgnoreQueryCount > 0);
	CustomTarget->IgnoredActors.Add(Wall);
	FMinimalViewInfo IgnoredView;
	Camera->GetCameraView(1.f, IgnoredView);
	TestTrue(TEXT("Ignoring the wall restores the full camera offset"), IgnoredView.Location.Equals(FVector(-300.f, 0.f, 0.f), 0.001));
	TestTrue(TEXT("Ignoring the wall moves the camera past the blocked view"), IgnoredView.Location.X < BlockedView.Location.X - 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeAsyncIgnoreTest, "ModularCameraSystem.ThirdPerson.PendingAsyncHitUsesCurrentTargetIgnores", TestFlags)

bool FCameraModeAsyncIgnoreTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld Fixture(true, true);
	Fixture.World->InitializeActorsForPlay(FURL());
	ACameraAssistRegressionTestActor* Owner = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	ACameraAssistRegressionTestActor* PreviousTarget = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	ACameraAssistRegressionTestActor* CurrentTarget = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	Owner->PenetrationTarget = PreviousTarget;
	UCameraModeComponent* Camera = AddThirdPersonCamera(Owner);
	FMinimalViewInfo View;
	Camera->GetCameraView(1.f, View);
	UCameraModeRegressionTestLag* Mode = CastChecked<UCameraModeRegressionTestLag>(Camera->GetActiveCameraMode());
	// Unreal's first async request in a fresh world's frame zero has the invalid all-zero handle.
	Fixture.World->Tick(LEVELTICK_All, 1.f / 60.f);
	Mode->bRunAsyncCollision = true;
	AActor* Wall = AddBlockingBox(Fixture.World, FVector(-150.f, 0.f, 0.f), FVector(10.f, 100.f, 100.f));
	Camera->GetCameraView(0.f, View);
	const FTraceHandle PendingHandle = Mode->PenetrationAvoidanceFeelers[0].AsyncTraceHandle;
	if (!TestTrue(TEXT("Production camera submits its asynchronous main feeler"), PendingHandle.IsValid()))
	{
		return false;
	}

	Fixture.World->Tick(LEVELTICK_All, 1.f / 60.f);
	Owner->PenetrationTarget = CurrentTarget;
	CurrentTarget->IgnoredActors.Add(Wall);
	bool bConsumedDuringTick = false;
	// The next tick completes the previous trace before callbacks and advances its frame after callbacks.
	const FDelegateHandle TickHandle = FWorldDelegates::OnWorldPreActorTick.AddLambda([&](UWorld* World, ELevelTick TickType, float DeltaSeconds)
	{
		if (World != Fixture.World || bConsumedDuringTick)
		{
			return;
		}
		bConsumedDuringTick = true;
		FTraceDatum TraceDatum;
		if (!TestTrue(TEXT("Submitted physics trace is ready on its consumption frame"), World->QueryTraceData(PendingHandle, TraceDatum)))
		{
			return;
		}
		TestTrue(TEXT("Pending trace really hit the wall before it became ignored"), TraceDatum.OutHits.ContainsByPredicate([Wall](const FHitResult& Hit) { return Hit.bBlockingHit && Hit.GetActor() == Wall; }));
		Camera->GetCameraView(1.f, View);
		TestTrue(TEXT("Changed target's current ignore list prevents collapse to the stale hit"), View.Location.Equals(FVector(-300.f, 0.f, 0.f), 0.001));
		TestTrue(TEXT("Current custom target supplies its ignored actors when consuming"), CurrentTarget->IgnoreQueryCount > 0);
	});
	Fixture.World->Tick(LEVELTICK_All, 1.f / 60.f);
	FWorldDelegates::OnWorldPreActorTick.Remove(TickHandle);
	TestTrue(TEXT("Camera consumes the trace during the world's next tick"), bConsumedDuringTick);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeBoxCornerAnchorTest, "ModularCameraSystem.ThirdPerson.BoxCornerAnchorKeepsCameraOutsideCollision", TestFlags)

bool FCameraModeBoxCornerAnchorTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld Fixture(true);
	Fixture.World->InitializeActorsForPlay(FURL());
	ACameraAssistRegressionTestActor* Owner = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	AActor* TargetBox = AddBlockingBox(Fixture.World, FVector::ZeroVector, FVector(100.f, 100.f, 100.f));
	Owner->PenetrationTarget = TargetBox;
	AddBlockingBox(Fixture.World, FVector(110.f, 0.f, 0.f), FVector(10.f, 1000.f, 1000.f));
	UCameraModeComponent* Camera = AddThirdPersonCamera(Owner);
	Owner->SetActorLocation(FVector(100.f, 100.f, 0.f));
	FMinimalViewInfo View;
	Camera->GetCameraView(1.f, View);
	const UCameraModeRegressionTestLag* Mode = CastChecked<UCameraModeRegressionTestLag>(Camera->GetActiveCameraMode());
	const FCollisionShape CameraSphere = FCollisionShape::MakeSphere(Mode->PenetrationAvoidanceFeelers[0].Extent);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(TargetBox);
	TestTrue(TEXT("Physics query detects the flush wall at the box-corner pivot"), Fixture.World->OverlapBlockingTestByChannel(Mode->GetCurrentPivot(), FQuat::Identity, ECC_Camera, CameraSphere, QueryParams));
	TestFalse(TEXT("Box-corner safe anchor keeps the camera's main sphere outside the flush wall"), Fixture.World->OverlapBlockingTestByChannel(View.Location, FQuat::Identity, ECC_Camera, CameraSphere, QueryParams));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeInactivePushTest, "ModularCameraSystem.Stack.InactivePushWaitsForActivation", TestFlags)

bool FCameraModeInactivePushTest::RunTest(const FString& Parameters)
{
	UCameraModeComponent* Component = NewObject<UCameraModeComponent>();
	UCameraModeStack* Stack = NewObject<UCameraModeStack>(Component);
	Stack->DeactivateStack();
	Stack->PushCameraMode(UCameraModeRegressionTestA::StaticClass());
	const UCameraModeRegressionTestBase* Mode = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
	TestEqual(TEXT("Pushing into an inactive stack does not activate the mode"), Mode->ActivationCount, 0);
	Stack->ActivateStack();
	Stack->ActivateStack();
	TestEqual(TEXT("Activating the stack activates the mode once"), Mode->ActivationCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeUpdateClearTest, "ModularCameraSystem.Stack.UpdateViewMayClearStack", TestFlags)

bool FCameraModeUpdateClearTest::RunTest(const FString& Parameters)
{
	UCameraModeComponent* Component = NewObject<UCameraModeComponent>();
	UCameraModeStack* Stack = NewObject<UCameraModeStack>(Component);
	Stack->PushCameraMode(UCameraModeRegressionTestA::StaticClass());
	const UCameraModeRegressionTestBase* ModeA = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
	FCameraModeView View;
	Stack->EvaluateStack(0.f, View);
	Stack->PushCameraMode(UCameraModeRegressionTestB::StaticClass());
	UCameraModeRegressionTestBase* ModeB = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
	ModeB->SetBlendWeight(0.25f);
	ModeB->OnUpdate = [Stack]() { Stack->ClearStack(); };
	TestFalse(TEXT("Clearing the stack from UpdateView invalidates its evaluation"), Stack->EvaluateStack(0.f, View));
	TestNull(TEXT("UpdateView clearing leaves no active mode"), Stack->GetActiveCameraMode());
	TestEqual(TEXT("Cleared lower mode deactivates once"), ModeA->DeactivationCount, 1);
	TestEqual(TEXT("Cleared updating mode deactivates once"), ModeB->DeactivationCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModePruningRepushTest, "ModularCameraSystem.Stack.PrunedModeMayRepushDuringDeactivation", TestFlags)

bool FCameraModePruningRepushTest::RunTest(const FString& Parameters)
{
	UCameraModeComponent* Component = NewObject<UCameraModeComponent>();
	UCameraModeStack* Stack = NewObject<UCameraModeStack>(Component);
	Stack->PushCameraMode(UCameraModeRegressionTestA::StaticClass());
	UCameraModeRegressionTestBase* ModeA = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
	FCameraModeView View;
	Stack->EvaluateStack(0.f, View);
	ModeA->OnDeactivate = [Stack]() { Stack->PushCameraMode(UCameraModeRegressionTestA::StaticClass()); };
	Stack->PushCameraMode(UCameraModeRegressionTestB::StaticClass());
	const UCameraModeRegressionTestBase* ModeB = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
	Stack->EvaluateStack(1.f, View);
	TestTrue(TEXT("Deactivation callback repushes the removed mode"), Stack->GetActiveCameraMode() == ModeA);
	TestEqual(TEXT("Pruned mode deactivates once"), ModeA->DeactivationCount, 1);
	TestEqual(TEXT("Repushed mode activates again"), ModeA->ActivationCount, 2);
	TestTrue(TEXT("Repushed stack evaluates next frame"), Stack->EvaluateStack(0.f, View));
	TestTrue(TEXT("Repushed mode starts blending from the retained mode"), View.Location.Equals(ModeB->FixedLocation));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeMeshlessCharacterTest, "ModularCameraSystem.FirstPerson.CharacterWithoutMeshUsesPawnView", TestFlags)

bool FCameraModeMeshlessCharacterTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld Fixture;
	Fixture.World->InitializeActorsForPlay(FURL());
	ACameraRegressionTestMeshlessCharacter* Character = Fixture.World->SpawnActor<ACameraRegressionTestMeshlessCharacter>();
	TestNull(TEXT("Character fixture suppresses its optional mesh"), Character->GetMesh());
	Character->SetActorLocation(FVector(80.f, 50.f, 100.f));
	const FVector PawnViewLocation = Character->GetPawnViewLocation();
	UCameraModeComponent* Camera = AddThirdPersonCamera(Character);
	Camera->DefaultCameraMode = UCameraMode_FirstPerson::StaticClass();
	FMinimalViewInfo View;
	Camera->GetCameraView(1.f, View);
	TestTrue(TEXT("First-person camera falls back to the meshless character's pawn view"), View.Location.Equals(PawnViewLocation));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeInvalidLagStepTest, "ModularCameraSystem.ThirdPerson.InvalidLagTimeStepUsesDirectInterpolation", TestFlags)

bool FCameraModeInvalidLagStepTest::RunTest(const FString& Parameters)
{
	constexpr float DeltaTime = 0.02f;
	const FVector TargetLocation(1000.f, 500.f, 200.f);
	const FRotator TargetRotation(0.f, 90.f, 0.f);
	const float InvalidSteps[] = { 0.f, -0.01f, 1.e-12f };
	for (const float TimeStep : InvalidSteps)
	{
		UCameraModeRegressionTestLag* Mode = NewObject<UCameraModeRegressionTestLag>();
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		Mode->ApplyLag(DeltaTime, Location, Rotation, Location);
		Mode->EnableSubsteppedLag(TimeStep);
		Location = TargetLocation;
		Rotation = TargetRotation;
		Mode->ApplyLag(DeltaTime, TargetLocation, Rotation, Location);
		TestTrue(TEXT("Invalid lag step uses direct location interpolation"), Location.Equals(FMath::VInterpTo(FVector::ZeroVector, TargetLocation, DeltaTime, 10.f)));
		TestTrue(TEXT("Invalid lag step uses direct rotation interpolation"), Rotation.Equals(FMath::QInterpTo(FQuat::Identity, TargetRotation.Quaternion(), DeltaTime, 10.f).Rotator()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeDiscardedBlockersTest, "ModularCameraSystem.ThirdPerson.DiscardedBlockersDoNotMaskRealWall", TestFlags)

bool FCameraModeDiscardedBlockersTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld Fixture(true);
	Fixture.World->InitializeActorsForPlay(FURL());
	ACameraAssistRegressionTestActor* Owner = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	for (int32 Index = 0; Index < 16; ++Index)
	{
		AActor* IgnoredBlocker = AddBlockingBox(Fixture.World, FVector(-30.f - Index * 12.f, 0.f, 0.f), FVector(1.f, 100.f, 100.f));
		IgnoredBlocker->Tags.Add(TEXT("IgnoreCameraCollision"));
	}
	AActor* Wall = AddBlockingBox(Fixture.World, FVector(-260.f, 0.f, 0.f), FVector(5.f, 100.f, 100.f));
	TestTrue(TEXT("Physics query detects the wall behind the sixteen discarded blockers"), Fixture.World->OverlapBlockingTestByChannel(Wall->GetActorLocation(), FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(1.f)));
	UCameraModeComponent* Camera = AddThirdPersonCamera(Owner);
	FMinimalViewInfo View;
	Camera->GetCameraView(1.f, View);
	TestTrue(TEXT("Sixteen discarded blockers do not hide the real wall"), View.Location.X > -260.f && View.Location.X < -210.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeCrouchBlendTest, "ModularCameraSystem.ThirdPerson.RepeatedCrouchTargetPreservesBlendProgress", TestFlags)

bool FCameraModeCrouchBlendTest::RunTest(const FString& Parameters)
{
	const FVector CrouchedOffset(0.f, 0.f, -34.f);
	UCameraModeRegressionTestLag* Repeated = NewObject<UCameraModeRegressionTestLag>();
	Repeated->SetCrouchTarget(CrouchedOffset);
	Repeated->UpdateCrouch(0.1f);
	TestTrue(TEXT("Crouch reaches the halfway offset after 0.1 seconds"), Repeated->GetCrouchOffset().Equals(FVector(0.f, 0.f, -17.f)));
	Repeated->SetCrouchTarget(CrouchedOffset);
	Repeated->UpdateCrouch(0.1f);
	UCameraModeRegressionTestLag* Single = NewObject<UCameraModeRegressionTestLag>();
	Single->SetCrouchTarget(CrouchedOffset);
	Single->UpdateCrouch(0.2f);
	TestTrue(TEXT("Repeated unchanged target completes the crouch blend"), Repeated->GetCrouchOffset().Equals(CrouchedOffset));
	TestTrue(TEXT("Two 0.1-second frames match one 0.2-second frame"), Repeated->GetCrouchOffset().Equals(Single->GetCrouchOffset()));

	UCameraModeRegressionTestLag* Reversal = NewObject<UCameraModeRegressionTestLag>();
	Reversal->SetCrouchTarget(CrouchedOffset);
	Reversal->UpdateCrouch(0.1f);
	Reversal->SetCrouchTarget(FVector::ZeroVector);
	TestTrue(TEXT("Reversing crouch preserves the current offset before updating"), Reversal->GetCrouchOffset().Equals(FVector(0.f, 0.f, -17.f)));
	Reversal->UpdateCrouch(0.1f);
	TestTrue(TEXT("Reversed blend starts from the current crouch offset"), Reversal->GetCrouchOffset().Equals(FVector(0.f, 0.f, -8.5f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeResetBlendTest, "ModularCameraSystem.Stack.ResetInterpolationKeepsBlendComplete", TestFlags)

bool FCameraModeResetBlendTest::RunTest(const FString& Parameters)
{
	UCameraModeComponent* Component = NewObject<UCameraModeComponent>();
	UCameraModeStack* Stack = NewObject<UCameraModeStack>(Component);
	FCameraModeView View;
	Stack->PushCameraMode(UCameraModeRegressionTestA::StaticClass());
	Stack->EvaluateStack(0.f, View);
	Stack->PushCameraMode(UCameraModeRegressionTestB::StaticClass());
	UCameraModeRegressionTestBase* Mode = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
	Stack->EvaluateStack(0.25f, View);
	TestEqual(TEXT("Mode starts partway through its blend"), Mode->GetBlendWeight(), 0.25f);
	Mode->ResetInterpolation();
	Stack->EvaluateStack(0.f, View);
	TestEqual(TEXT("ResetInterpolation completes the blend"), Mode->GetBlendWeight(), 1.f);
	Stack->EvaluateStack(0.f, View);
	TestEqual(TEXT("Completed blend remains complete on the following update"), Mode->GetBlendWeight(), 1.f);
	TestTrue(TEXT("Reset mode remains the full camera view"), View.Location.Equals(Mode->FixedLocation));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeCrouchReactivationTest, "ModularCameraSystem.ThirdPerson.CrouchedReactivationPreservesLaggedView", TestFlags)

bool FCameraModeCrouchReactivationTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld Fixture(true);
	Fixture.World->InitializeActorsForPlay(FURL());
	ACharacter* Character = Fixture.World->SpawnActor<ACharacter>();
	Character->GetCapsuleComponent()->SetCapsuleSize(34.f, 40.f);
	Character->SetActorLocation(FVector(0.f, 0.f, 40.f));
	Character->bIsCrouched = true;
	UCameraModeComponent* Camera = AddThirdPersonCamera(Character);
	FMinimalViewInfo Before;
	Camera->GetCameraView(1.f, Before);
	UCameraModeRegressionTestLag* Mode = CastChecked<UCameraModeRegressionTestLag>(Camera->GetActiveCameraMode());
	Mode->bPreventPenetration = false;
	Mode->SetMaximumPitch(65.f);
	Character->SetActorRotation(FRotator(80.f, 0.f, 0.f));
	Camera->GetCameraView(0.f, Before);
	Mode->EnableLag();
	Camera->GetCameraView(0.f, Before);
	TestTrue(TEXT("Cached camera mode has settled the character's crouch offset"), Mode->GetCrouchOffset().Equals(FVector(0.f, 0.f, -32.f)));
	TestTrue(TEXT("Camera has settled at the configured pitch limit"), FMath::IsNearlyEqual(Before.Rotation.Pitch, 65.0, 0.001));
	Camera->DefaultCameraMode = nullptr;
	FMinimalViewInfo After;
	Camera->GetCameraView(0.f, After);
	Camera->DefaultCameraMode = UCameraModeRegressionTestLag::StaticClass();
	Camera->GetCameraView(1.f / 60.f, After);
	TestTrue(TEXT("Reactivation reuses the cached mode"), Camera->GetActiveCameraMode() == Mode);
	TestTrue(TEXT("Stationary crouched reactivation does not raise the lagged camera"), After.Location.Equals(Before.Location, 0.001));
	TestTrue(TEXT("Reactivation seeds rotation lag at the configured pitch limit"), FMath::IsNearlyEqual(After.Rotation.Pitch, 65.0, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeComponentLifecycleTest, "ModularCameraSystem.Component.DeactivationAndDestructionBalancePenetration", TestFlags)

bool FCameraModeComponentLifecycleTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld Fixture;
	Fixture.World->InitializeActorsForPlay(FURL());
	ACameraAssistRegressionTestActor* Owner = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	ACameraAssistRegressionTestActor* Target = Fixture.World->SpawnActor<ACameraAssistRegressionTestActor>();
	UCameraModeComponent* Camera = AddThirdPersonCamera(Owner);
	Camera->Activate(true);
	FMinimalViewInfo View;
	Camera->GetCameraView(0.f, View);
	UCameraModeRegressionTestLag* Mode = CastChecked<UCameraModeRegressionTestLag>(Camera->GetActiveCameraMode());
	Mode->NotifyPenetration(true, { Owner, Target });
	Camera->Deactivate();
	TestEqual(TEXT("Deactivating camera exits the owner"), Owner->ExitCount, 1);
	TestEqual(TEXT("Deactivating camera exits its penetration target"), Target->ExitCount, 1);
	Camera->Activate();
	Camera->GetCameraView(0.f, View);
	Mode = CastChecked<UCameraModeRegressionTestLag>(Camera->GetActiveCameraMode());
	Mode->NotifyPenetration(true, { Owner, Target });
	TestEqual(TEXT("Reactivated camera can enter its owner again"), Owner->EnterCount, 2);
	TestEqual(TEXT("Reactivated camera can enter its target again"), Target->EnterCount, 2);
	Camera->UnregisterComponent();
	TestEqual(TEXT("Unregistering camera exits its owner"), Owner->ExitCount, 2);
	TestEqual(TEXT("Unregistering camera exits its target"), Target->ExitCount, 2);
	Camera->RegisterComponent();
	Camera->Activate();
	Camera->GetCameraView(0.f, View);
	Mode = CastChecked<UCameraModeRegressionTestLag>(Camera->GetActiveCameraMode());
	Mode->NotifyPenetration(true, { Owner, Target });
	TestEqual(TEXT("Registered camera can enter its owner again"), Owner->EnterCount, 3);
	TestEqual(TEXT("Registered camera can enter its target again"), Target->EnterCount, 3);
	Camera->DestroyComponent();
	TestEqual(TEXT("Destroying camera exits its owner again"), Owner->ExitCount, 3);
	TestEqual(TEXT("Destroying camera exits its target again"), Target->ExitCount, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeUnusedResetTest, "ModularCameraSystem.Stack.ReusedUnevaluatedModeStartsFreshBlend", TestFlags)

bool FCameraModeUnusedResetTest::RunTest(const FString& Parameters)
{
	UCameraModeComponent* Component = NewObject<UCameraModeComponent>();
	UCameraModeStack* Stack = NewObject<UCameraModeStack>(Component);
	Stack->PushCameraMode(UCameraModeRegressionTestA::StaticClass());
	UCameraModeRegressionTestBase* ModeA = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
	Stack->ClearStack();
	Stack->PushCameraMode(UCameraModeRegressionTestB::StaticClass());
	UCameraModeRegressionTestBase* ModeB = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
	FCameraModeView View;
	Stack->EvaluateStack(0.f, View);
	Stack->PushCameraMode(UCameraModeRegressionTestA::StaticClass());
	Stack->EvaluateStack(0.25f, View);
	TestEqual(TEXT("Unused automatic reset does not snap the reused mode"), ModeA->GetBlendWeight(), 0.25f);
	TestEqual(TEXT("Reused blend retains its lower mode"), ModeB->DeactivationCount, 0);

	Stack->ClearStack();
	Stack->PushCameraMode(UCameraModeRegressionTestB::StaticClass());
	Stack->EvaluateStack(0.f, View);
	ModeA->OnActivate = [ModeA]() { ModeA->ResetInterpolation(); };
	Stack->PushCameraMode(UCameraModeRegressionTestA::StaticClass());
	Stack->EvaluateStack(0.25f, View);
	TestEqual(TEXT("Activation callback can request a fresh reset"), ModeA->GetBlendWeight(), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraModeActivationEvaluationTest, "ModularCameraSystem.Stack.ActivationQueriesWaitForAllModeHooks", TestFlags)

bool FCameraModeActivationEvaluationTest::RunTest(const FString& Parameters)
{
	UCameraModeComponent* Component = NewObject<UCameraModeComponent>();
	UCameraModeStack* Stack = NewObject<UCameraModeStack>(Component);
	FCameraModeView View;
	Stack->PushCameraMode(UCameraModeRegressionTestA::StaticClass());
	UCameraModeRegressionTestBase* ModeA = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
	Stack->EvaluateStack(0.f, View);
	Stack->PushCameraMode(UCameraModeRegressionTestB::StaticClass());
	UCameraModeRegressionTestBase* ModeB = CastChecked<UCameraModeRegressionTestBase>(Stack->GetActiveCameraMode());
	Stack->EvaluateStack(0.25f, View);
	Stack->DeactivateStack();
	bool bLowerModeUpdated = false;
	bool bNestedEvaluationSucceeded = true;
	ModeA->OnUpdate = [&bLowerModeUpdated]() { bLowerModeUpdated = true; };
	ModeB->OnActivate = [Stack, &bNestedEvaluationSucceeded]()
	{
		FCameraModeView NestedView;
		bNestedEvaluationSucceeded = Stack->EvaluateStack(0.f, NestedView);
	};
	Stack->ActivateStack();
	TestFalse(TEXT("Activation callback waits for lower mode activation"), bNestedEvaluationSucceeded);
	TestFalse(TEXT("Lower mode is not updated before its activation hook"), bLowerModeUpdated);
	TestTrue(TEXT("Stack evaluates after all modes activate"), Stack->EvaluateStack(0.f, View));
	TestTrue(TEXT("Activated lower mode updates normally"), bLowerModeUpdated);
	return true;
}

#endif
