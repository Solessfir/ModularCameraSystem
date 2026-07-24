// Copyright Solessfir. All Rights Reserved.

#include "ModularCameraSystem.h"
#include "Camera/CameraModeComponent.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "FModularCameraSystemModule"

DEFINE_LOG_CATEGORY(LogModularCameraSystem);

// Draw the local player's active camera mode stack.
static TAutoConsoleVariable<bool> CVarShowCameraModeDebug(
	TEXT("ModularCameraSystem.ShowDebug"),
	false,
	TEXT("Draws the local player's Camera Mode Component debug info (FOV, blend weights, penetration feelers)."),
	ECVF_Default);

void FModularCameraSystemModule::StartupModule()
{
	DebugDrawHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateRaw(this, &FModularCameraSystemModule::DrawCameraModeDebug));
}

void FModularCameraSystemModule::ShutdownModule()
{
	UDebugDrawService::Unregister(DebugDrawHandle);
}

bool FModularCameraSystemModule::IsShowDebugEnabled()
{
	return CVarShowCameraModeDebug.GetValueOnGameThread();
}

void FModularCameraSystemModule::DrawCameraModeDebug(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (!IsShowDebugEnabled() || !PlayerController || !Canvas)
	{
		return;
	}

	if (const UCameraModeComponent* CameraComponent = UCameraModeComponent::FindCameraComponent(PlayerController->GetPawn()))
	{
		// AHUD normally initializes this cursor/font before any DisplayDebug call - we're drawing outside that pipeline (via UDebugDrawService directly).
		// We have to do it ourselves, otherwise DrawDebug's text (Location/Rotation/FOV/Stack) never actually shows up.
		Canvas->DisplayDebugManager.Initialize(Canvas, GEngine->GetSmallFont(), FVector2D(4.f, 24.f));
		CameraComponent->DrawDebug(Canvas);
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FModularCameraSystemModule, ModularCameraSystem)
