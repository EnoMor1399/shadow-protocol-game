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
const TArray<FInputAxisKeyMapping>& OriginalAxes()
{
    static const TArray<FInputAxisKeyMapping> Original = GetDefault<UInputSettings>()->GetAxisMappings();
    return Original;
}
bool IsKeyboardKey(FKey Key)
{
    return Key.IsValid() && Key.IsBindableToActions() && !Key.IsGamepadKey() && !Key.IsAnalog()
        && !Key.IsMouseButton() && !Key.IsTouch() && !Key.IsGesture() && Key != EKeys::AnyKey;
}
void Install(const TArray<FInputActionKeyMapping>& Mappings, const TArray<FInputAxisKeyMapping>& Axes)
{
    auto* Input = GetMutableDefault<UInputSettings>();
    const auto Old = Input->GetActionMappings();
    for (const auto& Mapping : Old) Input->RemoveActionMapping(Mapping, false);
    for (const auto& Mapping : Mappings) Input->AddActionMapping(Mapping, false);
    const auto OldAxes = Input->GetAxisMappings();
    for (const auto& Mapping : OldAxes) Input->RemoveAxisMapping(Mapping, false);
    for (const auto& Mapping : Axes) Input->AddAxisMapping(Mapping, false);
    Input->ForceRebuildKeymaps();
    // Persist only our overrides in GameUserSettings, never rewrite project Input defaults.
}
}

bool USPControlSettings::CanRebindAction(FName Action)
{
    static const TArray<FName> Allowed = {TEXT("Fire"), TEXT("Aim"), TEXT("Reload"), TEXT("Crouch"),
        TEXT("Sprint"), TEXT("LeanLeft"), TEXT("LeanRight"), TEXT("Vault"), TEXT("CycleEquipment"),
        TEXT("ThrowEquipment"), TEXT("CycleOptic"), TEXT("SquadOrder"), TEXT("Fortify"), TEXT("Scoreboard")};
    return Allowed.Contains(Action);
}

