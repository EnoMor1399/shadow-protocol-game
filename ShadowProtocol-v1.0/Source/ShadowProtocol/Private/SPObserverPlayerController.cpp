#include "SPControlsWidget.h"
#include "SPControlSettings.h"
#include "SPCharacter.h"
#include "GameFramework/PlayerInput.h"
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
        InputComponent->BindAction("Controls",IE_Pressed,this,&ASPObserverPlayerController::ToggleControls);
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
    DOREPLIFETIME_CONDITION(ASPObserverPlayerController,AvailableSpawnGroups,COND_OwnerOnly);
}

void ASPObserverPlayerController::BeginPlay()
{
    Super::BeginPlay();
    const auto* Settings = GetDefault<USPControlSettings>();
    SetMouseSensitivity(Settings->MouseSensitivity);
    bInvertMouseY = Settings->bInvertMouseY;
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
    if (HasAuthority())
    {
        SpawnChoicesRefreshRemaining -= DeltaTime;
        if (SpawnChoicesRefreshRemaining <= 0.f)
        {
            SpawnChoicesRefreshRemaining = 0.25f;
            const auto* Mode = GetWorld()->GetAuthGameMode<ASPProtocolGameMode>();
            const TArray<FName> Choices = Mode ? Mode->GetReadyRoomSpawnGroups(GetPlayerState<ASPPlayerState>()) : TArray<FName>();
            if (Choices != AvailableSpawnGroups) { AvailableSpawnGroups = Choices; ForceNetUpdate(); }
        }
    }
    if (IsLocalController() && PendingReadyRoomRequestId != 0 && FPlatformTime::Seconds() >= ReadyRoomRequestDeadline)
    {
        PendingReadyRoomRequestId = 0;
        ReadyRoomFeedback = TEXT("No confirmation received. Check your current selection before retrying.");
    }
    if (!IsLocalController()) return;
    const auto* GS = GetWorld()->GetGameState<ASPProtocolGameState>();
    const bool bShow = ReadyRoomWidget && GS && !GS->bMatchComplete && GS->MatchPhase == ESPMatchPhase::Planning;
    if (ReadyRoomWidget)
    {
        ReadyRoomWidget->SetVisibility(bShow && !bControlsOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        ReadyRoomRefreshRemaining -= DeltaTime;
        if (bShow && ReadyRoomRefreshRemaining <= 0.f)
        {
            ReadyRoomWidget->RefreshReadyRoom();
            ReadyRoomRefreshRemaining = 0.1f;
        }
    }
    if (bShow != bReadyRoomInputActive)
    {
        bReadyRoomInputActive = bShow;
        ApplyInterfaceInputMode();
    }
}

void ASPObserverPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsLocalController()) SaveControlSettings();
    if (ControlsWidget) ControlsWidget->RemoveFromParent();
    ControlsWidget = nullptr;
    if (ReadyRoomWidget) ReadyRoomWidget->RemoveFromParent();
    ReadyRoomWidget = nullptr;
    Super::EndPlay(EndPlayReason);
}

bool ASPObserverPlayerController::BeginReadyRoomRequest()
{
    const double Now = FPlatformTime::Seconds();
    if (!IsLocalController() || PendingReadyRoomRequestId != 0 || Now < NextLocalReadyRoomRequestSeconds) return false;
    NextLocalReadyRoomRequestSeconds = Now + 0.2;
    ReadyRoomRequestSequence = ReadyRoomRequestSequence == MAX_int32 ? 1 : ReadyRoomRequestSequence + 1;
    PendingReadyRoomRequestId = ReadyRoomRequestSequence;
    ReadyRoomRequestDeadline = Now + 4.0;
    ReadyRoomFeedback = TEXT("Waiting for server confirmation...");
    return true;
}

void ASPObserverPlayerController::RequestReadyState(bool bReady)
{
    if (BeginReadyRoomRequest()) ServerSetReadyState(bReady, PendingReadyRoomRequestId);
}

void ASPObserverPlayerController::RequestSpawnGroup(FName SpawnGroupId)
{
    if (BeginReadyRoomRequest()) ServerSelectSpawnGroup(SpawnGroupId, PendingReadyRoomRequestId);
}

