#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPObjectiveSiteActor.generated.h"

UCLASS()
class SHADOWPROTOCOL_API ASPObjectiveSiteActor : public AActor
{
    GENERATED_BODY()

public:
    ASPObjectiveSiteActor();

    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category="Objective")
    FName SiteId = TEXT("ARCHIVE-A");

    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category="Objective")
    FText DisplayName;

    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category="Objective")
    FName EmbassyZone = TEXT("ARCHIVE CORE");

    UPROPERTY(ReplicatedUsing=OnRep_Active, BlueprintReadOnly, Category="Objective")
    bool bActiveSite = false;

    UFUNCTION(BlueprintCallable, Category="Objective")
    void SetActiveSiteAuthority(bool bActive);

    UFUNCTION(BlueprintImplementableEvent, Category="Objective")
    void BP_OnObjectiveSiteStateChanged(bool bNowActive);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION()
    void OnRep_Active();
};
