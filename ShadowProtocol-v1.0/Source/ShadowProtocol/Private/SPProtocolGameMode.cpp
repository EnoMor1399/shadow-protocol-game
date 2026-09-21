#include "SPProtocolGameMode.h"
#include "SPCombatHUD.h"
#include "SPOnlineGameSession.h"
#include "SPProtocolGameState.h"
#include "SPPlayerState.h"
#include "SPCharacter.h"
#include "SPObserverPlayerController.h"
#include "SPObjectiveSite.h"
#include "SPDedicatedServerBackendSubsystem.h"
#include "SPBuildInfoLibrary.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Misc/App.h"
#include "HAL/PlatformTime.h"

namespace
{
bool IsSafeAdmissionOption(const FString& Value)
{
    if (Value.IsEmpty()) return false;

    for (int32 Index = 0; Index < Value.Len(); ++Index)
    {
        const TCHAR Character = Value[Index];
        if (!FChar::IsAlnum(Character)
            && Character != TEXT('-')
            && Character != TEXT('_')
            && Character != TEXT('.'))
        {
            return false;
        }
    }
    return true;
}
}

ASPProtocolGameMode::ASPProtocolGameMode()
{
    HUDClass=ASPCombatHUD::StaticClass();
    GameSessionClass=ASPOnlineGameSession::StaticClass();
    DefaultPawnClass=ASPCharacter::StaticClass();
    GameStateClass=ASPProtocolGameState::StaticClass();
    PlayerStateClass=ASPPlayerState::StaticClass();
    PlayerControllerClass=ASPObserverPlayerController::StaticClass();
    PrimaryActorTick.bCanEverTick=true;
}

void ASPProtocolGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (IsDedicatedAdmissionRequired())
    {
        if (USPDedicatedServerBackendSubsystem* Backend = GetDedicatedServerBackend())
        {
            Backend->OnAdmissionCompleted.AddDynamic(this, &ASPProtocolGameMode::HandleBackendAdmissionCompleted);
            Backend->OnAdmissionFailed.AddDynamic(this, &ASPProtocolGameMode::HandleBackendAdmissionFailed);
        }
    }

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

void ASPProtocolGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (USPDedicatedServerBackendSubsystem* Backend = GetDedicatedServerBackend())
    {
        Backend->OnAdmissionCompleted.RemoveDynamic(this, &ASPProtocolGameMode::HandleBackendAdmissionCompleted);
        Backend->OnAdmissionFailed.RemoveDynamic(this, &ASPProtocolGameMode::HandleBackendAdmissionFailed);
    }
    PendingAdmissions.Reset();
    AdmittedPlayerMatches.Reset();
    Super::EndPlay(EndPlayReason);
}

bool ASPProtocolGameMode::IsDedicatedAdmissionRequired() const
{
    return bRequireDedicatedServerAdmission && IsRunningDedicatedServer();
}

USPDedicatedServerBackendSubsystem* ASPProtocolGameMode::GetDedicatedServerBackend() const
{
    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    return GameInstance ? GameInstance->GetSubsystem<USPDedicatedServerBackendSubsystem>() : nullptr;
}

