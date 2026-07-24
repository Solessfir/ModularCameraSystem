// Copyright Solessfir. All Rights Reserved.

#include "ModularCameraSystemEditor.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/AppStyle.h"

void FModularCameraSystemEditorModule::StartupModule()
{
	StyleSet = MakeShared<FSlateStyleSet>(FName("ModularCameraSystemEditorStyle"));

	// Small badge icon
	FSlateBrush* Icon = new FSlateBrush(*FAppStyle::Get().GetBrush("ClassIcon.CameraComponent"));
	StyleSet->Set("ClassIcon.CameraMode", Icon);

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FModularCameraSystemEditorModule::ShutdownModule()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}
}

IMPLEMENT_MODULE(FModularCameraSystemEditorModule, ModularCameraSystemEditor)
