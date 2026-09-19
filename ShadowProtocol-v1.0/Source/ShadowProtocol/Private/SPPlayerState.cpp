#include "SPPlayerState.h"
#include "Net/UnrealNetwork.h"

void ASPPlayerState::AddScoreEvent(ESPScoreEvent Event, int32 OverrideValue)
{
    if (!HasAuthority()) return;
    int32 Value = OverrideValue;
    if (Value == 0)
    {
        switch (Event)
        {
            case ESPScoreEvent::Elimination: Value = 100; break;
            case ESPScoreEvent::Assist: Value = 60; break;
            case ESPScoreEvent::Objective: Value = 250; ++ObjectiveActions; break;
            case ESPScoreEvent::IntelligenceRecovery: Value = 200; ++IntelRecovered; break;
            case ESPScoreEvent::Revive: Value = 125; break;
            case ESPScoreEvent::Reconnaissance: Value = 80; break;
            case ESPScoreEvent::Hack: Value = 150; break;
            case ESPScoreEvent::Defense: Value = 90; break;
            case ESPScoreEvent::CoordinatedAction: Value = 110; break;
            case ESPScoreEvent::Extraction: Value = 300; break;
        }
    }
    TacticalScore += Value;
    SetScore((float)TacticalScore);
}

void ASPPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPPlayerState, Team);
    DOREPLIFETIME(ASPPlayerState, TacticalRole);
    DOREPLIFETIME(ASPPlayerState, bSquadLeader);
    DOREPLIFETIME(ASPPlayerState, TacticalScore);
    DOREPLIFETIME(ASPPlayerState, IntelRecovered);
    DOREPLIFETIME(ASPPlayerState, ObjectiveActions);
    DOREPLIFETIME(ASPPlayerState, bReady);
    DOREPLIFETIME(ASPPlayerState, SelectedSpawnGroup);
    DOREPLIFETIME(ASPPlayerState, ConnectionState);
    DOREPLIFETIME(ASPPlayerState, AuthenticatedSessionId);
    DOREPLIFETIME(ASPPlayerState, AuthenticatedUserId);
    DOREPLIFETIME(ASPPlayerState, bSessionAuthenticated);
    DOREPLIFETIME(ASPPlayerState, Eliminations);
    DOREPLIFETIME(ASPPlayerState, Deaths);
    DOREPLIFETIME(ASPPlayerState, Assists);
    DOREPLIFETIME(ASPPlayerState, Headshots);
    DOREPLIFETIME(ASPPlayerState, TeamDamage);
    DOREPLIFETIME(ASPPlayerState, FriendlyFireIncidents);
    DOREPLIFETIME(ASPPlayerState, FriendlyFireState);
}
