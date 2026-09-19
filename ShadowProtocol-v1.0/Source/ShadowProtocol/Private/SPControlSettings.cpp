#include "SPControlSettings.h"
#include "GameFramework/InputSettings.h"

namespace
{
const TArray<FInputActionKeyMapping>& OriginalMappings()
{
    // Capture before the first override, including project-specific defaults.
    static const TArray<FInputActionKeyMapping> Original = GetDefault<UInputSettings>()->GetActionMappings();
    return Original;
}
void Install(const TArray<FInputActionKeyMapping>& Mappings)
{
    auto* Input = GetMutableDefault<UInputSettings>();
    const auto Old = Input->GetActionMappings();
    for (const auto& Mapping : Old) Input->RemoveActionMapping(Mapping, false);
    for (const auto& Mapping : Mappings) Input->AddActionMapping(Mapping, false);
    Input->ForceRebuildKeymaps();
    // Persist only our overrides in GameUserSettings, never rewrite project Input defaults.
}
}

bool USPControlSettings::CanRebindAction(FName Action)
{
    static const TArray<FName> Allowed = {TEXT("Fire"), TEXT("Aim"), TEXT("Reload"), TEXT("Crouch"),
        TEXT("Sprint"), TEXT("LeanLeft"), TEXT("LeanRight"), TEXT("Vault"), TEXT("CycleEquipment"),
        TEXT("ThrowEquipment"), TEXT("CycleOptic"), TEXT("SquadOrder"), TEXT("Fortify")};
    return Allowed.Contains(Action);
}

bool USPControlSettings::ApplyActionOverrides(FString& Error)
{
    TArray<FInputActionKeyMapping> Candidate = OriginalMappings();
    const auto* Input = GetDefault<UInputSettings>();
    TSet<FName> Seen;
    for (const auto& Override : ActionOverrides)
    {
        const FKey Key = Override.Key;
        if (!CanRebindAction(Override.ActionName) || Seen.Contains(Override.ActionName)
            || !Key.IsValid() || !Key.IsBindableToActions() || Key.IsTouch() || Key.IsGesture() || Key.IsGamepadKey() || Key.IsAnalog() || Key == EKeys::AnyKey
            || Key == EKeys::Escape || Key == EKeys::Tab || Key == EKeys::Enter
            || Key == EKeys::LeftCommand || Key == EKeys::RightCommand
            || Input->ConsoleKeys.Contains(Key) || Override.bShift || Override.bCtrl || Override.bAlt || Override.bCmd)
        {
            Error = TEXT("Choose one keyboard key or mouse button. Menu, console and system keys are reserved.");
            return false;
        }
        Seen.Add(Override.ActionName);
        for (const auto& Axis : Input->GetAxisMappings())
            if (Axis.Key == Key) { Error = TEXT("That key is used for movement or look. Choose a different key."); return false; }
    }
    // Remove every overridden action first, allowing saved swaps to reload atomically.
    Candidate.RemoveAll([&](const FInputActionKeyMapping& Mapping)
        { return Seen.Contains(Mapping.ActionName) && !Mapping.Key.IsGamepadKey(); });
    for (const auto& Override : ActionOverrides)
    {
        for (const auto& Existing : Candidate)
            if (Existing.Key == Override.Key && Existing.ActionName != Override.ActionName)
            {
                Error = FString::Printf(TEXT("That key is already assigned to %s. Choose another key first."), *Existing.ActionName.ToString());
                return false;
            }
        Candidate.Add(Override);
    }
    Install(Candidate);
    Error.Reset();
    return true;
}

bool USPControlSettings::RebindAction(FName Action, FKey Key, FString& Error)
{
    const auto Previous = ActionOverrides;
    ActionOverrides.RemoveAll([&](const FInputActionKeyMapping& Mapping) { return Mapping.ActionName == Action; });
    ActionOverrides.Add(FInputActionKeyMapping(Action, Key));
    if (!ApplyActionOverrides(Error)) { ActionOverrides = Previous; return false; }
    SaveConfig();
    return true;
}

void USPControlSettings::ResetActionBindings()
{
    ActionOverrides.Reset();
    Install(OriginalMappings());
    SaveConfig();
}
