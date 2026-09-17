#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPTypes.h"
#include "SPTacticalPlanningComponent.generated.h"

UCLASS(ClassGroup=(ShadowProtocol),meta=(BlueprintSpawnableComponent))
class SHADOWPROTOCOL_API USPTacticalPlanningComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USPTacticalPlanningComponent();
    UPROPERTY(Replicated,BlueprintReadOnly) TArray<FSPTacticalMarker> Markers;
    UFUNCTION(Server,Reliable,BlueprintCallable) void ServerAddMarker(const FSPTacticalMarker& Marker);
    UFUNCTION(Server,Reliable,BlueprintCallable) void ServerClearOwnedMarkers(int32 OwnerPlayerId);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
