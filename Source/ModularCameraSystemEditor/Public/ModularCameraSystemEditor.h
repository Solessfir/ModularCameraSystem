// Copyright Solessfir. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"

class FModularCameraSystemEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

private:
	TSharedPtr<class FSlateStyleSet> StyleSet;
};
