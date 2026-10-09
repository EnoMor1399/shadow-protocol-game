#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "SPTypes.h"
#include "SPPlayerState.generated.h"

UCLASS()
class SHADOWPROTOCOL_API ASPPlayerState : public APlayerState
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated, BlueprintReadOnly) ESPTeam Team = ESPTeam::None;
    UPROPERTY(Replicated, BlueprintReadOnly) ESPTacticalRole TacticalRole = ESPTacticalRole::Breacher;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bSquadLeader = false;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 TacticalScore = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 IntelRecovered = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 ObjectiveActions = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bReady = false;
    UPROPERTY(Replicated, BlueprintReadOnly) FName SelectedSpawnGroup = "ALPHA";
    UPROPERTY(Replicated, BlueprintReadOnly) ESPConnectionState ConnectionState = ESPConnectionState::Connected;
    UPROPERTY(Replicated, BlueprintReadOnly) FString AuthenticatedSessionId;
    UPROPERTY(Replicated, BlueprintReadOnly) FString AuthenticatedUserId;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 CompetitiveSlotIndex = INDEX_NONE;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bSessionAuthenticated = false;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 Eliminations = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 Deaths = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 Assists = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 Headshots = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) float TeamDamage = 0.f;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 FriendlyFireIncidents = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) ESPFriendlyFireState FriendlyFireState = ESPFriendlyFireState::Normal;

    UFUNCTION(BlueprintCallable) void AddScoreEvent(ESPScoreEvent Event, int32 OverrideValue = 0);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
