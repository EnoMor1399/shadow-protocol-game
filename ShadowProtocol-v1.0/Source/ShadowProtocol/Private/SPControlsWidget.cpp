#include "SPControlSettings.h"
#include "SPCharacter.h"
#include "Components/InputKeySelector.h"
#include "SPControlsWidget.h"
#include "SPObserverPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "GameFramework/InputSettings.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> USPControlsWidget::RebuildWidget()
{
    SetIsFocusable(true);
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        auto* Backdrop = WidgetTree->ConstructWidget<UBorder>();
        Backdrop->SetBrushColor(FLinearColor(0.008f, 0.014f, 0.023f, 0.97f));
        Backdrop->SetPadding(FMargin(24));
        Backdrop->SetHorizontalAlignment(HAlign_Center);
        Backdrop->SetVerticalAlignment(VAlign_Fill);
        auto* Width = WidgetTree->ConstructWidget<USizeBox>();
        Width->SetMaxDesiredWidth(720);
        Backdrop->SetContent(Width);
        auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
        Width->SetContent(Scroll);
        auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
        Scroll->AddChild(Column);
        const auto Label = [&](const FString& Text, int32 Size, FLinearColor Color)
        {
            auto* Item = WidgetTree->ConstructWidget<UTextBlock>();
            Item->SetText(FText::FromString(Text));
            Item->SetAutoWrapText(true);
            Item->SetColorAndOpacity(FSlateColor(Color));
            auto Font = Item->GetFont(); Font.Size = Size; Item->SetFont(Font);
            Column->AddChildToVerticalBox(Item)->SetPadding(FMargin(0, 8));
            return Item;
        };
        const auto Button = [&](const TCHAR* Text)
        {
            auto* Item = WidgetTree->ConstructWidget<UButton>();
            auto* Caption = WidgetTree->ConstructWidget<UTextBlock>();
            Caption->SetText(FText::FromString(Text));
            Caption->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
            Item->SetContent(Caption);
            Column->AddChildToVerticalBox(Item)->SetPadding(FMargin(0, 10));
            return Item;
        };
        Label(TEXT("SHADOW PROTOCOL / CONTROLS"), 28, FLinearColor(0.35f, 0.85f, 0.8f));
        Label(TEXT("The online match continues while this panel is open. Esc closes this panel. Tab moves between controls."), 16, FLinearColor::White);
        ResumeButton = Button(TEXT("Return to game / ready room"));
        ResumeButton->OnClicked.AddUniqueDynamic(this, &USPControlsWidget::CloseControls);
        SensitivityLabel = Label(TEXT("Mouse sensitivity"), 20, FLinearColor::White);
        SensitivitySlider = WidgetTree->ConstructWidget<USlider>();
        SensitivitySlider->SetMinValue(0.25f); SensitivitySlider->SetMaxValue(3.f);
        SensitivitySlider->SetStepSize(0.05f);
        SensitivitySlider->OnValueChanged.AddUniqueDynamic(this, &USPControlsWidget::SetSensitivity);
        Column->AddChildToVerticalBox(SensitivitySlider);
        AimSensitivityLabel = Label(TEXT("Aim sensitivity"), 18, FLinearColor::White);
        AimSensitivitySlider = WidgetTree->ConstructWidget<USlider>();
        AimSensitivitySlider->SetMinValue(0.1f); AimSensitivitySlider->SetMaxValue(1.f);
        AimSensitivitySlider->SetStepSize(0.05f);
        AimSensitivitySlider->OnValueChanged.AddUniqueDynamic(this, &USPControlsWidget::SetAimSensitivity);
        Column->AddChildToVerticalBox(AimSensitivitySlider);
        Label(TEXT("Aim sensitivity scales both mouse axes while aiming. 1.00x keeps your normal sensitivity."), 16, FLinearColor::White);
        InvertCheck = WidgetTree->ConstructWidget<UCheckBox>();
        auto* InvertLabel = WidgetTree->ConstructWidget<UTextBlock>();
        InvertLabel->SetText(FText::FromString(TEXT("Invert vertical mouse look")));
        InvertCheck->SetContent(InvertLabel);
        InvertCheck->OnCheckStateChanged.AddUniqueDynamic(this, &USPControlsWidget::SetInvert);
        Column->AddChildToVerticalBox(InvertCheck)->SetPadding(FMargin(0, 12));
        const auto Check = [&](const TCHAR* Caption)
        {
            auto* Item = WidgetTree->ConstructWidget<UCheckBox>();
            auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
            Text->SetText(FText::FromString(Caption)); Text->SetAutoWrapText(true);
            Item->SetContent(Text);
            Column->AddChildToVerticalBox(Item)->SetPadding(FMargin(0, 8));
            return Item;
        };
        ToggleAimCheck = Check(TEXT("Press to toggle aiming"));
        ToggleAimCheck->OnCheckStateChanged.AddUniqueDynamic(this, &USPControlsWidget::SetToggleAim);
        auto* Reset = Button(TEXT("Reset mouse settings"));
        Reset->OnClicked.AddUniqueDynamic(this, &USPControlsWidget::ResetDefaults);
        Label(TEXT("HUD READABILITY"), 20, FLinearColor(0.35f, 0.85f, 0.8f));
        ContrastCheck = Check(TEXT("High-contrast combat HUD"));
        ContrastCheck->OnCheckStateChanged.AddUniqueDynamic(this, &USPControlsWidget::SetHUDContrast);
        CrosshairCheck = Check(TEXT("Show crosshair"));
        CrosshairCheck->OnCheckStateChanged.AddUniqueDynamic(this, &USPControlsWidget::SetCrosshairVisible);
        HintsCheck = Check(TEXT("Show HUD help text"));
        HintsCheck->OnCheckStateChanged.AddUniqueDynamic(this, &USPControlsWidget::SetHUDHints);
        ScoreboardToggleCheck = Check(TEXT("Press to toggle scoreboard (instead of holding)"));
        ScoreboardToggleCheck->OnCheckStateChanged.AddUniqueDynamic(this, &USPControlsWidget::SetScoreboardToggle);
        CrosshairLabel = Label(TEXT("Crosshair size"), 18, FLinearColor::White);
        CrosshairSlider = WidgetTree->ConstructWidget<USlider>();
        CrosshairSlider->SetMinValue(0.75f); CrosshairSlider->SetMaxValue(2.5f); CrosshairSlider->SetStepSize(0.05f);
        CrosshairSlider->OnValueChanged.AddUniqueDynamic(this, &USPControlsWidget::SetCrosshairScale);
        Column->AddChildToVerticalBox(CrosshairSlider)->SetPadding(FMargin(0, 8));
        auto* ResetHUD = Button(TEXT("Reset HUD preferences"));
        ResetHUD->OnClicked.AddUniqueDynamic(this, &USPControlsWidget::ResetHUDPreferences);
        Label(TEXT("HUD changes appear when you return to play. Objective and player status remain visible when help text is hidden."), 16, FLinearColor::White);
        Label(TEXT("Use the scoreboard binding in gameplay to view team statistics. The match continues while viewing it."), 16, FLinearColor::White);
        Label(TEXT("MOVEMENT"), 20, FLinearColor(0.35f, 0.85f, 0.8f));
        const UInputSettings* Input = GetDefault<UInputSettings>();
        struct FControlHint { const TCHAR* Mapping; const TCHAR* Label; };
        Label(TEXT("Choose four movement keys, then Apply to replace keyboard movement bindings. Presets fill the draft only. Closing this panel discards unapplied movement changes."), 16, FLinearColor::White);
        MovementSummary = Label(TEXT("Active keyboard layout"), 16, FLinearColor::White);
        MovementSelectors.Reset();
        for (const TCHAR* Direction : {TEXT("Forward"), TEXT("Backward"), TEXT("Strafe left"), TEXT("Strafe right")})
        {
            Label(Direction, 16, FLinearColor::White);
            auto* Selector = WidgetTree->ConstructWidget<UInputKeySelector>();
            Selector->SetAllowGamepadKeys(false);
            Selector->SetAllowModifierKeys(false);
            Selector->SetEscapeKeys(TArray<FKey>{EKeys::Escape});
            Column->AddChildToVerticalBox(Selector)->SetPadding(FMargin(0, 4));
            MovementSelectors.Add(Selector);
        }
        Button(TEXT("Fill WASD preset"))->OnClicked.AddUniqueDynamic(this, &USPControlsWidget::UseWASD);
        Button(TEXT("Fill arrow-key preset"))->OnClicked.AddUniqueDynamic(this, &USPControlsWidget::UseArrowKeys);
        Button(TEXT("Apply movement keys"))->OnClicked.AddUniqueDynamic(this, &USPControlsWidget::ApplyMovementKeys);
        Button(TEXT("Discard movement draft"))->OnClicked.AddUniqueDynamic(this, &USPControlsWidget::RefreshMovementKeys);
        MovementFeedback = Label(TEXT("Each direction needs a different keyboard key. Applying replaces existing keyboard movement bindings."), 16, FLinearColor::White);
        for (const FControlHint& Hint : {FControlHint{TEXT("Turn"), TEXT("Look horizontally")},
            FControlHint{TEXT("LookUp"), TEXT("Look vertically")}})
        {
            FString Keys;
            for (const auto& Mapping : Input->GetAxisMappings())
                if (Mapping.AxisName == FName(Hint.Mapping))
                    Keys += (Keys.IsEmpty() ? TEXT("") : TEXT(" / ")) + Mapping.Key.GetDisplayName().ToString();
            Label(FString::Printf(TEXT("%s   %s"), Hint.Label, *Keys), 16, FLinearColor::White);
        }
        Label(TEXT("COMBAT & TACTICS"), 20, FLinearColor(0.35f, 0.85f, 0.8f));
        Label(TEXT("Choose an action, then click its key to rebind. Esc cancels key capture. Menu shortcuts and mouse-look axes stay fixed."), 16, FLinearColor::White);
        BindingAction = WidgetTree->ConstructWidget<UComboBoxString>();
        BindingAction->OnSelectionChanged.AddUniqueDynamic(this, &USPControlsWidget::ChooseBindingAction);
        Column->AddChildToVerticalBox(BindingAction)->SetPadding(FMargin(0, 8));
        BindingKey = WidgetTree->ConstructWidget<UInputKeySelector>();
        BindingKey->SetAllowGamepadKeys(false);
        BindingKey->SetAllowModifierKeys(false);
        BindingKey->SetEscapeKeys(TArray<FKey>{EKeys::Escape});
        BindingKey->OnKeySelected.AddUniqueDynamic(this, &USPControlsWidget::CaptureBinding);
        Column->AddChildToVerticalBox(BindingKey)->SetPadding(FMargin(0, 8));
        BindingFeedback = Label(TEXT("Single keys only. Conflicts can be reviewed and swapped safely."), 16, FLinearColor::White);
        ConfirmSwapButton = Button(TEXT("Confirm key swap"));
        ConfirmSwapButton->SetIsEnabled(false);
        ConfirmSwapText = Cast<UTextBlock>(ConfirmSwapButton->GetContent());
        ConfirmSwapButton->OnClicked.AddUniqueDynamic(this, &USPControlsWidget::ConfirmPendingSwap);
        auto* Restore = Button(TEXT("Restore all original key bindings"));
        Restore->OnClicked.AddUniqueDynamic(this, &USPControlsWidget::ResetBindings);
        // Only list implemented player controls. Config-only actions are not advertised.
        for (const FControlHint& Hint : {
            FControlHint{TEXT("Fire"), TEXT("Fire (single press)")}, FControlHint{TEXT("Aim"), TEXT("Aim (hold or toggle)")},
            FControlHint{TEXT("Reload"), TEXT("Reload")}, FControlHint{TEXT("Crouch"), TEXT("Crouch / silent movement (hold)")},
            FControlHint{TEXT("Sprint"), TEXT("Sprint (hold)")}, FControlHint{TEXT("LeanLeft"), TEXT("Lean left (hold)")},
            FControlHint{TEXT("LeanRight"), TEXT("Lean right (hold)")}, FControlHint{TEXT("Vault"), TEXT("Vault low obstacle")},
            FControlHint{TEXT("CycleEquipment"), TEXT("Select equipment")}, FControlHint{TEXT("ThrowEquipment"), TEXT("Throw equipment")},
            FControlHint{TEXT("CycleOptic"), TEXT("Switch optic")}, FControlHint{TEXT("SquadOrder"), TEXT("Cycle squad order")},
            FControlHint{TEXT("Fortify"), TEXT("Fortify marked point")},
            FControlHint{TEXT("Scoreboard"), TEXT("Scoreboard")}})
        {
            FString Keys;
            for (const auto& Mapping : Input->GetActionMappings())
                if (Mapping.ActionName == FName(Hint.Mapping))
                {
                    FString Key = Mapping.bCtrl ? TEXT("Ctrl+") : TEXT("");
                    if (Mapping.bAlt) Key += TEXT("Alt+");
                    if (Mapping.bShift) Key += TEXT("Shift+");
                    if (Mapping.bCmd) Key += TEXT("Cmd+");
                    Key += Mapping.Key.GetDisplayName().ToString();
                    Keys += (Keys.IsEmpty() ? TEXT("") : TEXT(" / ")) + Key;
                }
            ActionNames.Add(Hint.Label, FName(Hint.Mapping));
            BindingAction->AddOption(Hint.Label);
            BindingLabels.Add(FName(Hint.Mapping), Label(FString::Printf(TEXT("%s   %s"), Hint.Label, Keys.IsEmpty() ? TEXT("Unbound") : *Keys), 16, FLinearColor::White));
        }
        Label(TEXT("Crouch, sprint and lean are hold controls. Aim uses your hold/toggle preference. Release and press again after closing a menu."), 16, FLinearColor::White);
        BindingAction->SetSelectedIndex(0);
        WidgetTree->RootWidget = Backdrop;
    }
    return Super::RebuildWidget();
}

void USPControlsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    const auto* HUDSettings = GetDefault<USPControlSettings>();
    AimSensitivitySlider->SetValue(HUDSettings->GetSafeAimSensitivityMultiplier());
    SetAimSensitivity(HUDSettings->GetSafeAimSensitivityMultiplier());
    ToggleAimCheck->SetIsChecked(HUDSettings->bToggleAim);
    ContrastCheck->SetIsChecked(HUDSettings->bHighContrastHUD);
    CrosshairCheck->SetIsChecked(HUDSettings->bShowCrosshair);
    HintsCheck->SetIsChecked(HUDSettings->bShowHUDHints);
    ScoreboardToggleCheck->SetIsChecked(HUDSettings->bToggleScoreboard);
    CrosshairSlider->SetValue(HUDSettings->GetSafeCrosshairScale());
    SetCrosshairScale(HUDSettings->GetSafeCrosshairScale());
    CrosshairSlider->SetIsEnabled(HUDSettings->bShowCrosshair);
    if (auto* PC = GetOwningPlayer<ASPObserverPlayerController>())
    {
        SensitivitySlider->SetValue(PC->GetMouseSensitivity());
        InvertCheck->SetIsChecked(PC->IsMouseYInverted());
        SetSensitivity(PC->GetMouseSensitivity());
        FString BindingError;
        if (!GetMutableDefault<USPControlSettings>()->ApplyActionOverrides(BindingError))
            BindingFeedback->SetText(FText::FromString(TEXT("Saved bindings could not be applied. Restore original bindings to recover. ") + BindingError));
        RefreshBindings();
        RefreshMovementKeys();
    }
}

