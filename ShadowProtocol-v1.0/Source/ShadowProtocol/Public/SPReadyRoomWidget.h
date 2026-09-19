#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "SPReadyRoomWidget.generated.h"

class UTextBlock;
class UButton;

/** Native panel displaying replicated state and submitting owner-only requests. */
UCLASS()
class SHADOWPROTOCOL_API USPReadyRoomWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void RefreshReadyRoom();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RosterText;
    UFUNCTION() void OpenControls();
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> SpawnChoice;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> FeedbackText;
    TArray<FName> DisplayedSpawnGroups;
    bool bSynchronizingSpawnChoice = false;
    UFUNCTION() void SelectSpawn(FString Selection, ESelectInfo::Type SelectionType);
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ButtonText;
    UPROPERTY(Transient) TObjectPtr<UButton> ReadyButton;
    UFUNCTION() void ToggleReady();
};
