using UnrealBuildTool;
public class ShadowProtocol : ModuleRules
{
    public ShadowProtocol(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
            "UMG", "Slate", "SlateCore", "GameplayTags", "AIModule",
            "NavigationSystem", "OnlineSubsystem", "OnlineSubsystemUtils", "NetCore"
        });
    }
}