bool USPControlSettings::ApplyActionOverrides(FString& Error)
{
    TArray<FInputActionKeyMapping> Candidate = OriginalMappings();
    const auto* Input = GetDefault<UInputSettings>();
    TArray<FInputAxisKeyMapping> CandidateAxes = OriginalAxes();
    if (!MovementKeys.IsEmpty())
    {
        if (MovementKeys.Num() != 4)
        {
            Error = TEXT("Choose all four movement keys before applying.");
            return false;
        }
        // Replace keyboard movement only; retain mouse look, analog and gamepad axes.
        CandidateAxes.RemoveAll([](const FInputAxisKeyMapping& Mapping)
        {
            return (Mapping.AxisName == TEXT("MoveForward") || Mapping.AxisName == TEXT("MoveRight"))
                && IsKeyboardKey(Mapping.Key);
        });
        TSet<FKey> MovementSeen;
        for (int32 Index = 0; Index < MovementKeys.Num(); ++Index)
        {
            const FKey Key = MovementKeys[Index];
            if (!IsKeyboardKey(Key) || MovementSeen.Contains(Key) || Key == EKeys::Escape
                || Key == EKeys::Tab || Key == EKeys::Enter || Key == EKeys::LeftCommand
                || Key == EKeys::RightCommand || Input->ConsoleKeys.Contains(Key))
            {
                Error = TEXT("Movement needs four different keyboard keys. Menu, console, mouse and gamepad keys are unavailable.");
                return false;
            }
            for (const auto& Axis : CandidateAxes)
                if (Axis.Key == Key)
                {
                    Error = FString::Printf(TEXT("%s is already used by %s. Choose another movement key."),
                        *Key.GetDisplayName().ToString(), *Axis.AxisName.ToString());
                    return false;
                }
            MovementSeen.Add(Key);
        }
        CandidateAxes.Add(FInputAxisKeyMapping(TEXT("MoveForward"), MovementKeys[0], 1.f));
        CandidateAxes.Add(FInputAxisKeyMapping(TEXT("MoveForward"), MovementKeys[1], -1.f));
        CandidateAxes.Add(FInputAxisKeyMapping(TEXT("MoveRight"), MovementKeys[2], -1.f));
        CandidateAxes.Add(FInputAxisKeyMapping(TEXT("MoveRight"), MovementKeys[3], 1.f));
    }
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
        for (const auto& Axis : CandidateAxes)
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
    for (const FKey Key : MovementKeys)
        for (const auto& Action : Candidate)
            if (Action.Key == Key)
            {
                Error = FString::Printf(TEXT("%s is assigned to %s. Rebind that action first or choose another movement key."),
                    *Key.GetDisplayName().ToString(), *Action.ActionName.ToString());
                return false;
            }
    Install(Candidate, CandidateAxes);
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

bool USPControlSettings::FindActionUsingKey(FKey Key, FName ExcludingAction, FName& OutAction) const
{
    OutAction = NAME_None;
    if (!Key.IsValid()) return false;

    const auto* Input = GetDefault<UInputSettings>();
    for (const auto& Mapping : Input->GetActionMappings())
    {
        if (Mapping.Key == Key
            && Mapping.ActionName != ExcludingAction
            && !Mapping.Key.IsGamepadKey()
            && CanRebindAction(Mapping.ActionName))
        {
            OutAction = Mapping.ActionName;
            return true;
        }
    }

    return false;
}

bool USPControlSettings::SwapActionBinding(FName Action, FName ConflictingAction, FKey NewKey, FString& Error, bool bSaveSettings)
{
    if (!CanRebindAction(Action) || !CanRebindAction(ConflictingAction) || Action == ConflictingAction)
    {
        Error = TEXT("That binding cannot be swapped.");
        return false;
    }

    FName CurrentOwner;
    if (!FindActionUsingKey(NewKey, Action, CurrentOwner) || CurrentOwner != ConflictingAction)
    {
        Error = TEXT("The binding changed before the swap could be confirmed. Review the current controls and try again.");
        return false;
    }

    const auto* Input = GetDefault<UInputSettings>();
    FKey PreviousActionKey;
    int32 ActionKeyCount = 0;
    int32 ConflictKeyCount = 0;
    bool bHasModifiers = false;
    for (const auto& Mapping : Input->GetActionMappings())
    {
        if (Mapping.Key.IsGamepadKey()) continue;
        if (Mapping.ActionName != Action && Mapping.ActionName != ConflictingAction) continue;
        bHasModifiers |= Mapping.bShift || Mapping.bCtrl || Mapping.bAlt || Mapping.bCmd;
        if (Mapping.ActionName == Action)
        {
            ++ActionKeyCount;
            PreviousActionKey = Mapping.Key;
        }
        else ++ConflictKeyCount;
    }

    // A two-key swap must not silently discard alternate keys or modifier chords.
    if (ActionKeyCount != 1 || ConflictKeyCount != 1 || bHasModifiers
        || !PreviousActionKey.IsValid() || PreviousActionKey == NewKey)
    {
        Error = TEXT("Swap requires one unmodified keyboard or mouse binding per action. Choose a different key or assign a single binding first.");
        return false;
    }

    const auto PreviousOverrides = ActionOverrides;
    ActionOverrides.RemoveAll([&](const FInputActionKeyMapping& Mapping)
        { return Mapping.ActionName == Action || Mapping.ActionName == ConflictingAction; });
    ActionOverrides.Add(FInputActionKeyMapping(Action, NewKey));
    ActionOverrides.Add(FInputActionKeyMapping(ConflictingAction, PreviousActionKey));

    if (!ApplyActionOverrides(Error))
    {
        ActionOverrides = PreviousOverrides;
        return false;
    }

    if (bSaveSettings) SaveConfig();
    Error.Reset();
    return true;
}

bool USPControlSettings::SetMovementKeys(const TArray<FKey>& Keys, FString& Error, bool bSaveSettings)
{
    const auto Previous = MovementKeys;
    MovementKeys = Keys;
    if (!ApplyActionOverrides(Error)) { MovementKeys = Previous; return false; }
    if (bSaveSettings) SaveConfig();
    return true;
}

void USPControlSettings::ResetAllBindings(bool bSaveSettings)
{
    ActionOverrides.Reset();
    MovementKeys.Reset();
    Install(OriginalMappings(), OriginalAxes());
    if (bSaveSettings) SaveConfig();
}

float USPControlSettings::GetSafeCrosshairScale() const
{
    return FMath::IsFinite(CrosshairScale) ? FMath::Clamp(CrosshairScale, 0.75f, 2.5f) : 1.f;
}

FString USPControlSettings::GetActionKeyLabel(FName Action)
{
    FString Label;
    for (const auto& Mapping : GetDefault<UInputSettings>()->GetActionMappings())
    {
        if (Mapping.ActionName != Action || Mapping.Key.IsGamepadKey()) continue;
        FString Key;
        if (Mapping.bCtrl) Key += TEXT("Ctrl+");
        if (Mapping.bAlt) Key += TEXT("Alt+");
        if (Mapping.bShift) Key += TEXT("Shift+");
        if (Mapping.bCmd) Key += TEXT("Cmd+");
        Key += Mapping.Key.GetDisplayName().ToString();
        if (!Label.IsEmpty()) Label += TEXT(" / ");
        Label += Key;
    }
    return Label.IsEmpty() ? TEXT("Unbound") : Label;
}

float USPControlSettings::GetSafeAimSensitivityMultiplier() const
{
    return FMath::IsFinite(AimSensitivityMultiplier) ? FMath::Clamp(AimSensitivityMultiplier, 0.1f, 1.f) : 1.f;
}