void ASPProtocolGameMode::PreLogin(
    const FString& Options,
    const FString& Address,
    const FUniqueNetIdRepl& UniqueId,
    FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (!ErrorMessage.IsEmpty() || !IsDedicatedAdmissionRequired())
    {
        return;
    }

    const auto* OnlineSession = Cast<ASPOnlineGameSession>(GameSession);
    if (!OnlineSession || !OnlineSession->IsAcceptingAdmissions())
    {
        ErrorMessage = TEXT("Dedicated server online session is unavailable.");
        return;
    }

    const FString AllocationId = UGameplayStatics::ParseOption(Options, TEXT("spAllocationId"));
    const FString MatchId = UGameplayStatics::ParseOption(Options, TEXT("spMatchId"));
    const FString ConnectToken = UGameplayStatics::ParseOption(Options, TEXT("spConnectToken"));
    const FString TargetServerId = UGameplayStatics::ParseOption(Options, TEXT("spServerId"));
    const FString NetworkBuild = UGameplayStatics::ParseOption(Options, TEXT("spNetworkBuild"));

    const FString ReconnectGrantId = UGameplayStatics::ParseOption(Options, TEXT("spReconnectGrantId"));
    FGuid ReconnectGuid;
    if (!ReconnectGrantId.IsEmpty() && !FGuid::Parse(ReconnectGrantId, ReconnectGuid))
    {
        ErrorMessage = TEXT("Invalid reconnect grant.");
        return;
    }

    FGuid AllocationGuid;
    FGuid MatchGuid;
    if (!FGuid::Parse(AllocationId, AllocationGuid)
        || !FGuid::Parse(MatchId, MatchGuid)
        || ConnectToken.Len() > 256
        || ConnectToken.Len() < 24
        || !IsSafeAdmissionOption(ConnectToken)
        || !IsSafeAdmissionOption(TargetServerId)
        || !IsSafeAdmissionOption(NetworkBuild))
    {
        ErrorMessage = TEXT("Invalid or incomplete Shadow Protocol admission envelope.");
        return;
    }

    USPDedicatedServerBackendSubsystem* Backend = GetDedicatedServerBackend();
    if (!Backend || !Backend->IsConfigured() || !Backend->IsRegistered() || Backend->IsDraining())
    {
        ErrorMessage = TEXT("Dedicated server admission service is unavailable.");
        return;
    }

    if (!TargetServerId.Equals(Backend->GetServerId(), ESearchCase::CaseSensitive))
    {
        ErrorMessage = TEXT("Allocation targets a different dedicated server.");
        return;
    }

    if (!NetworkBuild.Equals(USPBuildInfoLibrary::GetNetworkBuildId(), ESearchCase::CaseSensitive))
    {
        ErrorMessage = TEXT("Client/server network build mismatch.");
    }
}

FString ASPProtocolGameMode::InitNewPlayer(
    APlayerController* NewPlayerController,
    const FUniqueNetIdRepl& UniqueId,
    const FString& Options,
    const FString& Portal)
{
    const FString InitError = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
    if (!InitError.IsEmpty() || !IsDedicatedAdmissionRequired())
    {
        return InitError;
    }

    const FString AllocationId = UGameplayStatics::ParseOption(Options, TEXT("spAllocationId"));
    const FString MatchId = UGameplayStatics::ParseOption(Options, TEXT("spMatchId"));
    const FString ConnectToken = UGameplayStatics::ParseOption(Options, TEXT("spConnectToken"));

    if (!NewPlayerController || PendingAdmissions.Contains(AllocationId))
    {
        return TEXT("Duplicate or invalid pending allocation.");
    }

    FSPPendingPlayerAdmission Pending;
    Pending.Controller = NewPlayerController;
    Pending.AllocationId = AllocationId;
    Pending.MatchId = MatchId;
    Pending.ConnectToken = ConnectToken;
    Pending.RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Pending.ReconnectGrantId = UGameplayStatics::ParseOption(Options, TEXT("spReconnectGrantId"));
    Pending.DeadlineRealSeconds = FPlatformTime::Seconds() + FMath::Max(3.0f, PendingAdmissionTimeoutSeconds);
    PendingAdmissions.Add(AllocationId, MoveTemp(Pending));
    return FString();
}

