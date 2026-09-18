#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ButtonText;
    UPROPERTY(Transient) TObjectPtr<UButton> ReadyButton;
    UFUNCTION() void ToggleReady();
};
