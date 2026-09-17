#include "SPProtocolGameMode.h"
#include "SPProtocolGameState.h"
#include "SPPlayerState.h"
#include "SPCharacter.h"
#include "SPObserverPlayerController.h"
#include "SPObjectiveSite.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

ASPProtocolGameMode::ASPProtocolGameMode()
{
    GameStateClass=ASPProtocolGameState::StaticClass();
    PlayerStateClass=ASPPlayerState::StaticClass();
    PlayerControllerClass=ASPObserverPlayerController::StaticClass();
    PrimaryActorTick.bCanEverTick=true;
}

void ASPProtocolGameMode::BeginPlay()
{
    Super::BeginPlay();
    if(auto* GS=GetGameState<ASPProtocolGameState>())
    {
        GS->MatchPhase=ESPMatchPhase::Planning;
        GS->RoundState=ESPRoundState::Waiting;
        GS->RoundTimeRemaining=PlanningDuration;
        GS->AttackingTeam=ESPTeam::DirectorateNine;
        GS->DefendingTeam=ESPTeam::Helix;
        GS->ExpectedPlayerCount=ExpectedCompetitivePlayers;
        GS->bTeamOnlySpectating=true;
        GS->ServerTickRate=DedicatedServerTickRate;
        GS->ServerInstanceId=FString::Printf(TEXT("SP-%s"),*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8).ToUpper());
        GS->bRankedRules=true;
    }
}

void ASPProtocolGameMode::Tick(float DT)
{
    Super::Tick(DT);
    auto* GS=GetGameState<ASPProtocolGameState>();
    UpdateReconnectReservations();
    if(!GS || GS->bMatchComplete) return;

    if(GS->MatchPhase==ESPMatchPhase::Planning && !CanStartCompetitiveMatch())
    {
        GS->RoundTimeRemaining=PlanningDuration;
        return;
    }
    GS->RoundTimeRemaining=FMath::Max(0.f, GS->RoundTimeRemaining-DT);
    if(GS->RoundTimeRemaining>0.f) return;

    if(GS->MatchPhase==ESPMatchPhase::Planning) BeginPreparation();
    else if(GS->RoundState==ESPRoundState::Preparation) BeginDeployment();
    else if(GS->RoundState==ESPRoundState::Action)
    {
        if(GS->bObjectiveSecured && !GS->bOvertimeUsedThisRound) BeginOvertime();
        else FinishRound(GS->DefendingTeam,TEXT("Mission timer expired"));
    }
    else if(GS->RoundState==ESPRoundState::Overtime) FinishRound(GS->DefendingTeam,TEXT("Overtime expired"));
    else if(GS->RoundState==ESPRoundState::PostRound) StartNextRound();
}

void ASPProtocolGameMode::ResetRoundState()
{
    if(auto* GS=GetGameState<ASPProtocolGameState>())
    {
        GS->bTrueObjectiveRevealed=false;
        GS->bObjectiveSecured=false;
        GS->bAlternateExtractionUnlocked=false;
        GS->bCamerasDisabled=false;
        GS->RoundWinner=ESPTeam::None;
        GS->bOvertimeActive=false;
        GS->bOvertimeUsedThisRound=false;
        SelectObjectiveSiteForRound();
    }
}

void ASPProtocolGameMode::BeginPreparation()
{
    if(auto* GS=GetGameState<ASPProtocolGameState>())
    {
        ResetRoundState();
        GS->MatchPhase=ESPMatchPhase::Preparation;
        GS->RoundState=ESPRoundState::Preparation;
        GS->RoundTimeRemaining=PreparationDuration;
    }
}

void ASPProtocolGameMode::BeginDeployment()
{
    if(auto* GS=GetGameState<ASPProtocolGameState>())
    {
        GS->MatchPhase=ESPMatchPhase::Infiltration;
        GS->RoundState=ESPRoundState::Action;
        GS->RoundTimeRemaining=RoundDuration;
    }
}

