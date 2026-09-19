#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SPControlsWidget.generated.h"
class UTextBlock;
class USlider;
class UCheckBox;
class UButton;

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
    UFUNCTION() void SetSensitivity(float Value);
    UFUNCTION() void SetInvert(bool bChecked);
    UFUNCTION() void ResetDefaults();
    UFUNCTION() void CloseControls();
};
