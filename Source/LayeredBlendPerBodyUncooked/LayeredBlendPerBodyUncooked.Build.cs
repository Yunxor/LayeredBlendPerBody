using UnrealBuildTool;

public class LayeredBlendPerBodyUncooked : ModuleRules
{
    public LayeredBlendPerBodyUncooked(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "AnimationCore",
                "AnimGraph",
                "AnimGraphRuntime",
                "AnimationModifiers",
                "AnimationBlueprintLibrary",
                "AnimationModifierLibrary",
                "Core",
                "CoreUObject",
                "Engine",
                "LayeredBlendPerBody",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "SignalProcessing"
            }
        );
        
        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "BlueprintGraph",
                    "EditorFramework",
                    "Kismet",
                    "ToolMenus",
                    "UnrealEd",
                }
            );
        };
    }
}