void ASPProtocolGameMode::Tick(float DT)
{
    Super::Tick(DT);
    UpdatePendingAdmissions();
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
    const auto* CurrentState = GetGameState<ASPProtocolGameState>();
    if (!CurrentState || CurrentState->bMatchComplete) return;
    // A player can withdraw readiness while asynchronous OSS start is pending.
    if (CurrentState->MatchPhase == ESPMatchPhase::Planning && !CanStartCompetitiveMatch()) return;
    if (IsDedicatedAdmissionRequired())
    {
        auto* OnlineSession = Cast<ASPOnlineGameSession>(GameSession);
        if (!OnlineSession || !OnlineSession->StartProtocolSession()) return;
    }
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

    auto* PS = NewPlayer ? NewPlayer->GetPlayerState<ASPPlayerState>() : nullptr;
    if (!PS) return;

    if (IsDedicatedAdmissionRequired())
    {
        FString AllocationId;
        if (!FindPendingAdmissionForController(NewPlayer, AllocationId))
        {
            DisconnectPlayer(NewPlayer, TEXT("Dedicated-server admission state was not created."));
            return;
        }

        FSPPendingPlayerAdmission* Pending = PendingAdmissions.Find(AllocationId);
        USPDedicatedServerBackendSubsystem* Backend = GetDedicatedServerBackend();
        if (!Pending || !Backend || !Backend->IsRegistered() || Backend->IsDraining())
        {
            RejectPendingAdmission(AllocationId, TEXT("Dedicated-server admission service became unavailable."));
            return;
        }

        PS->Team = ESPTeam::None;
        PS->ConnectionState = ESPConnectionState::Disconnected;
        PS->bReady = false;
        PS->bSessionAuthenticated = false;
        PS->AuthenticatedSessionId.Reset();
        PS->AuthenticatedUserId.Reset();

        const auto* CurrentState = GetGameState<ASPProtocolGameState>();
        if (!CurrentState || (!Pending->ReconnectGrantId.IsEmpty()
            && (CurrentState->bMatchComplete || CurrentState->MatchPhase != ESPMatchPhase::Planning
                || CurrentState->RoundState != ESPRoundState::Waiting)))
        {
            RejectPendingAdmission(AllocationId, TEXT("Reconnect is only available in the ready room."));
            return;
        }
        Pending->bRequestStarted = true;
        const FString ConnectToken = Pending->ConnectToken;
        Pending->ConnectToken.Reset();
        Backend->AdmitConnection(Pending->AllocationId, Pending->MatchId, ConnectToken,
            Pending->RequestId, Pending->ReconnectGrantId, GetGameState<ASPProtocolGameState>()->RoundNumber);
        return;
    }

    AssignCompetitiveTeam(PS);
    PS->ConnectionState = ESPConnectionState::Connected;
    PS->bReady = false;
    RefreshCompetitiveSlots();
}

void ASPProtocolGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    if (IsDedicatedAdmissionRequired())
    {
        FString AllocationId;
        if (FindPendingAdmissionForController(NewPlayer, AllocationId))
        {
            return;
        }

        const ASPPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<ASPPlayerState>() : nullptr;
        if (!PS || !PS->bSessionAuthenticated)
        {
            return;
        }
    }

    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void ASPProtocolGameMode::AssignCompetitiveTeam(ASPPlayerState* Player)
{
    if (!Player) return;

    int32 D9 = 0;
    int32 Helix = 0;
    for (APlayerState* BasePS : GameState->PlayerArray)
    {
        const auto* Existing = Cast<ASPPlayerState>(BasePS);
        if (!Existing || Existing == Player || (IsDedicatedAdmissionRequired() && !Existing->bSessionAuthenticated))
        {
            continue;
        }

        if (Existing->Team == ESPTeam::DirectorateNine) ++D9;
        else if (Existing->Team == ESPTeam::Helix) ++Helix;
    }

    Player->Team = (D9 <= Helix && D9 < 5) ? ESPTeam::DirectorateNine : ESPTeam::Helix;
}

bool ASPProtocolGameMode::FindPendingAdmissionForController(APlayerController* PlayerController, FString& OutAllocationId) const
{
    if (!PlayerController) return false;

    for (const TPair<FString, FSPPendingPlayerAdmission>& Pair : PendingAdmissions)
    {
        if (Pair.Value.Controller.Get() == PlayerController)
        {
            OutAllocationId = Pair.Key;
            return true;
        }
    }
    return false;
}

void ASPProtocolGameMode::HandleBackendAdmissionCompleted(FSPDedicatedServerAdmission Admission)
{
    FSPPendingPlayerAdmission* Pending = PendingAdmissions.Find(Admission.AllocationId);
    if (!Pending || Pending->MatchId != Admission.MatchId || Pending->RequestId != Admission.RequestId)
    {
        return;
    }

    APlayerController* PlayerController = Pending->Controller.Get();
    if (!PlayerController)
    {
        PendingAdmissions.Remove(Admission.AllocationId);
        return;
    }

    PromoteAdmittedPlayer(PlayerController, Admission);
}

