#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPTypes.h"
#include "SPInteractiveDoor.generated.h"

class UStaticMeshComponent;

UCLASS()
class SHADOWPROTOCOL_API ASPInteractiveDoor : public AActor
{
    GENERATED_BODY()
public:
    ASPInteractiveDoor();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> DoorMesh;
    UPROPERTY(ReplicatedUsing=OnRep_DoorState, BlueprintReadOnly) ESPDoorState DoorState = ESPDoorState::Closed;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float PeekAngleDegrees = 28.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float OpenAngleDegrees = 92.f;
    UFUNCTION(Server, Reliable) void ServerCycleDoor(AController* RequestingController);
    UFUNCTION(Server, Reliable) void ServerBreachDoor(AController* RequestingController);
    UFUNCTION() void OnRep_DoorState();
    UFUNCTION(BlueprintImplementableEvent, Category="Door") void BP_ApplyDoorState(ESPDoorState NewState);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