void USPControlsWidget::SetSensitivity(float Value)
{
    if (auto* PC = GetOwningPlayer<ASPObserverPlayerController>()) PC->SetMouseSensitivity(Value);
    if (SensitivityLabel) SensitivityLabel->SetText(FText::FromString(FString::Printf(TEXT("Mouse sensitivity   %.2fx"), Value)));
}
void USPControlsWidget::SetInvert(bool bChecked)
{
    if (auto* PC = GetOwningPlayer<ASPObserverPlayerController>()) PC->SetMouseYInverted(bChecked);
}
void USPControlsWidget::ResetDefaults()
{
    SensitivitySlider->SetValue(1.f); InvertCheck->SetIsChecked(false);
    SetSensitivity(1.f); SetInvert(false);
    ToggleAimCheck->SetIsChecked(false);
    SetToggleAim(false);
    AimSensitivitySlider->SetValue(1.f);
    SetAimSensitivity(1.f);
}
void USPControlsWidget::CloseControls()
{
    ClearPendingSwap();
    if (auto* PC = GetOwningPlayer<ASPObserverPlayerController>()) PC->ToggleControls();
}
FReply USPControlsWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape && !Event.IsRepeat())
    {
        if (BindingKey && BindingKey->GetIsSelectingKey())
            return Super::NativeOnPreviewKeyDown(Geometry, Event); // let the selector cancel capture
        for (const auto& Selector : MovementSelectors)
            if (Selector && Selector->GetIsSelectingKey())
                return Super::NativeOnPreviewKeyDown(Geometry, Event);
        CloseControls();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void USPControlsWidget::ChooseBindingAction(FString Selection, ESelectInfo::Type SelectionType)
{
    ClearPendingSwap();
    RefreshBindings();
}

void USPControlsWidget::RefreshBindings()
{
    if (!BindingAction || !BindingKey) return;
    bSynchronizingBinding = true;
    const auto* Input = GetDefault<UInputSettings>();
    const FName* Selected = ActionNames.Find(BindingAction->GetSelectedOption());
    FInputChord Current;
    for (const auto& Pair : ActionNames)
    {
        FString Keys;
        for (const auto& Mapping : Input->GetActionMappings())
            if (Mapping.ActionName == Pair.Value)
            {
                FString Key = Mapping.bCtrl ? TEXT("Ctrl+") : TEXT("");
                if (Mapping.bAlt) Key += TEXT("Alt+");
                if (Mapping.bShift) Key += TEXT("Shift+");
                if (Mapping.bCmd) Key += TEXT("Cmd+");
                Key += Mapping.Key.GetDisplayName().ToString();
                Keys += (Keys.IsEmpty() ? TEXT("") : TEXT(" / ")) + Key;
                if (Selected && Mapping.ActionName == *Selected && !Mapping.Key.IsGamepadKey())
                    Current = FInputChord(Mapping.Key, Mapping.bShift, Mapping.bCtrl, Mapping.bAlt, Mapping.bCmd);
            }
        if (auto* Label = BindingLabels.Find(Pair.Value))
            (*Label)->SetText(FText::FromString(Pair.Key + TEXT("   ") + (Keys.IsEmpty() ? TEXT("Unbound") : Keys)));
    }
    BindingKey->SetIsEnabled(Selected != nullptr);
    BindingKey->SetSelectedKey(Current);
    bSynchronizingBinding = false;
}

void USPControlsWidget::CaptureBinding(FInputChord Chord)
{
    if (bSynchronizingBinding) return;
    ClearPendingSwap();

    const FName* Action = ActionNames.Find(BindingAction->GetSelectedOption());
    FString Error;
    if (!Action || Chord.bShift || Chord.bCtrl || Chord.bAlt || Chord.bCmd)
    {
        Error = TEXT("Choose a single key without a modifier combination.");
    }
    else
    {
        if (auto* PC = GetOwningPlayer<ASPObserverPlayerController>())
            if (auto* Character = Cast<ASPCharacter>(PC->GetPawn())) Character->ReleaseHeldControls();

        auto* Settings = GetMutableDefault<USPControlSettings>();
        FName ConflictAction;
        if (Settings->FindActionUsingKey(Chord.Key, *Action, ConflictAction))
        {
            PendingSwapAction = *Action;
            PendingConflictAction = ConflictAction;
            PendingSwapKey = Chord.Key;
            if (ConfirmSwapButton) ConfirmSwapButton->SetIsEnabled(true);
            if (ConfirmSwapText)
            {
                ConfirmSwapText->SetText(FText::FromString(FString::Printf(
                    TEXT("Swap %s with %s"),
                    *PendingSwapAction.ToString(),
                    *PendingConflictAction.ToString())));
            }
            Error = FString::Printf(
                TEXT("%s is assigned to %s. Confirm the swap to exchange their current keys without leaving either action unbound."),
                *Chord.Key.GetDisplayName().ToString(),
                *ConflictAction.ToString());
        }
        else
        {
            Settings->RebindAction(*Action, Chord.Key, Error);
        }
    }

    BindingFeedback->SetText(FText::FromString(Error.IsEmpty() ? TEXT("Binding applied and saved.") : Error));
    RefreshBindings();
}

void USPControlsWidget::ConfirmPendingSwap()
{
    if (PendingSwapAction.IsNone() || PendingConflictAction.IsNone() || !PendingSwapKey.IsValid())
    {
        ClearPendingSwap();
        return;
    }

    if (auto* PC = GetOwningPlayer<ASPObserverPlayerController>())
        if (auto* Character = Cast<ASPCharacter>(PC->GetPawn())) Character->ReleaseHeldControls();

    FString Error;
    const FString SwapSummary = FString::Printf(
        TEXT("%s and %s swapped and saved."),
        *PendingSwapAction.ToString(),
        *PendingConflictAction.ToString());

    const bool bSwapped = GetMutableDefault<USPControlSettings>()->SwapActionBinding(
        PendingSwapAction,
        PendingConflictAction,
        PendingSwapKey,
        Error);

    ClearPendingSwap();
    BindingFeedback->SetText(FText::FromString(bSwapped ? SwapSummary : Error));
    RefreshBindings();
}

void USPControlsWidget::ClearPendingSwap()
{
    PendingSwapAction = NAME_None;
    PendingConflictAction = NAME_None;
    PendingSwapKey = FKey();
    if (ConfirmSwapButton) ConfirmSwapButton->SetIsEnabled(false);
    if (ConfirmSwapText) ConfirmSwapText->SetText(FText::FromString(TEXT("Confirm key swap")));
}

void USPControlsWidget::ResetBindings()
{
    ClearPendingSwap();
    GetMutableDefault<USPControlSettings>()->ResetAllBindings();
    BindingFeedback->SetText(FText::FromString(TEXT("Original movement and action keys restored. Mouse and HUD preferences unchanged.")));
    RefreshBindings();
    RefreshMovementKeys();
}

void USPControlsWidget::SetHUDContrast(bool bEnabled)
{
    GetMutableDefault<USPControlSettings>()->bHighContrastHUD = bEnabled;
}
void USPControlsWidget::SetCrosshairVisible(bool bEnabled)
{
    GetMutableDefault<USPControlSettings>()->bShowCrosshair = bEnabled;
    if (CrosshairSlider) CrosshairSlider->SetIsEnabled(bEnabled);
}
void USPControlsWidget::SetHUDHints(bool bEnabled)
{
    GetMutableDefault<USPControlSettings>()->bShowHUDHints = bEnabled;
}
void USPControlsWidget::SetCrosshairScale(float Value)
{
    auto* Settings = GetMutableDefault<USPControlSettings>();
    Settings->CrosshairScale = Value;
    Settings->CrosshairScale = Settings->GetSafeCrosshairScale();
    if (CrosshairLabel) CrosshairLabel->SetText(FText::FromString(FString::Printf(TEXT("Crosshair size   %.2fx"), Settings->CrosshairScale)));
}
void USPControlsWidget::ResetHUDPreferences()
{
    ContrastCheck->SetIsChecked(false); CrosshairCheck->SetIsChecked(true); HintsCheck->SetIsChecked(true);
    ScoreboardToggleCheck->SetIsChecked(false);
    SetScoreboardToggle(false);
    CrosshairSlider->SetValue(1.f);
    SetHUDContrast(false); SetCrosshairVisible(true); SetHUDHints(true); SetCrosshairScale(1.f);
}

void USPControlsWidget::SetScoreboardToggle(bool bEnabled)
{
    GetMutableDefault<USPControlSettings>()->bToggleScoreboard = bEnabled;
}

void USPControlsWidget::SetAimSensitivity(float Value)
{
    auto* Settings = GetMutableDefault<USPControlSettings>();
    Settings->AimSensitivityMultiplier = Value;
    Settings->AimSensitivityMultiplier = Settings->GetSafeAimSensitivityMultiplier();
    if (AimSensitivityLabel) AimSensitivityLabel->SetText(FText::FromString(FString::Printf(
        TEXT("Aim sensitivity   %.2fx normal"), Settings->AimSensitivityMultiplier)));
}

void USPControlsWidget::SetToggleAim(bool bEnabled)
{
    GetMutableDefault<USPControlSettings>()->bToggleAim = bEnabled;
}

void USPControlsWidget::StageMovementKeys(const TArray<FKey>& Keys)
{
    if (Keys.Num() != MovementSelectors.Num()) return;
    for (int32 Index = 0; Index < Keys.Num(); ++Index)
        MovementSelectors[Index]->SetSelectedKey(FInputChord(Keys[Index]));
    MovementFeedback->SetText(FText::FromString(TEXT("Preset staged. Apply movement keys to save it.")));
}

void USPControlsWidget::UseWASD()
{
    StageMovementKeys({EKeys::W, EKeys::S, EKeys::A, EKeys::D});
}
void USPControlsWidget::UseArrowKeys()
{
    StageMovementKeys({EKeys::Up, EKeys::Down, EKeys::Left, EKeys::Right});
}

void USPControlsWidget::RefreshMovementKeys()
{
    const FName Axes[] = {TEXT("MoveForward"), TEXT("MoveForward"), TEXT("MoveRight"), TEXT("MoveRight")};
    const float Scales[] = {1.f, -1.f, -1.f, 1.f};
    const TCHAR* Names[] = {TEXT("Forward"), TEXT("Backward"), TEXT("Left"), TEXT("Right")};
    FString Summary = TEXT("Active keyboard layout: ");
    for (int32 Index = 0; Index < MovementSelectors.Num(); ++Index)
    {
        FKey FirstKey;
        FString Keys;
        for (const auto& Mapping : GetDefault<UInputSettings>()->GetAxisMappings())
        {
            if (Mapping.AxisName != Axes[Index] || Mapping.Scale != Scales[Index]
                || Mapping.Key.IsGamepadKey() || Mapping.Key.IsAnalog() || Mapping.Key.IsMouseButton()
                || Mapping.Key.IsTouch() || Mapping.Key.IsGesture()) continue;
            if (!FirstKey.IsValid()) FirstKey = Mapping.Key;
            if (!Keys.IsEmpty()) Keys += TEXT(" / ");
            Keys += Mapping.Key.GetDisplayName().ToString();
        }
        MovementSelectors[Index]->SetSelectedKey(FInputChord(FirstKey));
        if (Index > 0) Summary += TEXT(" | ");
        Summary += FString(Names[Index]) + TEXT(" ") + (Keys.IsEmpty() ? TEXT("Unbound") : Keys);
    }
    MovementSummary->SetText(FText::FromString(Summary));
    MovementFeedback->SetText(FText::FromString(TEXT("Showing current bindings. Edits take effect only after Apply.")));
}

void USPControlsWidget::ApplyMovementKeys()
{
    TArray<FKey> Keys;
    for (const auto& Selector : MovementSelectors)
    {
        const FInputChord Chord = Selector->GetSelectedKey();
        if (Chord.bShift || Chord.bCtrl || Chord.bAlt || Chord.bCmd)
        {
            MovementFeedback->SetText(FText::FromString(TEXT("Choose single keyboard keys without modifier combinations.")));
            return;
        }
        Keys.Add(Chord.Key);
    }
    ClearPendingSwap();
    if (auto* PC = GetOwningPlayer<ASPObserverPlayerController>())
        if (auto* Character = Cast<ASPCharacter>(PC->GetPawn())) Character->ReleaseHeldControls();
    FString Error;
    if (GetMutableDefault<USPControlSettings>()->SetMovementKeys(Keys, Error))
    {
        RefreshMovementKeys();
        RefreshBindings();
        MovementFeedback->SetText(FText::FromString(TEXT("Movement keys applied and saved.")));
    }
    else MovementFeedback->SetText(FText::FromString(Error));
}