void ASPProtocolGameMode::PromoteAdmittedPlayer(APlayerController* PlayerController, const FSPDedicatedServerAdmission& Admission)
{
    FSPPendingPlayerAdmission* Pending = PendingAdmissions.Find(Admission.AllocationId);
    if (!Pending || Pending->Controller.Get() != PlayerController || Pending->MatchId != Admission.MatchId
        || Pending->RequestId != Admission.RequestId || Pending->ReconnectGrantId != Admission.ReconnectGrantId)
    {
        return;
    }

    const auto* OnlineSession = Cast<ASPOnlineGameSession>(GameSession);
    const auto* Backend = GetDedicatedServerBackend();
    if (!Pending->bRequestStarted || Pending->DeadlineRealSeconds <= FPlatformTime::Seconds()
        || !OnlineSession || !OnlineSession->IsAcceptingAdmissions()
        || !Backend || !Backend->IsRegistered() || Backend->IsDraining())
    {
        RejectPendingAdmission(Admission.AllocationId, TEXT("Admission expired or server became unavailable."));
        return;
    }

    ASPPlayerState* PS = PlayerController->GetPlayerState<ASPPlayerState>();
    if (!PS || Admission.UserId.IsEmpty())
    {
        RejectPendingAdmission(Admission.AllocationId, TEXT("Backend admission did not provide a valid authenticated player identity."));
        return;
    }

    auto* GS = GetGameState<ASPProtocolGameState>();
    for (APlayerState* BasePS : GameState->PlayerArray)
    {
        const auto* Existing = Cast<ASPPlayerState>(BasePS);
        if (Existing && Existing != PS && Existing->bSessionAuthenticated
            && Existing->AuthenticatedUserId == Admission.UserId)
        {
            RejectPendingAdmission(Admission.AllocationId, TEXT("Player is already connected."));
            return;
        }
    }
    const FSPCompetitivePlayerSlot* Reserved = nullptr;
    if (!Admission.ReconnectGrantId.IsEmpty())
    {
        const FString* ReservedMatch = AdmittedPlayerMatches.Find(Admission.UserId);
        // Until pawn/life-state recovery exists, reconnect is restricted to the ready room.
        if (ReservedMatch && *ReservedMatch == Admission.MatchId
            && GS && !GS->bMatchComplete && GS->RoundNumber == Admission.RoundNumber
            && GS->MatchPhase == ESPMatchPhase::Planning && GS->RoundState == ESPRoundState::Waiting)
            for (const FSPCompetitivePlayerSlot& Slot : GS->PlayerSlots)
            {
                const float* Deadline = ReconnectDeadlines.Find(Slot.PlayerId);
                if (Slot.SessionId == Admission.UserId && Slot.SlotIndex == Admission.SlotIndex
                    && Slot.ConnectionState == ESPConnectionState::Reconnecting
                    && Deadline && *Deadline > GetWorld()->GetTimeSeconds()) { Reserved = &Slot; break; }
            }
        if (!Reserved)
        {
            RejectPendingAdmission(Admission.AllocationId, TEXT("Local reconnect reservation is no longer valid."));
            return;
        }
        PS->Team = Reserved->Team;
        PS->SelectedSpawnGroup = Reserved->SpawnGroup;
        ReconnectDeadlines.Remove(Reserved->PlayerId);
    }
    if (!Reserved)
    {
        if (Admission.SlotIndex < 0 || Admission.SlotIndex >= ExpectedCompetitivePlayers)
        {
            RejectPendingAdmission(Admission.AllocationId, TEXT("Backend admission returned an invalid competitive slot."));
            return;
        }

        if (Admission.Team.Equals(TEXT("DirectorateNine"), ESearchCase::CaseSensitive))
        {
            PS->Team = ESPTeam::DirectorateNine;
        }
        else if (Admission.Team.Equals(TEXT("Helix"), ESearchCase::CaseSensitive))
        {
            PS->Team = ESPTeam::Helix;
        }
        else
        {
            RejectPendingAdmission(Admission.AllocationId, TEXT("Backend admission returned an invalid competitive team."));
            return;
        }
    }

    PendingAdmissions.Remove(Admission.AllocationId);

    AdmittedPlayerMatches.Add(Admission.UserId, Admission.MatchId);
    PS->AuthenticatedUserId = Admission.UserId;
    PS->AuthenticatedSessionId = Admission.UserId;
    PS->CompetitiveSlotIndex = Admission.SlotIndex;
    PS->bSessionAuthenticated = true;
    PS->ConnectionState = ESPConnectionState::Connected;
    PS->bReady = false;
    RefreshCompetitiveSlots();

    Super::HandleStartingNewPlayer_Implementation(PlayerController);
}

