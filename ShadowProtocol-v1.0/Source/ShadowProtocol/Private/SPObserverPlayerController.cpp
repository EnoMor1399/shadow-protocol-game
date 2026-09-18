#include "SPObserverPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Components/InputComponent.h"
#include "SPProtocolGameState.h"
#include "SPProtocolGameMode.h"
#include "SPPlayerState.h"
#include "SPReadyRoomWidget.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"

void ASPObserverPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if(InputComponent)
    {
        InputComponent->BindAction("ObserverNext",IE_Pressed,this,&ASPObserverPlayerController::CycleObserverNext);
        InputComponent->BindAction("ObserverFree",IE_Pressed,this,&ASPObserverPlayerController::ToggleFreeObserver);
    }
}
void ASPObserverPlayerController::CycleObserverNext(){ ServerCycleObserverTarget(1); }
void ASPObserverPlayerController::ToggleFreeObserver(){ ServerSetFreeObserverCamera(!bFreeObserverCamera); }

void ASPObserverPlayerController::ServerCycleObserverTarget_Implementation(int32 Direction)
{
    ObserverTargetIndex=FMath::Max(0,ObserverTargetIndex+(Direction>=0?1:-1));
    bFreeObserverCamera=false;
}
void ASPObserverPlayerController::ServerSetFreeObserverCamera_Implementation(bool bEnabled)
{
    const auto* GS=GetWorld() ? GetWorld()->GetGameState<ASPProtocolGameState>() : nullptr;
    const bool bLiveCompetitiveRound=GS && GS->bTeamOnlySpectating && (GS->RoundState==ESPRoundState::Action || GS->RoundState==ESPRoundState::Overtime);
    bFreeObserverCamera=bLiveCompetitiveRound ? false : bEnabled;
}
void ASPObserverPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPObserverPlayerController,bFreeObserverCamera);
    DOREPLIFETIME(ASPObserverPlayerController,ObserverTargetIndex);
}

void ASPObserverPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController() && GetNetMode() != NM_DedicatedServer && bShowNativeReadyRoom)
    {
        ReadyRoomWidget = CreateWidget<USPReadyRoomWidget>(this, USPReadyRoomWidget::StaticClass());
        if (ReadyRoomWidget)
        {
            ReadyRoomWidget->AddToPlayerScreen(20);
            ReadyRoomWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void ASPObserverPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!IsLocalController() || !ReadyRoomWidget) return;
    const auto* GS = GetWorld()->GetGameState<ASPProtocolGameState>();
    const bool bShow = GS && !GS->bMatchComplete && GS->MatchPhase == ESPMatchPhase::Planning;
    ReadyRoomWidget->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bShow) ReadyRoomWidget->RefreshReadyRoom();
    if (bShow != bReadyRoomInputActive)
    {
        bReadyRoomInputActive = bShow;
        bShowMouseCursor = bShow;
        if (bShow)
        {
            FInputModeUIOnly Mode;
            Mode.SetWidgetToFocus(ReadyRoomWidget->TakeWidget());
            SetInputMode(Mode);
        }
        else SetInputMode(FInputModeGameOnly());
    }
}

void ASPObserverPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ReadyRoomWidget) ReadyRoomWidget->RemoveFromParent();
    ReadyRoomWidget = nullptr;
    Super::EndPlay(EndPlayReason);
}

void ASPObserverPlayerController::RequestReadyState(bool bReady)
{
    const double Now = FPlatformTime::Seconds();
    if (!IsLocalController() || Now < NextLocalReadyRoomRequestSeconds) return;
    NextLocalReadyRoomRequestSeconds = Now + 0.2;
    ServerSetReadyState(bReady);
}

void ASPObserverPlayerController::RequestSpawnGroup(FName SpawnGroupId)
{
    const double Now = FPlatformTime::Seconds();
    if (!IsLocalController() || Now < NextLocalReadyRoomRequestSeconds) return;
    NextLocalReadyRoomRequestSeconds = Now + 0.2;
    ServerSelectSpawnGroup(SpawnGroupId);
}

bool ASPObserverPlayerController::ConsumeReadyRoomRequest()
{
    const double Now = FPlatformTime::Seconds();
    if (!HasAuthority() || Now < NextReadyRoomRequestSeconds) return false;
    NextReadyRoomRequestSeconds = Now + 0.1;
    return true;
}

void ASPObserverPlayerController::ServerSetReadyState_Implementation(bool bReady)
{
    if (!ConsumeReadyRoomRequest()) return;
    if (auto* Mode = GetWorld()->GetAuthGameMode<ASPProtocolGameMode>())
        Mode->SetPlayerReady(GetPlayerState<ASPPlayerState>(), bReady);
}

void ASPObserverPlayerController::ServerSelectSpawnGroup_Implementation(FName SpawnGroupId)
{
    if (!ConsumeReadyRoomRequest()) return;
    if (auto* Mode = GetWorld()->GetAuthGameMode<ASPProtocolGameMode>())
        Mode->SelectSpawnGroup(GetPlayerState<ASPPlayerState>(), SpawnGroupId);
}
