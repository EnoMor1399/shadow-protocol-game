#include "SPObserverPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Components/InputComponent.h"
#include "SPProtocolGameState.h"
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
