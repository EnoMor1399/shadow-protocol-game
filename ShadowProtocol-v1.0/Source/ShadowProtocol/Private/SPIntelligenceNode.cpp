#include "SPIntelligenceNode.h"
#include "SPProtocolGameMode.h"
#include "SPPlayerState.h"
#include "Net/UnrealNetwork.h"

ASPIntelligenceNode::ASPIntelligenceNode(){ bReplicates=true; }
void ASPIntelligenceNode::ServerCapture_Implementation(ASPPlayerState* CapturingPlayer)
{
    if(!HasAuthority() || (!bRepeatable && bCaptured) || !CapturingPlayer) return;
    bCaptured=true;
    CapturingPlayer->AddScoreEvent(ESPScoreEvent::IntelligenceRecovery);
    if(ASPProtocolGameMode* GM=GetWorld()->GetAuthGameMode<ASPProtocolGameMode>()) GM->ApplyIntelligenceEffect(IntelType, EffectTag, CapturingPlayer);
    BP_OnIntelCaptured(EffectTag);
}
void ASPIntelligenceNode::OnRep_Captured(){ if(bCaptured) BP_OnIntelCaptured(EffectTag); }
void ASPIntelligenceNode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ASPIntelligenceNode,bCaptured); }
