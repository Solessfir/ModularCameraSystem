// Copyright Solessfir. All Rights Reserved.

using UnrealBuildTool;

public class ModularCameraSystemEditor : ModuleRules
{
	public ModularCameraSystemEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.Add("Core");

		PrivateDependencyModuleNames.AddRange([
			"CoreUObject",
			"UnrealEd",
			"Slate",
			"SlateCore"
		]);
	}
}
