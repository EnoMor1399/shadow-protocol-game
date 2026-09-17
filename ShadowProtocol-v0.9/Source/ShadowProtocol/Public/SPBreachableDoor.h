#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPBreachableDoor.generated.h"

UCLASS()
class SHADOWPROTOCOL_API ASPBreachableDoor : public AActor
{
    GENERATED_BODY()
public:
    ASPBreachableDoor();
    UPROPERTY(EditAnywhere,BlueprintReadOnly) float BreachResistance=100.f;
    UPROPERTY(ReplicatedUsing=OnRep_Breached,BlueprintReadOnly) bool bBreached=false;
    UFUNCTION(Server,Reliable,BlueprintCallable) void ServerApplyBreach(float Force);
    UFUNCTION(BlueprintImplementableEvent) void BP_OnBreached();
protected:
    UFUNCTION() void OnRep_Breached();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
