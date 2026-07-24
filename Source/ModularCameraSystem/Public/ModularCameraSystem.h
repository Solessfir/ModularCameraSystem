// Copyright Solessfir. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class UCanvas;
class APlayerController;

DECLARE_LOG_CATEGORY_EXTERN(LogModularCameraSystem, Log, All);

class MODULARCAMERASYSTEM_API FModularCameraSystemModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

	// Whether `ModularCameraSystem.ShowDebug` is currently on - other debug draws in this plugin
	// (e.g. camera lag markers) gate off this instead of adding their own separate toggle.
	static bool IsShowDebugEnabled();

private:
	void DrawCameraModeDebug(UCanvas* Canvas, APlayerController* PlayerController);

	FDelegateHandle DebugDrawHandle;
};
