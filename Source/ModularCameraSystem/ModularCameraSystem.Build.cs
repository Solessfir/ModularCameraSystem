// Copyright Solessfir. All Rights Reserved.

using UnrealBuildTool;

public class ModularCameraSystem : ModuleRules
{
	public ModularCameraSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
		[
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags"
		]);
	}
}
