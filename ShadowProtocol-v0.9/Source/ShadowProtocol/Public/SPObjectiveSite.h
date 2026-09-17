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
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SiteId = "ARCHIVE_A";
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bActiveThisRound = false;
    void SetActiveSite(bool bActive);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
