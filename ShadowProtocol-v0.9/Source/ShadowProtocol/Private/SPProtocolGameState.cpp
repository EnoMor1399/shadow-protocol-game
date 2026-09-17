#include "SPProtocolGameState.h"
#include "Net/UnrealNetwork.h"
void ASPProtocolGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPProtocolGameState,MatchPhase);
    DOREPLIFETIME(ASPProtocolGameState,RoundState);
    DOREPLIFETIME(ASPProtocolGameState,RoundTimeRemaining);
    DOREPLIFETIME(ASPProtocolGameState,RoundNumber);
    DOREPLIFETIME(ASPProtocolGameState,DirectorateRoundWins);
    DOREPLIFETIME(ASPProtocolGameState,HelixRoundWins);
    DOREPLIFETIME(ASPProtocolGameState,AttackingTeam);
    DOREPLIFETIME(ASPProtocolGameState,DefendingTeam);
    DOREPLIFETIME(ASPProtocolGameState,SideRotationCount);
    DOREPLIFETIME(ASPProtocolGameState,bMatchComplete);
    DOREPLIFETIME(ASPProtocolGameState,bTrueObjectiveRevealed);
    DOREPLIFETIME(ASPProtocolGameState,bObjectiveSecured);
    DOREPLIFETIME(ASPProtocolGameState,bAlternateExtractionUnlocked);
    DOREPLIFETIME(ASPProtocolGameState,bCamerasDisabled);
    DOREPLIFETIME(ASPProtocolGameState,ActiveObjectiveSiteId);
    DOREPLIFETIME(ASPProtocolGameState,RoundWinner);
    DOREPLIFETIME(ASPProtocolGameState,ExpectedPlayerCount);
    DOREPLIFETIME(ASPProtocolGameState,ReadyPlayerCount);
    DOREPLIFETIME(ASPProtocolGameState,bAllPlayersReady);
    DOREPLIFETIME(ASPProtocolGameState,bOvertimeActive);
    DOREPLIFETIME(ASPProtocolGameState,bOvertimeUsedThisRound);
    DOREPLIFETIME(ASPProtocolGameState,MatchPointTeam);
    DOREPLIFETIME(ASPProtocolGameState,bDecidingRound);
    DOREPLIFETIME(ASPProtocolGameState,bTeamOnlySpectating);
    DOREPLIFETIME(ASPProtocolGameState,PlayerSlots);
    DOREPLIFETIME(ASPProtocolGameState,ServerInstanceId);
    DOREPLIFETIME(ASPProtocolGameState,ServerTickRate);
    DOREPLIFETIME(ASPProtocolGameState,bRankedRules);
}