void ASPObserverPlayerController::ClientReadyRoomResult_Implementation(int32 RequestId, bool bAccepted, const FString& Message)
{
    if (RequestId != PendingReadyRoomRequestId || RequestId == 0) return;
    PendingReadyRoomRequestId = 0;
    ReadyRoomFeedback = (bAccepted ? TEXT("") : TEXT("Not applied: ")) + Message;
}

bool ASPObserverPlayerController::ConsumeReadyRoomRequest()
{
    const double Now = FPlatformTime::Seconds();
    if (!HasAuthority() || Now < NextReadyRoomRequestSeconds) return false;
    NextReadyRoomRequestSeconds = Now + 0.1;
    return true;
}

void ASPObserverPlayerController::ServerSetReadyState_Implementation(bool bReady, int32 RequestId)
{
    if (!ConsumeReadyRoomRequest()) { ClientReadyRoomResult(RequestId, false, TEXT("Please wait before trying again.")); return; }
    auto* Mode = GetWorld()->GetAuthGameMode<ASPProtocolGameMode>();
    const bool bAccepted = Mode && Mode->SetPlayerReady(GetPlayerState<ASPPlayerState>(), bReady);
    ClientReadyRoomResult(RequestId, bAccepted, bAccepted
        ? (bReady ? TEXT("Ready confirmed.") : TEXT("Ready cancelled."))
        : TEXT("Readiness cannot change right now. Check admission and match phase."));
}

void ASPObserverPlayerController::ServerSelectSpawnGroup_Implementation(FName SpawnGroupId, int32 RequestId)
{
    if (!ConsumeReadyRoomRequest()) { ClientReadyRoomResult(RequestId, false, TEXT("Please wait before trying again.")); return; }
    auto* Mode = GetWorld()->GetAuthGameMode<ASPProtocolGameMode>();
    const bool bAccepted = Mode && Mode->SelectSpawnGroup(GetPlayerState<ASPPlayerState>(), SpawnGroupId);
    ClientReadyRoomResult(RequestId, bAccepted, bAccepted ? TEXT("Spawn selection confirmed. Check readiness before deployment.")
        : TEXT("That spawn is unavailable for your team or the planning phase has ended."));
}

void ASPObserverPlayerController::SetMouseSensitivity(float Value)
{
    MouseSensitivity = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.25f, 3.f) : 1.f;
}
void ASPObserverPlayerController::SaveControlSettings()
{
    auto* Settings = GetMutableDefault<USPControlSettings>();
    Settings->MouseSensitivity = MouseSensitivity;
    Settings->bInvertMouseY = bInvertMouseY;
    Settings->SaveConfig();
}
void ASPObserverPlayerController::ToggleControls()
{
    if (!IsLocalController()) return;
    if (!bControlsOpen)
    {
        ControlsWidget = CreateWidget<USPControlsWidget>(this, USPControlsWidget::StaticClass());
        if (!ControlsWidget) return;
        ControlsWidget->AddToPlayerScreen(100);
        bControlsOpen = true;
    }
    else
    {
        bControlsOpen = false;
        if (ControlsWidget) ControlsWidget->RemoveFromParent();
        ControlsWidget = nullptr;
        SaveControlSettings();
    }
    if (ReadyRoomWidget) ReadyRoomWidget->SetVisibility(bReadyRoomInputActive && !bControlsOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ApplyInterfaceInputMode();
}
void ASPObserverPlayerController::ApplyInterfaceInputMode()
{
    // Release hold actions before the UI starts consuming key-up events.
    if (auto* Character = Cast<ASPCharacter>(GetPawn())) Character->ReleaseHeldControls();
    if (PlayerInput) PlayerInput->FlushPressedKeys();
    const bool bModal = IsGameplayInputBlocked();
    // These are paired on every transition; do not accumulate the controller ignore counters.
    if (bInterfaceInputIgnored != bModal)
    {
        SetIgnoreMoveInput(bModal); SetIgnoreLookInput(bModal);
        bInterfaceInputIgnored = bModal;
    }
    bShowMouseCursor = bModal;
    UUserWidget* Focus = bControlsOpen ? static_cast<UUserWidget*>(ControlsWidget.Get()) : static_cast<UUserWidget*>(ReadyRoomWidget.Get());
    if (bModal && Focus)
    {
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(Focus->TakeWidget());
        SetInputMode(Mode);
        Focus->SetKeyboardFocus();
    }
    else SetInputMode(FInputModeGameOnly());
}
