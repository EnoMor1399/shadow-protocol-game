#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "Framework/Commands/InputChord.h"
#include "SPControlsWidget.generated.h"
class UTextBlock;
class USlider;
class UCheckBox;
class UButton;
class UInputKeySelector;

UCLASS()
class SHADOWPROTOCOL_API USPControlsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SensitivityLabel;
    UPROPERTY(Transient) TObjectPtr<USlider> SensitivitySlider;
    UPROPERTY(Transient) TObjectPtr<UCheckBox> InvertCheck;
    UPROPERTY(Transient) TObjectPtr<UButton> ResumeButton;
    UPROPERTY(Transient) TObjectPtr<UCheckBox> ContrastCheck;
    UPROPERTY(Transient) TObjectPtr<UCheckBox> CrosshairCheck;
    UPROPERTY(Transient) TObjectPtr<UCheckBox> HintsCheck;
    UPROPERTY(Transient) TObjectPtr<UCheckBox> ScoreboardToggleCheck;
    UFUNCTION() void SetScoreboardToggle(bool bEnabled);
    UPROPERTY(Transient) TObjectPtr<USlider> CrosshairSlider;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CrosshairLabel;
    UFUNCTION() void SetHUDContrast(bool bEnabled);
    UFUNCTION() void SetCrosshairVisible(bool bEnabled);
    UFUNCTION() void SetHUDHints(bool bEnabled);
    UFUNCTION() void SetCrosshairScale(float Value);
    UFUNCTION() void ResetHUDPreferences();
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> BindingAction;
    UPROPERTY(Transient) TObjectPtr<UInputKeySelector> BindingKey;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> BindingFeedback;
    UPROPERTY(Transient) TObjectPtr<UButton> ConfirmSwapButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ConfirmSwapText;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UTextBlock>> BindingLabels;
    TMap<FString, FName> ActionNames;
    FName PendingSwapAction = NAME_None;
    FName PendingConflictAction = NAME_None;
    FKey PendingSwapKey;
    bool bSynchronizingBinding = false;
    void RefreshBindings();
    void ClearPendingSwap();
    UFUNCTION() void ChooseBindingAction(FString Selection, ESelectInfo::Type SelectionType);
    UFUNCTION() void CaptureBinding(FInputChord Chord);
    UFUNCTION() void ConfirmPendingSwap();
    UFUNCTION() void ResetBindings();
    UFUNCTION() void SetSensitivity(float Value);
    UFUNCTION() void SetInvert(bool bChecked);
    UFUNCTION() void ResetDefaults();
    UFUNCTION() void CloseControls();
};
