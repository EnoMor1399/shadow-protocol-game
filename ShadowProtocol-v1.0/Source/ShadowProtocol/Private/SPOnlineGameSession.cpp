#include "SPOnlineGameSession.h"
#include "SPBuildInfoLibrary.h"
#include "SPProtocolGameMode.h"
#include "SPDedicatedServerBackendSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

ASPOnlineGameSession::ASPOnlineGameSession()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    MaxPlayers = 10;
}

void ASPOnlineGameSession::BeginPlay()
{
    Super::BeginPlay();
    // GameModeBase does not drive AGameMode's match/session lifecycle.
    RegisterServer();
}

void ASPOnlineGameSession::RegisterServer()
{
    if (GetNetMode() != NM_DedicatedServer || bOwnsSession || bFailed || bClosing) return;
    Sessions = Online::GetSessionInterface(GetWorld());
    if (!Sessions.IsValid() || Sessions->GetNamedSession(NAME_GameSession))
    {
        // Never adopt or destroy a session owned by another world/session actor.
        FailSession();
        return;
    }

    FOnlineSessionSettings Settings;
    const auto* Mode = GetWorld()->GetAuthGameMode<ASPProtocolGameMode>();
    MaxPlayers = Mode ? FMath::Clamp(Mode->ExpectedCompetitivePlayers, 1, 10) : 10;
    Settings.NumPublicConnections = MaxPlayers;
    Settings.bIsDedicated = true;
    Settings.bShouldAdvertise = false;
    Settings.bAllowInvites = false;
    Settings.bUsesPresence = false;
    Settings.bAllowJoinViaPresence = false;
    Settings.bAllowJoinInProgress = true; // Reconnects still require backend admission.
    Settings.bUseLobbiesIfAvailable = false;
    Settings.Set(FName(TEXT("SP_NETWORK_BUILD")), USPBuildInfoLibrary::GetNetworkBuildId(),
        EOnlineDataAdvertisementType::DontAdvertise);

    CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateUObject(this, &ASPOnlineGameSession::OnCreated));
    bOwnsSession = true;
    BeginOperation(EOperation::Create);
    if (!Sessions->CreateSession(0, NAME_GameSession, Settings)) FailSession();
}

void ASPOnlineGameSession::BeginOperation(EOperation Next)
{
    Operation = Next;
    OperationDeadline = FPlatformTime::Seconds() + 15.0;
}

bool ASPOnlineGameSession::IsAcceptingAdmissions() const
{
    if (!Sessions.IsValid() || !bOwnsSession || bFailed || bClosing) return false;
    const auto State = Sessions->GetSessionState(NAME_GameSession);
    return State == EOnlineSessionState::Pending || State == EOnlineSessionState::Starting
        || State == EOnlineSessionState::InProgress;
}

bool ASPOnlineGameSession::StartProtocolSession()
{
    if (!IsAcceptingAdmissions()) return false;
    if (Sessions->GetSessionState(NAME_GameSession) == EOnlineSessionState::InProgress) return true;
    if (Operation != EOperation::None) return false;
    StartHandle = Sessions->AddOnStartSessionCompleteDelegate_Handle(
        FOnStartSessionCompleteDelegate::CreateUObject(this, &ASPOnlineGameSession::OnStarted));
    BeginOperation(EOperation::Start);
    if (!Sessions->StartSession(NAME_GameSession)) FailSession();
    // Do not advance the round until the provider has confirmed success.
    return !bFailed && Operation == EOperation::None
        && Sessions->GetSessionState(NAME_GameSession) == EOnlineSessionState::InProgress;
}

void ASPOnlineGameSession::EndProtocolSession()
{
    if (!Sessions.IsValid() || bFailed || bClosing || Operation != EOperation::None) return;
    if (Sessions->GetSessionState(NAME_GameSession) != EOnlineSessionState::InProgress) return;
    bClosing = true;
    EndHandle = Sessions->AddOnEndSessionCompleteDelegate_Handle(
        FOnEndSessionCompleteDelegate::CreateUObject(this, &ASPOnlineGameSession::OnEnded));
    BeginOperation(EOperation::End);
    if (!Sessions->EndSession(NAME_GameSession)) FailSession();
}

void ASPOnlineGameSession::OnCreated(FName Name, bool bSuccess)
{
    if (Name != NAME_GameSession || Operation != EOperation::Create || bFailed || bClosing) return;
    Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
    CreateHandle.Reset();
    Operation = EOperation::None;
    if (!bSuccess) FailSession();
}

void ASPOnlineGameSession::OnStarted(FName Name, bool bSuccess)
{
    if (Name != NAME_GameSession || Operation != EOperation::Start || bFailed || bClosing) return;
    Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartHandle);
    StartHandle.Reset();
    Operation = EOperation::None;
    if (!bSuccess) FailSession();
}

void ASPOnlineGameSession::OnEnded(FName Name, bool bSuccess)
{
    if (Name != NAME_GameSession || Operation != EOperation::End || bFailed) return;
    Sessions->ClearOnEndSessionCompleteDelegate_Handle(EndHandle);
    EndHandle.Reset();
    Operation = EOperation::None;
    if (!bSuccess) FailSession();
}

void ASPOnlineGameSession::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bFailed) return;
    if (Operation != EOperation::None && FPlatformTime::Seconds() >= OperationDeadline)
    {
        FailSession();
        return;
    }
    if (bOwnsSession && !bClosing && Operation == EOperation::None && !IsAcceptingAdmissions()) FailSession();
}

void ASPOnlineGameSession::ClearDelegates()
{
    if (!Sessions.IsValid()) return;
    Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
    Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartHandle);
    Sessions->ClearOnEndSessionCompleteDelegate_Handle(EndHandle);
    CreateHandle.Reset();
    StartHandle.Reset();
    EndHandle.Reset();
}

void ASPOnlineGameSession::FailSession()
{
    bFailed = true;
    Operation = EOperation::None;
    ClearDelegates();
    UE_LOG(LogTemp, Error, TEXT("Shadow Protocol online session unavailable; new admissions are blocked."));
    if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (auto* Backend = Instance->GetSubsystem<USPDedicatedServerBackendSubsystem>()) Backend->MarkDraining();
    }
}

void ASPOnlineGameSession::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bClosing = true;
    ClearDelegates();
    // Shutdown is best effort; process exit may precede a provider's asynchronous completion.
    if (Sessions.IsValid() && bOwnsSession && Sessions->GetNamedSession(NAME_GameSession))
        Sessions->DestroySession(NAME_GameSession);
    bOwnsSession = false;
    Sessions.Reset();
    Super::EndPlay(EndPlayReason);
}
