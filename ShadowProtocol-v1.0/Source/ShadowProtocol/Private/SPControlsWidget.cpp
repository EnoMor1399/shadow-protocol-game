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
        InvertCheck = WidgetTree->ConstructWidget<UCheckBox>();
        auto* InvertLabel = WidgetTree->ConstructWidget<UTextBlock>();
        InvertLabel->SetText(FText::FromString(TEXT("Invert vertical mouse look")));
        InvertCheck->SetContent(InvertLabel);
        InvertCheck->OnCheckStateChanged.AddUniqueDynamic(this, &USPControlsWidget::SetInvert);
        Column->AddChildToVerticalBox(InvertCheck)->SetPadding(FMargin(0, 12));
        auto* Reset = Button(TEXT("Reset mouse settings"));
        Reset->OnClicked.AddUniqueDynamic(this, &USPControlsWidget::ResetDefaults);
        Label(TEXT("MOVEMENT"), 20, FLinearColor(0.35f, 0.85f, 0.8f));
        const UInputSettings* Input = GetDefault<UInputSettings>();
        struct FControlHint { const TCHAR* Mapping; const TCHAR* Label; };
        for (const FControlHint& Hint : {FControlHint{TEXT("MoveForward"), TEXT("Forward / backward")},
            FControlHint{TEXT("MoveRight"), TEXT("Strafe right / left")}, FControlHint{TEXT("Turn"), TEXT("Look horizontally")},
            FControlHint{TEXT("LookUp"), TEXT("Look vertically")}})
        {
            FString Keys;
            for (const auto& Mapping : Input->GetAxisMappings())
                if (Mapping.AxisName == FName(Hint.Mapping))
                    Keys += (Keys.IsEmpty() ? TEXT("") : TEXT(" / ")) + Mapping.Key.GetDisplayName().ToString();
            Label(FString::Printf(TEXT("%s   %s"), Hint.Label, *Keys), 16, FLinearColor::White);
        }
        Label(TEXT("COMBAT & TACTICS"), 20, FLinearColor(0.35f, 0.85f, 0.8f));
        // Only list implemented pawn controls. Config-only actions are not advertised.
        for (const FControlHint& Hint : {
            FControlHint{TEXT("Fire"), TEXT("Fire (single press)")}, FControlHint{TEXT("Aim"), TEXT("Aim (hold)")},
            FControlHint{TEXT("Reload"), TEXT("Reload")}, FControlHint{TEXT("Crouch"), TEXT("Crouch / silent movement (hold)")},
            FControlHint{TEXT("Sprint"), TEXT("Sprint (hold)")}, FControlHint{TEXT("LeanLeft"), TEXT("Lean left (hold)")},
            FControlHint{TEXT("LeanRight"), TEXT("Lean right (hold)")}, FControlHint{TEXT("Vault"), TEXT("Vault low obstacle")},
            FControlHint{TEXT("CycleEquipment"), TEXT("Select equipment")}, FControlHint{TEXT("ThrowEquipment"), TEXT("Throw equipment")},
            FControlHint{TEXT("CycleOptic"), TEXT("Switch optic")}, FControlHint{TEXT("SquadOrder"), TEXT("Cycle squad order")},
            FControlHint{TEXT("Fortify"), TEXT("Fortify marked point")}})
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
            Label(FString::Printf(TEXT("%s   %s"), Hint.Label, Keys.IsEmpty() ? TEXT("Unbound") : *Keys), 16, FLinearColor::White);
        }
        Label(TEXT("Aim, crouch, sprint and lean are hold controls. Release and press again after closing a menu."), 16, FLinearColor::White);
        WidgetTree->RootWidget = Backdrop;
    }
    return Super::RebuildWidget();
}

void USPControlsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (auto* PC = GetOwningPlayer<ASPObserverPlayerController>())
    {
        SensitivitySlider->SetValue(PC->GetMouseSensitivity());
        InvertCheck->SetIsChecked(PC->IsMouseYInverted());
        SetSensitivity(PC->GetMouseSensitivity());
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
}
void USPControlsWidget::CloseControls()
{
    if (auto* PC = GetOwningPlayer<ASPObserverPlayerController>()) PC->ToggleControls();
}
FReply USPControlsWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape && !Event.IsRepeat())
    {
        CloseControls();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
