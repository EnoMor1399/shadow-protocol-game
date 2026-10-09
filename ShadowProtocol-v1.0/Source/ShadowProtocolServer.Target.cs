using UnrealBuildTool;

public class ShadowProtocolServerTarget : TargetRules
{
    public ShadowProtocolServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        ExtraModuleNames.Add("ShadowProtocol");
    }
}