void ASPProtocolGameMode::StartNextRound()
{
    auto* GS=GetGameState<ASPProtocolGameState>(); if(!GS || GS->bMatchComplete) return;
    ++GS->RoundNumber;
    RotateSidesIfRequired();
    BeginPreparation();
}

void ASPProtocolGameMode::RotateSidesIfRequired()
{
    auto* GS=GetGameState<ASPProtocolGameState>(); if(!GS || SideSwitchInterval<=0) return;
    if(((GS->RoundNumber-1)%SideSwitchInterval)!=0) return;
    Swap(GS->AttackingTeam,GS->DefendingTeam);
    ++GS->SideRotationCount;
}

void ASPProtocolGameMode::ApplyIntelligenceEffect(ESPIntelType Type, FName EffectTag, ASPPlayerState* Player)
{
    auto* GS=GetGameState<ASPProtocolGameState>(); if(!GS || GS->RoundState!=ESPRoundState::Action) return;
    if(EffectTag=="RevealObjective" || Type==ESPIntelType::CommunicationsTerminal){ GS->bTrueObjectiveRevealed=true; GS->MatchPhase=ESPMatchPhase::ObjectiveIdentified; }
    else if(EffectTag=="DisableCameras" || Type==ESPIntelType::SecuritySystem){ GS->bCamerasDisabled=true; }
    else if(EffectTag=="UnlockExtraction" || Type==ESPIntelType::EncryptedDevice || Type==ESPIntelType::ExtractionCoordinates){ GS->bAlternateExtractionUnlocked=true; }
    if(Player) Player->AddScoreEvent(ESPScoreEvent::Hack);
}

void ASPProtocolGameMode::MarkObjectiveSecured(ASPPlayerState* Player)
{
    auto* GS=GetGameState<ASPProtocolGameState>(); if(!GS || GS->RoundState!=ESPRoundState::Action || !GS->bTrueObjectiveRevealed || GS->bObjectiveSecured) return;
    GS->bObjectiveSecured=true; GS->MatchPhase=ESPMatchPhase::ExtractionActive; if(Player) Player->AddScoreEvent(ESPScoreEvent::Objective);
}

void ASPProtocolGameMode::CompleteExtraction(ASPPlayerState* Player)
{
    auto* GS=GetGameState<ASPProtocolGameState>(); if(!GS || GS->RoundState!=ESPRoundState::Action || !GS->bObjectiveSecured) return;
    if(Player) Player->AddScoreEvent(ESPScoreEvent::Extraction); FinishRound(GS->AttackingTeam,TEXT("Intelligence extracted"));
}

void ASPProtocolGameMode::EvaluateEliminationWin()
{
    auto* GS=GetGameState<ASPProtocolGameState>(); if(!GS || GS->RoundState!=ESPRoundState::Action) return;
    int32 AttackersAlive=0, DefendersAlive=0;
    for(APlayerState* BasePS : GameState->PlayerArray)
    {
        auto* PS=Cast<ASPPlayerState>(BasePS); if(!PS || !PS->GetPawn()) continue;
        auto* C=Cast<ASPCharacter>(PS->GetPawn()); if(!C || !C->Health || !C->Health->IsAlive()) continue;
        if(PS->Team==GS->AttackingTeam) ++AttackersAlive;
        if(PS->Team==GS->DefendingTeam) ++DefendersAlive;
    }
    if(AttackersAlive==0) FinishRound(GS->DefendingTeam,TEXT("Attackers eliminated"));
    else if(DefendersAlive==0) FinishRound(GS->AttackingTeam,TEXT("Defenders eliminated"));
}

void ASPProtocolGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    auto* PS=NewPlayer ? NewPlayer->GetPlayerState<ASPPlayerState>() : nullptr;
    if(!PS) return;
    int32 D9=0, Helix=0;
    for(APlayerState* BasePS : GameState->PlayerArray)
    {
        if(const auto* Existing=Cast<ASPPlayerState>(BasePS))
        {
            if(Existing==PS) continue;
            if(Existing->Team==ESPTeam::DirectorateNine) ++D9;
            else if(Existing->Team==ESPTeam::Helix) ++Helix;
        }
    }
    PS->Team=(D9<=Helix && D9<5) ? ESPTeam::DirectorateNine : ESPTeam::Helix;
    PS->ConnectionState=ESPConnectionState::Connected;
    PS->bReady=false;
    RefreshCompetitiveSlots();
}

