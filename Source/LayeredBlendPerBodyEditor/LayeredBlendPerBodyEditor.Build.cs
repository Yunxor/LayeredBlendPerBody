using UnrealBuildTool;

public class LayeredBlendPerBodyEditor : ModuleRules
{
    public LayeredBlendPerBodyEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "AnimationCore",
                "LayeredBlendPerBody",
                "AnimGraph",
                "AnimGraphRuntime",
                "AnimationModifiers",
                "AnimationBlueprintLibrary",
                "AnimationModifierLibrary",
                "Core",
                "CoreUObject",
                "Engine",
                "UMG",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "ApplicationCore",
                "AssetDefinition",
                "Slate",
                "SlateCore",
                "SignalProcessing",
                "BlueprintGraph",
                "EditorWidgets",
                "EditorFramework",
                "GraphEditor",
                "InputCore",
                "Kismet",
                "KismetWidgets",
                "ToolMenus",
                "UnrealEd",
                "PropertyEditor", 
            }
        );
    }
}