void ASPProtocolGameMode::HandleBackendAdmissionFailed(FString AllocationId, FString MatchId, FString RequestId, FString ErrorMessage)
{
    const FSPPendingPlayerAdmission* Pending = PendingAdmissions.Find(AllocationId);
    if (!Pending || Pending->MatchId != MatchId || Pending->RequestId != RequestId)
    {
        return;
    }

    RejectPendingAdmission(AllocationId, ErrorMessage.IsEmpty() ? TEXT("Backend admission denied.") : ErrorMessage);
}

void ASPProtocolGameMode::RejectPendingAdmission(const FString& AllocationId, const FString& Reason)
{
    FSPPendingPlayerAdmission* Pending = PendingAdmissions.Find(AllocationId);
    if (!Pending) return;

    TWeakObjectPtr<APlayerController> Controller = Pending->Controller;
    PendingAdmissions.Remove(AllocationId);

    if (Controller.IsValid())
    {
        DisconnectPlayer(Controller.Get(), Reason);
    }
}

void ASPProtocolGameMode::DisconnectPlayer(APlayerController* PlayerController, const FString& Reason)
{
    if (!PlayerController) return;

    if (ASPPlayerState* PS = PlayerController->GetPlayerState<ASPPlayerState>())
    {
        PS->bSessionAuthenticated = false;
        PS->ConnectionState = ESPConnectionState::Disconnected;
        PS->bReady = false;
    }

    const FText ReasonText = FText::FromString(Reason.IsEmpty() ? TEXT("Dedicated-server admission denied.") : Reason);
    if (GameSession && GameSession->KickPlayer(PlayerController, ReasonText)) return;
    PlayerController->ClientReturnToMainMenuWithTextReason(ReasonText);
    PlayerController->Destroy();
}

void ASPProtocolGameMode::UpdatePendingAdmissions()
{
    if (!GetWorld() || PendingAdmissions.IsEmpty())
    {
        return;
    }

    const double Now = FPlatformTime::Seconds();
    TArray<FString> ExpiredAllocations;
    for (const TPair<FString, FSPPendingPlayerAdmission>& Pair : PendingAdmissions)
    {
        if (!Pair.Value.Controller.IsValid() || Pair.Value.DeadlineRealSeconds <= Now)
        {
            ExpiredAllocations.Add(Pair.Key);
        }
    }

    for (const FString& AllocationId : ExpiredAllocations)
    {
        RejectPendingAdmission(AllocationId, TEXT("Dedicated-server admission timed out."));
    }
}

