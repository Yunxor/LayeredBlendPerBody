// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LayeredBlendPerBody : ModuleRules
{
	public LayeredBlendPerBody(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AnimationCore",
				"AnimGraphRuntime",
				"Core",
				"CoreUObject",
				"Engine",
				"StructUtils",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
			}
		);
	}
}
