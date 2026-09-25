#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SPControlSettings.h"
#include "GameFramework/InputSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPRebindingTest, "ShadowProtocol.Controls.AtomicRebinding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPRebindingTest::RunTest(const FString& Parameters)
{
    auto* Input = GetMutableDefault<UInputSettings>();
    const auto Before = Input->GetActionMappings();
    const auto BeforeAxes = Input->GetAxisMappings();
    auto* Settings = NewObject<USPControlSettings>();
    Settings->MovementKeys.Reset();
    FString Error;
    // Disable persistence on swap calls: tests must never change stored player preferences.
    for (FKey Key : {EKeys::Escape, EKeys::Tab, EKeys::W, EKeys::LeftMouseButton})
    {
        Settings->ActionOverrides = {FInputActionKeyMapping(TEXT("Reload"), Key)};
        TestFalse(TEXT("Reserved/occupied keys are denied"), Settings->ApplyActionOverrides(Error));
        TestTrue(TEXT("Denial leaves runtime bindings intact"), Before == Input->GetActionMappings());
        TestFalse(TEXT("Denial has actionable feedback"), Error.IsEmpty());
    }
    Settings->ActionOverrides = {FInputActionKeyMapping(TEXT("Reload"), EKeys::F12)};
    TestTrue(TEXT("An available key can replace Reload"), Settings->ApplyActionOverrides(Error));
    TestTrue(TEXT("New mapping is installed"), Input->GetActionMappings().Contains(Settings->ActionOverrides[0]));
    // Simulates loading a saved swap: neither action may conflict with its old key.
    Settings->ActionOverrides = {FInputActionKeyMapping(TEXT("Reload"), EKeys::LeftMouseButton),
        FInputActionKeyMapping(TEXT("Fire"), EKeys::R)};
    TestTrue(TEXT("Saved swaps validate as one candidate"), Settings->ApplyActionOverrides(Error));
    const auto Swapped = Input->GetActionMappings();
    Settings->ActionOverrides.Add(FInputActionKeyMapping(TEXT("Aim"), EKeys::R));
    TestFalse(TEXT("Duplicate assignments are rejected"), Settings->ApplyActionOverrides(Error));
    TestTrue(TEXT("Invalid batch preserves the previous valid batch"), Swapped == Input->GetActionMappings());

    Settings->ActionOverrides = {FInputActionKeyMapping(TEXT("Reload"), EKeys::F12)};
    TestTrue(TEXT("Reload can be staged on F12 before swap validation"), Settings->ApplyActionOverrides(Error));
    FName Conflict;
    TestTrue(TEXT("Conflict lookup identifies Fire on left mouse"), Settings->FindActionUsingKey(EKeys::LeftMouseButton, TEXT("Reload"), Conflict));
    TestEqual(TEXT("Conflict lookup returns Fire"), Conflict, FName(TEXT("Fire")));
    TestTrue(TEXT("Confirmed swap moves Reload to Fire's key atomically"), Settings->SwapActionBinding(TEXT("Reload"), TEXT("Fire"), EKeys::LeftMouseButton, Error, false));
    const auto AfterConfirmedSwap = Input->GetActionMappings();
    TestTrue(TEXT("Reload receives left mouse after swap"), AfterConfirmedSwap.Contains(FInputActionKeyMapping(TEXT("Reload"), EKeys::LeftMouseButton)));
    TestTrue(TEXT("Fire receives Reload's previous F12 key after swap"), AfterConfirmedSwap.Contains(FInputActionKeyMapping(TEXT("Fire"), EKeys::F12)));

    const auto StableAfterSwap = Input->GetActionMappings();
    TestFalse(TEXT("Stale conflict confirmation is rejected"), Settings->SwapActionBinding(TEXT("Aim"), TEXT("Fire"), EKeys::LeftMouseButton, Error, false));
    TestTrue(TEXT("Rejected stale swap preserves bindings"), StableAfterSwap == Input->GetActionMappings());

    for (FName MultiAction : {FName(TEXT("Reload")), FName(TEXT("Fire"))})
    {
        const FInputActionKeyMapping Alternate(MultiAction, EKeys::F11);
        Input->AddActionMapping(Alternate, false);
        const auto WithAlternate = Input->GetActionMappings();
        const auto PreviousOverrides = Settings->ActionOverrides;
        TestFalse(TEXT("Swap cannot discard either action's alternate key"),
            Settings->SwapActionBinding(TEXT("Reload"), TEXT("Fire"), EKeys::F12, Error, false));
        TestTrue(TEXT("Ambiguous swap preserves runtime mappings"), WithAlternate == Input->GetActionMappings());
        TestTrue(TEXT("Ambiguous swap preserves saved overrides"), PreviousOverrides == Settings->ActionOverrides);
        Input->RemoveActionMapping(Alternate, false);
    }
    const FInputActionKeyMapping PlainReload(TEXT("Reload"), EKeys::LeftMouseButton);
    const FInputActionKeyMapping ChordReload(TEXT("Reload"), EKeys::LeftMouseButton, false, true);
    Input->RemoveActionMapping(PlainReload, false);
    Input->AddActionMapping(ChordReload, false);
    const auto WithChord = Input->GetActionMappings();
    TestFalse(TEXT("Swap cannot discard a modifier chord"),
        Settings->SwapActionBinding(TEXT("Reload"), TEXT("Fire"), EKeys::F12, Error, false));
    TestTrue(TEXT("Rejected chord swap preserves runtime mappings"), WithChord == Input->GetActionMappings());

    const auto Current = Input->GetActionMappings();
    for (const auto& Mapping : Current) Input->RemoveActionMapping(Mapping, false);
    for (const auto& Mapping : Before) Input->AddActionMapping(Mapping, false);
    const auto CurrentAxes = Input->GetAxisMappings();
    for (const auto& Mapping : CurrentAxes) Input->RemoveAxisMapping(Mapping, false);
    for (const auto& Mapping : BeforeAxes) Input->AddAxisMapping(Mapping, false);
    Input->ForceRebuildKeymaps();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPMovementRebindingTest, "ShadowProtocol.Controls.MovementRebinding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPMovementRebindingTest::RunTest(const FString& Parameters)
{
    auto* Input = GetMutableDefault<UInputSettings>();
    const auto Before = Input->GetActionMappings();
    const auto BeforeAxes = Input->GetAxisMappings();
    auto* Settings = NewObject<USPControlSettings>();
    Settings->ActionOverrides.Reset();
    Settings->MovementKeys.Reset();
    FString Error;
    const TArray<FKey> Arrows = {EKeys::Up, EKeys::Down, EKeys::Left, EKeys::Right};
    TestTrue(TEXT("Apply all four arrow keys"), Settings->SetMovementKeys(Arrows, Error, false));
    TestTrue(TEXT("Forward is positive"), Input->GetAxisMappings().Contains(FInputAxisKeyMapping(TEXT("MoveForward"), EKeys::Up, 1.f)));
    TestTrue(TEXT("Backward is negative"), Input->GetAxisMappings().Contains(FInputAxisKeyMapping(TEXT("MoveForward"), EKeys::Down, -1.f)));
    TestTrue(TEXT("Left is negative"), Input->GetAxisMappings().Contains(FInputAxisKeyMapping(TEXT("MoveRight"), EKeys::Left, -1.f)));
    TestTrue(TEXT("Right is positive"), Input->GetAxisMappings().Contains(FInputAxisKeyMapping(TEXT("MoveRight"), EKeys::Right, 1.f)));
    for (const auto& Mapping : BeforeAxes)
        if (Mapping.Key.IsGamepadKey() || Mapping.AxisName == TEXT("Turn") || Mapping.AxisName == TEXT("LookUp"))
            TestTrue(TEXT("Look and gamepad axes survive"), Input->GetAxisMappings().Contains(Mapping));
    const auto ActiveActions = Input->GetActionMappings();
    const auto ActiveAxes = Input->GetAxisMappings();
    for (const FKey BadKey : {EKeys::Up, EKeys::R, EKeys::Escape, EKeys::Tab, EKeys::Enter,
        EKeys::LeftMouseButton, EKeys::MouseX, EKeys::Gamepad_LeftX, EKeys::AnyKey, FKey()})
    {
        TestFalse(TEXT("Invalid/conflicting movement layout is denied"),
            Settings->SetMovementKeys({EKeys::Up, BadKey, EKeys::Left, EKeys::Right}, Error, false));
        TestTrue(TEXT("Rejected layout preserves actions"), ActiveActions == Input->GetActionMappings());
        TestTrue(TEXT("Rejected layout preserves axes"), ActiveAxes == Input->GetAxisMappings());
        TestTrue(TEXT("Rejected layout preserves saved keys"), Settings->MovementKeys == Arrows);
    }
    TestFalse(TEXT("Partial layout is denied"), Settings->SetMovementKeys({EKeys::Up}, Error, false));
    TestTrue(TEXT("Saved layout can reload without duplicating axes"), Settings->ApplyActionOverrides(Error));
    TestTrue(TEXT("Reloaded layout is identical"), ActiveAxes == Input->GetAxisMappings());
    TestTrue(TEXT("Directions can swap in one batch"),
        Settings->SetMovementKeys({EKeys::Down, EKeys::Up, EKeys::Right, EKeys::Left}, Error, false));
    TestTrue(TEXT("Swapped forward direction installed"), Input->GetAxisMappings().Contains(FInputAxisKeyMapping(TEXT("MoveForward"), EKeys::Down, 1.f)));

    Settings->ActionOverrides = {FInputActionKeyMapping(TEXT("Reload"), EKeys::W)};
    TestTrue(TEXT("Former movement key can become an action"), Settings->ApplyActionOverrides(Error));
    const auto WithFreedKey = Input->GetActionMappings();
    const auto BeforeConflict = Input->GetAxisMappings();
    TestFalse(TEXT("Restoring movement cannot steal a saved action key"),
        Settings->SetMovementKeys({}, Error, false));
    TestTrue(TEXT("Cross-conflict preserves actions"), WithFreedKey == Input->GetActionMappings());
    TestTrue(TEXT("Cross-conflict preserves axes"), BeforeConflict == Input->GetAxisMappings());
    Settings->ActionOverrides = {FInputActionKeyMapping(TEXT("Reload"), EKeys::Up)};
    TestFalse(TEXT("Action cannot steal a current movement key"), Settings->ApplyActionOverrides(Error));

    Settings->MouseSensitivity = 1.7f;
    Settings->bToggleAim = true;
    Settings->bHighContrastHUD = true;
    Settings->ResetAllBindings(false);
    TestTrue(TEXT("Full reset clears both override sets"), Settings->ActionOverrides.IsEmpty() && Settings->MovementKeys.IsEmpty());
    TestTrue(TEXT("Full reset restores W forward"), Input->GetAxisMappings().Contains(FInputAxisKeyMapping(TEXT("MoveForward"), EKeys::W, 1.f)));
    TestEqual(TEXT("Reset preserves sensitivity"), Settings->MouseSensitivity, 1.7f);
    TestTrue(TEXT("Reset preserves aim/HUD preferences"), Settings->bToggleAim && Settings->bHighContrastHUD);

    // Restore the runtime snapshot; no test mutation is persisted.
    const auto Current = Input->GetActionMappings();
    for (const auto& Mapping : Current) Input->RemoveActionMapping(Mapping, false);
    for (const auto& Mapping : Before) Input->AddActionMapping(Mapping, false);
    const auto CurrentAxes = Input->GetAxisMappings();
    for (const auto& Mapping : CurrentAxes) Input->RemoveAxisMapping(Mapping, false);
    for (const auto& Mapping : BeforeAxes) Input->AddAxisMapping(Mapping, false);
    Input->ForceRebuildKeymaps();
    return true;
}
#endif
