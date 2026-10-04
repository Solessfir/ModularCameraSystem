// Copyright Solessfir. All Rights Reserved.

#include "CameraModeRegressionTestTypes.h"
#include "Camera/CameraModeComponent.h"

UCameraModeRegressionTestBase::UCameraModeRegressionTestBase()
{
	BlendTime = 1.f;
	BlendFunction = ECameraModeBlendFunction::Linear;
}

void UCameraModeRegressionTestBase::UpdateView_Implementation(float DeltaTime)
{
	View.Location = FixedLocation;
	View.Rotation = FixedRotation;
	View.ControlRotation = FixedControlRotation;
	View.FieldOfView = FixedFieldOfView;
	if (OnUpdate)
	{
		OnUpdate();
	}
}

UCameraModeRegressionTestB::UCameraModeRegressionTestB()
{
	FixedLocation = FVector(100.f, 200.f, 300.f);
	FixedFieldOfView = 90.f;
}

UCameraModeRegressionTestC::UCameraModeRegressionTestC()
{
	FixedLocation = FVector(300.f, 100.f, -100.f);
	FixedFieldOfView = 120.f;
}

UCameraModeRegressionTestLag::UCameraModeRegressionTestLag()
{
	TargetOffsetX.GetRichCurve()->Reset();
	TargetOffsetX.GetRichCurve()->AddKey(0.f, -300.f);
	TargetOffsetY.GetRichCurve()->Reset();
	TargetOffsetY.GetRichCurve()->AddKey(0.f, 0.f);
	TargetOffsetZ.GetRichCurve()->Reset();
	TargetOffsetZ.GetRichCurve()->AddKey(0.f, 0.f);
	bDoPredictiveAvoidance = false;
	bRunAsyncCollision = false;
}

void UCameraActivationRegressionTestReceiver::OnCameraActivated(UActorComponent* Component, bool bReset)
{
	UCameraModeComponent* Camera = CastChecked<UCameraModeComponent>(Component);
	Camera->GetBlendInfo(BlendWeight, BlendTag);
	FMinimalViewInfo View;
	Camera->GetCameraView(0.f, View);
	ViewLocation = View.Location;
	ViewFieldOfView = View.FOV;
	++ActivationCount;
}

void UCameraActivationRegressionTestReceiver::OnCameraDeactivated(UActorComponent* Component)
{
	UCameraModeComponent* Camera = CastChecked<UCameraModeComponent>(Component);
	FMinimalViewInfo View;
	Camera->GetCameraView(0.f, View);
	ViewLocation = View.Location;
	ViewFieldOfView = View.FOV;
	++DeactivationCount;
}

ACameraRegressionTestMeshlessCharacter::ACameraRegressionTestMeshlessCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(ACharacter::MeshComponentName))
{
}