void ASPProtocolGameMode::Logout(AController* Exiting)
{
    auto* GS=GetGameState<ASPProtocolGameState>();
    if(Exiting)
    {
        if(auto* PS=Exiting->GetPlayerState<ASPPlayerState>())
        {
            // Keep the replicated slot reserved while backend/session services validate a reconnect ticket.
            if(GS)
            {
                for(FSPCompetitivePlayerSlot& Slot : GS->PlayerSlots)
                {
                    if(Slot.PlayerId==PS->GetPlayerId())
                    {
                        Slot.ConnectionState=ESPConnectionState::Reconnecting;
                        Slot.bReady=false;
                        break;
                    }
                }
                GS->ReadyPlayerCount=0;
                for(const FSPCompetitivePlayerSlot& Slot : GS->PlayerSlots)
                    if(Slot.bReady && Slot.ConnectionState==ESPConnectionState::Connected) ++GS->ReadyPlayerCount;
                GS->bAllPlayersReady=false;
            }
            PS->ConnectionState=ESPConnectionState::Reconnecting;
            PS->bReady=false;
            ReconnectDeadlines.Add(PS->GetPlayerId(),GetWorld()->GetTimeSeconds()+ReconnectGraceSeconds);
        }
    }
    Super::Logout(Exiting);
}

void ASPProtocolGameMode::SetPlayerReady(ASPPlayerState* Player,bool bReady)
{
    if(!Player || !Player->HasAuthority()) return;
    Player->bReady=bReady;
    RefreshCompetitiveSlots();
}

bool ASPProtocolGameMode::SelectSpawnGroup(ASPPlayerState* Player,FName SpawnGroupId)
{
    if(!Player || !Player->HasAuthority()) return false;
    for(const FSPSpawnGroup& Group : SpawnGroups)
    {
        if(Group.GroupId==SpawnGroupId && (Group.Team==ESPTeam::None || Group.Team==Player->Team))
        {
            Player->SelectedSpawnGroup=SpawnGroupId;
            RefreshCompetitiveSlots();
            return true;
        }
    }
    return false;
}

bool ASPProtocolGameMode::CanStartCompetitiveMatch() const
{
    const auto* GS=GetGameState<ASPProtocolGameState>();
    if(!GS || GS->ReadyPlayerCount<ExpectedCompetitivePlayers || GS->PlayerSlots.Num()<ExpectedCompetitivePlayers) return false;
    if(bRequireAuthenticatedSessions)
    {
        int32 AuthenticatedReady=0;
        for(APlayerState* BasePS : GameState->PlayerArray)
        {
            const auto* PS=Cast<ASPPlayerState>(BasePS);
            if(PS && PS->bReady && PS->ConnectionState==ESPConnectionState::Connected && PS->bSessionAuthenticated) ++AuthenticatedReady;
        }
        if(AuthenticatedReady<ExpectedCompetitivePlayers) return false;
    }
    return true;
}

