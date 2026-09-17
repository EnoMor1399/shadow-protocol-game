#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPTypes.h"
#include "SPIntelligenceNode.generated.h"

class ASPPlayerState;

UCLASS()
class SHADOWPROTOCOL_API ASPIntelligenceNode : public AActor
{
    GENERATED_BODY()
public:
    ASPIntelligenceNode();
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ESPIntelType IntelType = ESPIntelType::CommunicationsTerminal;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName EffectTag = "RevealObjective";
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float InteractionSeconds = 4.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bRepeatable = false;
    UPROPERTY(ReplicatedUsing=OnRep_Captured, BlueprintReadOnly) bool bCaptured = false;

    UFUNCTION(Server, Reliable, BlueprintCallable) void ServerCapture(ASPPlayerState* CapturingPlayer);
    UFUNCTION(BlueprintImplementableEvent) void BP_OnIntelCaptured(FName AppliedEffect);
protected:
    UFUNCTION() void OnRep_Captured();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
