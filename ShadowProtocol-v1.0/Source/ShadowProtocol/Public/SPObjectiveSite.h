#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPObjectiveSite.generated.h"

UCLASS()
class SHADOWPROTOCOL_API ASPObjectiveSite : public AActor
{
    GENERATED_BODY()

public:
    ASPObjectiveSite();

    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category="Objective")
    FName SiteId = TEXT("ARCHIVE-A");

    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category="Objective")
    FText DisplayName;

    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category="Objective")
    FName EmbassyZone = TEXT("ARCHIVE CORE");

    UPROPERTY(ReplicatedUsing=OnRep_ActiveSite, BlueprintReadOnly, Category="Objective")
    bool bActiveThisRound = false;

    UFUNCTION(BlueprintCallable, Category="Objective")
    void SetActiveSite(bool bActive);

    UFUNCTION(BlueprintImplementableEvent, Category="Objective")
    void BP_OnObjectiveSiteStateChanged(bool bNowActive);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION()
    void OnRep_ActiveSite();
};
