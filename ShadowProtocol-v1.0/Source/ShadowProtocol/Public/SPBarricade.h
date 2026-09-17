#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPTypes.h"
#include "SPBarricade.generated.h"

UCLASS()
class SHADOWPROTOCOL_API ASPBarricade : public AActor
{
    GENERATED_BODY()
public:
    ASPBarricade();
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Fortification") float Health = 100.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fortification") float MaxHealth = 100.f;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Fortification") ESPTeam OwningTeam = ESPTeam::None;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Fortification") bool bDeployed = false;
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Fortification") void ServerDeploy(ESPTeam Team);
    UFUNCTION(BlueprintCallable, Category="Fortification") void ApplyTacticalDamage(float Amount);
    UFUNCTION(BlueprintImplementableEvent, Category="Fortification") void BP_OnBarricadeBroken();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
