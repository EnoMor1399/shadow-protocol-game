#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPDeploymentDirector.generated.h"

UENUM(BlueprintType)
enum class ESPDeploymentPhase : uint8
{
    Idle,
    Authentication,
    LoadoutCheck,
    Insertion,
    Live
};

UCLASS()
class SHADOWPROTOCOL_API ASPDeploymentDirector : public AActor
{
    GENERATED_BODY()

public:
    ASPDeploymentDirector();

    UPROPERTY(ReplicatedUsing=OnRep_DeploymentPhase, BlueprintReadOnly, Category="Deployment")
    ESPDeploymentPhase DeploymentPhase = ESPDeploymentPhase::Idle;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Deployment")
    FName SpawnGroup = TEXT("ALPHA");

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Deployment")
    FName OperationId = TEXT("FIRST-CONTACT");

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Deployment")
    void ServerBeginDeployment(FName RequestedSpawnGroup);

    UFUNCTION(BlueprintCallable, Category="Deployment")
    void AdvanceDeploymentAuthority();

    UFUNCTION(BlueprintImplementableEvent, Category="Deployment")
    void BP_OnDeploymentPhaseChanged(ESPDeploymentPhase NewPhase);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION()
    void OnRep_DeploymentPhase();
};
