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
    auto* Settings = NewObject<USPControlSettings>();
    FString Error;
    // No SaveConfig calls: the test never changes stored player preferences.
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
    TestTrue(TEXT("Confirmed swap moves Reload to Fire's key atomically"), Settings->SwapActionBinding(TEXT("Reload"), TEXT("Fire"), EKeys::LeftMouseButton, Error));
    const auto AfterConfirmedSwap = Input->GetActionMappings();
    TestTrue(TEXT("Reload receives left mouse after swap"), AfterConfirmedSwap.Contains(FInputActionKeyMapping(TEXT("Reload"), EKeys::LeftMouseButton)));
    TestTrue(TEXT("Fire receives Reload's previous F12 key after swap"), AfterConfirmedSwap.Contains(FInputActionKeyMapping(TEXT("Fire"), EKeys::F12)));

    const auto StableAfterSwap = Input->GetActionMappings();
    TestFalse(TEXT("Stale conflict confirmation is rejected"), Settings->SwapActionBinding(TEXT("Aim"), TEXT("Fire"), EKeys::LeftMouseButton, Error));
    TestTrue(TEXT("Rejected stale swap preserves bindings"), StableAfterSwap == Input->GetActionMappings());

    const auto Current = Input->GetActionMappings();
    for (const auto& Mapping : Current) Input->RemoveActionMapping(Mapping, false);
    for (const auto& Mapping : Before) Input->AddActionMapping(Mapping, false);
    Input->ForceRebuildKeymaps();
    return true;
}
#endif