void ASPProtocolGameMode::RefreshCompetitiveSlots()
{
    auto* GS=GetGameState<ASPProtocolGameState>(); if(!GS) return;
    TArray<FSPCompetitivePlayerSlot> Reserved;
    for(const FSPCompetitivePlayerSlot& Existing:GS->PlayerSlots)
        if(Existing.ConnectionState==ESPConnectionState::Reconnecting) Reserved.Add(Existing);
    GS->PlayerSlots.Reset();
    int32 Index=0, Ready=0;
    for(APlayerState* BasePS : GameState->PlayerArray)
    {
        auto* PS=Cast<ASPPlayerState>(BasePS); if(!PS || Index>=ExpectedCompetitivePlayers) continue;
        FSPCompetitivePlayerSlot Slot;
        Slot.SlotIndex=Index++;
        Slot.PlayerId=PS->GetPlayerId();
        Slot.Team=PS->Team;
        Slot.SpawnGroup=PS->SelectedSpawnGroup;
        Slot.Callsign=PS->GetPlayerName();
        Slot.SessionId=PS->AuthenticatedSessionId;
        Slot.bReady=PS->bReady;
        Slot.ConnectionState=PS->ConnectionState;
        if(Slot.bReady && Slot.ConnectionState==ESPConnectionState::Connected) ++Ready;
        GS->PlayerSlots.Add(Slot);
    }
    for(FSPCompetitivePlayerSlot Slot:Reserved)
    {
        if(Index>=ExpectedCompetitivePlayers) break;
        bool bReclaimed=false;
        for(const FSPCompetitivePlayerSlot& Live:GS->PlayerSlots) if(!Slot.SessionId.IsEmpty() && Live.SessionId==Slot.SessionId){bReclaimed=true;break;}
        if(bReclaimed) continue;
        Slot.SlotIndex=Index++;Slot.bReady=false;GS->PlayerSlots.Add(Slot);
    }
    GS->ReadyPlayerCount=Ready;
    GS->bAllPlayersReady=Ready>=ExpectedCompetitivePlayers && GS->PlayerSlots.Num()>=ExpectedCompetitivePlayers;
}

AActor* ASPProtocolGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    const auto* PS=Player ? Player->GetPlayerState<ASPPlayerState>() : nullptr;
    if(PS)
    {
        TArray<AActor*> Starts;
        UGameplayStatics::GetAllActorsOfClass(this,APlayerStart::StaticClass(),Starts);
        for(AActor* Candidate : Starts)
        {
            const auto* Start=Cast<APlayerStart>(Candidate);
            if(Start && Start->PlayerStartTag==PS->SelectedSpawnGroup && IsSpawnStartValid(Start,PS)) return Candidate;
        }
    }
    return Super::ChoosePlayerStart_Implementation(Player);
}

void ASPProtocolGameMode::BeginOvertime()
{
    if(auto* GS=GetGameState<ASPProtocolGameState>())
    {
        GS->bOvertimeActive=true;
        GS->bOvertimeUsedThisRound=true;
        GS->RoundState=ESPRoundState::Overtime;
        GS->RoundTimeRemaining=OvertimeDuration;
    }
}

void ASPProtocolGameMode::UpdateMatchPointState()
{
    auto* GS=GetGameState<ASPProtocolGameState>(); if(!GS) return;
    GS->MatchPointTeam=ESPTeam::None;
    GS->bDecidingRound=GS->DirectorateRoundWins==RoundsToWin-1 && GS->HelixRoundWins==RoundsToWin-1;
    if(!GS->bDecidingRound && GS->DirectorateRoundWins==RoundsToWin-1) GS->MatchPointTeam=ESPTeam::DirectorateNine;
    if(!GS->bDecidingRound && GS->HelixRoundWins==RoundsToWin-1) GS->MatchPointTeam=ESPTeam::Helix;
}

void ASPProtocolGameMode::FinishRound(ESPTeam Winner,const FString& Reason)
{
    auto* GS=GetGameState<ASPProtocolGameState>(); if(!GS || GS->RoundState==ESPRoundState::PostRound || GS->bMatchComplete) return;
    GS->RoundWinner=Winner;
    if(Winner==ESPTeam::DirectorateNine) ++GS->DirectorateRoundWins;
    else if(Winner==ESPTeam::Helix) ++GS->HelixRoundWins;
    UpdateMatchPointState();

    const bool bMatchWon=GS->DirectorateRoundWins>=RoundsToWin || GS->HelixRoundWins>=RoundsToWin || GS->RoundNumber>=MaxRounds;
    if(bMatchWon)
    {
        GS->bMatchComplete=true;
        GS->MatchPhase=ESPMatchPhase::MatchComplete;
        GS->RoundState=ESPRoundState::Complete;
        GS->RoundTimeRemaining=0.f;
    }
    else
    {
        GS->MatchPhase=ESPMatchPhase::RoundComplete;
        GS->RoundState=ESPRoundState::PostRound;
        GS->RoundTimeRemaining=PostRoundDuration;
    }
    UE_LOG(LogTemp,Log,TEXT("Shadow Protocol round complete: %s"),*Reason);
}