void ASPProtocolGameMode::Logout(AController* Exiting)
{
    if (APlayerController* PlayerController = Cast<APlayerController>(Exiting))
    {
        FString PendingAllocationId;
        if (FindPendingAdmissionForController(PlayerController, PendingAllocationId))
        {
            PendingAdmissions.Remove(PendingAllocationId);
            Super::Logout(Exiting);
            return;
        }
    }

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

bool ASPProtocolGameMode::CanEditReadyRoom(const ASPPlayerState* Player) const
{
    const auto* GS = GetGameState<ASPProtocolGameState>();
    if (!HasAuthority() || !Player || !Player->HasAuthority() || Player->GetWorld() != GetWorld()
        || !GS || !GS->PlayerArray.Contains(Player) || GS->bMatchComplete
        || GS->MatchPhase != ESPMatchPhase::Planning || GS->RoundState != ESPRoundState::Waiting
        || Player->ConnectionState != ESPConnectionState::Connected || Player->Team == ESPTeam::None)
        return false;
    if ((bRequireAuthenticatedSessions || IsDedicatedAdmissionRequired()) && !Player->bSessionAuthenticated)
        return false;
    if (IsDedicatedAdmissionRequired())
    {
        const auto* Backend = GetDedicatedServerBackend();
        const auto* OnlineSession = Cast<ASPOnlineGameSession>(GameSession);
        if (!Backend || !Backend->IsRegistered() || Backend->IsDraining()
            || !OnlineSession || !OnlineSession->IsAcceptingAdmissions()) return false;
    }
    return true;
}

TArray<FName> ASPProtocolGameMode::GetReadyRoomSpawnGroups(const ASPPlayerState* Player) const
{
    TArray<FName> Choices;
    if (!CanEditReadyRoom(Player)) return Choices;
    for (const auto& Group : SpawnGroups)
        if (!Group.GroupId.IsNone() && (Group.Team == ESPTeam::None || Group.Team == Player->Team))
            Choices.AddUnique(Group.GroupId);
    return Choices;
}

bool ASPProtocolGameMode::SetPlayerReady(ASPPlayerState* Player,bool bReady)
{
    if (!CanEditReadyRoom(Player)) return false;
    Player->bReady=bReady;
    Player->ForceNetUpdate();
    RefreshCompetitiveSlots();
    return true;
}

bool ASPProtocolGameMode::SelectSpawnGroup(ASPPlayerState* Player,FName SpawnGroupId)
{
    if (!CanEditReadyRoom(Player) || SpawnGroupId.IsNone()) return false;
    for(const FSPSpawnGroup& Group : SpawnGroups)
    {
        if(Group.GroupId==SpawnGroupId && (Group.Team==ESPTeam::None || Group.Team==Player->Team))
        {
            if (Player->SelectedSpawnGroup != SpawnGroupId)
            {
                Player->SelectedSpawnGroup=SpawnGroupId;
                Player->bReady=false; // Changed deployment choices require confirmation.
                Player->ForceNetUpdate();
            }
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
    TArray<FSPCompetitivePlayerSlot> Previous = GS->PlayerSlots;
    Previous.RemoveAll([](const FSPCompetitivePlayerSlot& Slot) { return Slot.ConnectionState == ESPConnectionState::Disconnected; });
    TSet<int32> Occupied;
    for (const auto& Slot : Previous) Occupied.Add(Slot.SlotIndex);
    TArray<FSPCompetitivePlayerSlot> Reserved;
    for(const FSPCompetitivePlayerSlot& Existing:GS->PlayerSlots)
        if(Existing.ConnectionState==ESPConnectionState::Reconnecting) Reserved.Add(Existing);
    GS->PlayerSlots.Reset();
    int32 Index=0, Ready=0;
    for(APlayerState* BasePS : GameState->PlayerArray)
    {
        auto* PS=Cast<ASPPlayerState>(BasePS); if(!PS || Index>=ExpectedCompetitivePlayers) continue;
        if (bRequireAuthenticatedSessions && !PS->bSessionAuthenticated) continue;
        FSPCompetitivePlayerSlot Slot;
        const FString Identity = !PS->AuthenticatedUserId.IsEmpty() ? PS->AuthenticatedUserId : PS->AuthenticatedSessionId;
        const auto* ExistingSlot = Previous.FindByPredicate([&](const FSPCompetitivePlayerSlot& Old)
            { return !Identity.IsEmpty() ? Old.SessionId == Identity : Old.PlayerId == PS->GetPlayerId(); });
        if (ExistingSlot) Slot.SlotIndex = ExistingSlot->SlotIndex;
        else if (PS->CompetitiveSlotIndex >= 0 && PS->CompetitiveSlotIndex < ExpectedCompetitivePlayers
            && !Occupied.Contains(PS->CompetitiveSlotIndex))
        {
            Slot.SlotIndex = PS->CompetitiveSlotIndex;
            Occupied.Add(Slot.SlotIndex);
        }
        else
        {
            int32 FreeIndex = 0;
            while (Occupied.Contains(FreeIndex)) ++FreeIndex;
            if (FreeIndex >= ExpectedCompetitivePlayers) continue;
            Slot.SlotIndex = FreeIndex;
            Occupied.Add(FreeIndex);
        }
        PS->CompetitiveSlotIndex = Slot.SlotIndex;
        ++Index;
        Slot.PlayerId=PS->GetPlayerId();
        Slot.Team=PS->Team;
        Slot.SpawnGroup=PS->SelectedSpawnGroup;
        Slot.Callsign=PS->GetPlayerName();
        Slot.SessionId=!PS->AuthenticatedUserId.IsEmpty() ? PS->AuthenticatedUserId : PS->AuthenticatedSessionId;
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
        ++Index;Slot.bReady=false;GS->PlayerSlots.Add(Slot);
    }
    GS->ReadyPlayerCount=Ready;
    GS->bAllPlayersReady=Ready>=ExpectedCompetitivePlayers && GS->PlayerSlots.Num()>=ExpectedCompetitivePlayers;
    GS->ForceNetUpdate();
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
        if (auto* OnlineSession = Cast<ASPOnlineGameSession>(GameSession)) OnlineSession->EndProtocolSession();
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
    // Dedicated identities can only be promoted by one-time backend redemption.
    if(IsDedicatedAdmissionRequired() || !Player || SessionId.Len()<8) return false;
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
