#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "SPTypes.h"
#include "SPProtocolGameState.generated.h"

UCLASS()
class SHADOWPROTOCOL_API ASPProtocolGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated, BlueprintReadOnly) ESPMatchPhase MatchPhase = ESPMatchPhase::Planning;
    UPROPERTY(Replicated, BlueprintReadOnly) ESPRoundState RoundState = ESPRoundState::Waiting;
    UPROPERTY(Replicated, BlueprintReadOnly) float RoundTimeRemaining = 0.f;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 RoundNumber = 1;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 DirectorateRoundWins = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 HelixRoundWins = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) ESPTeam AttackingTeam = ESPTeam::DirectorateNine;
    UPROPERTY(Replicated, BlueprintReadOnly) ESPTeam DefendingTeam = ESPTeam::Helix;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 SideRotationCount = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bMatchComplete = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bTrueObjectiveRevealed = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bObjectiveSecured = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bAlternateExtractionUnlocked = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bCamerasDisabled = false;
    UPROPERTY(Replicated, BlueprintReadOnly) FName ActiveObjectiveSiteId = NAME_None;
    UPROPERTY(Replicated, BlueprintReadOnly) ESPTeam RoundWinner = ESPTeam::None;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 ExpectedPlayerCount = 10;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 ReadyPlayerCount = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bAllPlayersReady = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bOvertimeActive = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bOvertimeUsedThisRound = false;
    UPROPERTY(Replicated, BlueprintReadOnly) ESPTeam MatchPointTeam = ESPTeam::None;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bDecidingRound = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bTeamOnlySpectating = true;
    UPROPERTY(Replicated, BlueprintReadOnly) FString ServerInstanceId = TEXT("UNASSIGNED");
    UPROPERTY(Replicated, BlueprintReadOnly) int32 ServerTickRate = 60;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bRankedRules = true;
    UPROPERTY(Replicated, BlueprintReadOnly) TArray<FSPCompetitivePlayerSlot> PlayerSlots;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