bool ASPProtocolGameMode::AuthorizePlayerSession(ASPPlayerState* Player,const FString& SessionId)
{
    if(!Player || SessionId.Len()<8) return false;
    auto* GS=GetGameState<ASPProtocolGameState>();
    if(GS)
    {
        for(FSPCompetitivePlayerSlot& Slot:GS->PlayerSlots)
        {
            if(Slot.ConnectionState==ESPConnectionState::Reconnecting && Slot.SessionId==SessionId)
            {
                ReconnectDeadlines.Remove(Slot.PlayerId);
                Player->Team=Slot.Team;
                Player->SelectedSpawnGroup=Slot.SpawnGroup;
                Slot.PlayerId=Player->GetPlayerId();
                Slot.ConnectionState=ESPConnectionState::Connected;
                break;
            }
        }
    }
    Player->AuthenticatedSessionId=SessionId;
    Player->bSessionAuthenticated=true;
    Player->ConnectionState=ESPConnectionState::Connected;
    ReconnectDeadlines.Remove(Player->GetPlayerId());
    RefreshCompetitiveSlots();
    return true;
}

bool ASPProtocolGameMode::ValidateClientShotTimestamp(float ClientServerTimeSeconds) const
{
    if(!GetWorld()) return false;
    const float Now=GetWorld()->GetTimeSeconds();
    const float Age=Now-ClientServerTimeSeconds;
    return Age>=-0.025f && Age<=MaxAcceptedShotAgeSeconds;
}

float ASPProtocolGameMode::ResolveCompetitiveDamage(ASPCharacter* Shooter,ASPCharacter* Victim,float RawDamage,bool& bOutReverseDamage)
{
    bOutReverseDamage=false;
    if(!Shooter || !Victim || RawDamage<=0.f) return 0.f;
    auto* ShooterPS=Shooter->GetPlayerState<ASPPlayerState>();
    const auto* VictimPS=Victim->GetPlayerState<ASPPlayerState>();
    if(ShooterPS && VictimPS && ShooterPS!=VictimPS && ShooterPS->Team!=ESPTeam::None && ShooterPS->Team==VictimPS->Team)
    {
        const float Scaled=RawDamage*FriendlyDamageScale;
        ShooterPS->TeamDamage+=Scaled;
        ++ShooterPS->FriendlyFireIncidents;
        if(ShooterPS->FriendlyFireIncidents>=ReverseFriendlyFireAfterIncidents)
        {
            ShooterPS->FriendlyFireState=ESPFriendlyFireState::ReverseDamage;
            bOutReverseDamage=true;
        }
        else ShooterPS->FriendlyFireState=ESPFriendlyFireState::Warning;
        return Scaled;
    }
    return RawDamage;
}

void ASPProtocolGameMode::RegisterDamageContribution(ASPCharacter* Shooter,ASPCharacter* Victim,float AppliedDamage)
{
    if(!Shooter || !Victim || AppliedDamage<=0.f) return;
    auto* ShooterPS=Shooter->GetPlayerState<ASPPlayerState>();const auto* VictimPS=Victim->GetPlayerState<ASPPlayerState>();
    if(!ShooterPS || !VictimPS || ShooterPS->Team==VictimPS->Team) return;
    const TWeakObjectPtr<ASPCharacter> VictimKey(Victim);
    const TWeakObjectPtr<ASPPlayerState> ShooterKey(ShooterPS);
    DamageLedger.FindOrAdd(VictimKey).FindOrAdd(ShooterKey)+=AppliedDamage;
}

void ASPProtocolGameMode::RegisterElimination(ASPCharacter* Killer,ASPCharacter* Victim,bool bHeadshot)
{
    if(!Victim) return;
    auto* KillerPS=Killer?Killer->GetPlayerState<ASPPlayerState>():nullptr;
    auto* VictimPS=Victim->GetPlayerState<ASPPlayerState>();
    if(VictimPS) ++VictimPS->Deaths;
    if(KillerPS && VictimPS && KillerPS->Team!=VictimPS->Team)
    {
        ++KillerPS->Eliminations;if(bHeadshot)++KillerPS->Headshots;
        KillerPS->AddScoreEvent(ESPScoreEvent::Elimination,bHeadshot?125:100);
        const TWeakObjectPtr<ASPCharacter> VictimKey(Victim);
        if(TMap<TWeakObjectPtr<ASPPlayerState>,float>* Contributions=DamageLedger.Find(VictimKey))
        {
            for(const auto& Pair:*Contributions)
            {
                ASPPlayerState* Contributor=Pair.Key.Get();
                if(Contributor && Contributor!=KillerPS && Pair.Value>=20.f){++Contributor->Assists;Contributor->AddScoreEvent(ESPScoreEvent::Assist);}
            }
        }
    }
    DamageLedger.Remove(TWeakObjectPtr<ASPCharacter>(Victim));
    EvaluateEliminationWin();
}

bool ASPProtocolGameMode::IsSpawnStartValid(const APlayerStart* Start,const ASPPlayerState* Player) const
{
    if(!Start || !Player) return false;
    const bool bTaggedD9=Start->ActorHasTag(TEXT("D9")),bTaggedHelix=Start->ActorHasTag(TEXT("HELIX"));
    if((bTaggedD9||bTaggedHelix) && ((Player->Team==ESPTeam::DirectorateNine&&!bTaggedD9)||(Player->Team==ESPTeam::Helix&&!bTaggedHelix))) return false;
    for(APlayerState* BasePS : GameState->PlayerArray)
    {
        const APawn* Pawn=BasePS?BasePS->GetPawn():nullptr;
        if(Pawn && FVector::DistSquared(Pawn->GetActorLocation(),Start->GetActorLocation())<FMath::Square(100.f)) return false;
    }
    return true;
}

void ASPProtocolGameMode::UpdateReconnectReservations()
{
    if(!GetWorld()) return;
    auto* GS=GetGameState<ASPProtocolGameState>();if(!GS)return;
    const float Now=GetWorld()->GetTimeSeconds();
    for(FSPCompetitivePlayerSlot& Slot:GS->PlayerSlots)
    {
        if(Slot.ConnectionState!=ESPConnectionState::Reconnecting) continue;
        const float* Deadline=ReconnectDeadlines.Find(Slot.PlayerId);
        if(Deadline && Now>=*Deadline){Slot.ConnectionState=ESPConnectionState::Disconnected;Slot.bReady=false;ReconnectDeadlines.Remove(Slot.PlayerId);}
    }
}

void ASPProtocolGameMode::SelectObjectiveSiteForRound()
{
    auto* GS=GetGameState<ASPProtocolGameState>();if(!GS)return;
    TArray<AActor*> Found;UGameplayStatics::GetAllActorsOfClass(this,ASPObjectiveSite::StaticClass(),Found);
    if(Found.Num()==0){GS->ActiveObjectiveSiteId=NAME_None;return;}
    Found.Sort([](const AActor& A,const AActor& B){return A.GetName()<B.GetName();});
    const int32 ActiveIndex=(GS->RoundNumber-1)%Found.Num();
    for(int32 i=0;i<Found.Num();++i)
    {
        if(auto* Site=Cast<ASPObjectiveSite>(Found[i])){const bool bActive=i==ActiveIndex;Site->SetActiveSite(bActive);if(bActive)GS->ActiveObjectiveSiteId=Site->SiteId;}
    }
}
