#include "SPCharacter.h"
#include "SPHealthComponent.h"
#include "SPWeaponBase.h"
#include "SPTacticalEquipmentComponent.h"
#include "SPBarricade.h"
#include "SPPlayerState.h"
#include "SPLagCompensationComponent.h"
#include "SPCombatFeedbackComponent.h"
#include "SPCoverSystemComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

ASPCharacter::ASPCharacter()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;
    Health = CreateDefaultSubobject<USPHealthComponent>(TEXT("Health"));
    TacticalEquipment = CreateDefaultSubobject<USPTacticalEquipmentComponent>(TEXT("TacticalEquipment"));
    LagCompensation = CreateDefaultSubobject<USPLagCompensationComponent>(TEXT("LagCompensation"));
    CombatFeedback = CreateDefaultSubobject<USPCombatFeedbackComponent>(TEXT("CombatFeedback"));
    CoverSystem = CreateDefaultSubobject<USPCoverSystemComponent>(TEXT("CoverSystem"));
    GetCharacterMovement()->MaxWalkSpeed = 420.f;
    GetCharacterMovement()->MaxWalkSpeedCrouched = 180.f;
}

void ASPCharacter::SetupPlayerInputComponent(UInputComponent* IC)
{
    Super::SetupPlayerInputComponent(IC);
    IC->BindAxis("MoveForward", this, &ASPCharacter::MoveForward);
    IC->BindAxis("MoveRight", this, &ASPCharacter::MoveRight);
    IC->BindAxis("Turn", this, &ASPCharacter::Turn);
    IC->BindAxis("LookUp", this, &ASPCharacter::LookUp);
    IC->BindAction("Fire", IE_Pressed, this, &ASPCharacter::Fire);
    IC->BindAction("Reload", IE_Pressed, this, &ASPCharacter::Reload);
    IC->BindAction("Crouch", IE_Pressed, this, &ASPCharacter::BeginSilent);
    IC->BindAction("Crouch", IE_Released, this, &ASPCharacter::EndSilent);
    IC->BindAction("Aim", IE_Pressed, this, &ASPCharacter::BeginAim);
    IC->BindAction("Aim", IE_Released, this, &ASPCharacter::EndAim);
    IC->BindAction("Sprint", IE_Pressed, this, &ASPCharacter::BeginSprint);
    IC->BindAction("Sprint", IE_Released, this, &ASPCharacter::EndSprint);
    IC->BindAction("CycleEquipment", IE_Pressed, this, &ASPCharacter::CycleEquipment);
    IC->BindAction("ThrowEquipment", IE_Pressed, this, &ASPCharacter::ThrowEquipment);
    IC->BindAction("SquadOrder", IE_Pressed, this, &ASPCharacter::CycleSquadOrder);
    IC->BindAction("Fortify", IE_Pressed, this, &ASPCharacter::Fortify);
    IC->BindAction("LeanLeft", IE_Pressed, this, &ASPCharacter::BeginLeanLeft);
    IC->BindAction("LeanLeft", IE_Released, this, &ASPCharacter::EndLeanLeft);
    IC->BindAction("LeanRight", IE_Pressed, this, &ASPCharacter::BeginLeanRight);
    IC->BindAction("LeanRight", IE_Released, this, &ASPCharacter::EndLeanRight);
    IC->BindAction("Vault", IE_Pressed, this, &ASPCharacter::Vault);
    IC->BindAction("CycleOptic", IE_Pressed, this, &ASPCharacter::CycleOptic);
    IC->BindAction("InspectWeapon", IE_Pressed, this, &ASPCharacter::InspectWeapon);
}

void ASPCharacter::MoveForward(float V){ if(V!=0.f) AddMovementInput(GetActorForwardVector(),V * (Health?Health->LegSpeedMultiplier:1.f)); }
void ASPCharacter::MoveRight(float V){ if(V!=0.f) AddMovementInput(GetActorRightVector(),V * (Health?Health->LegSpeedMultiplier:1.f)); }
void ASPCharacter::Turn(float V){ AddControllerYawInput(V); }
void ASPCharacter::LookUp(float V){ AddControllerPitchInput(V); }
void ASPCharacter::Fire(){ const AGameStateBase* GS=GetWorld()?GetWorld()->GetGameState():nullptr; const float ShotTime=GS?GS->GetServerWorldTimeSeconds():(GetWorld()?GetWorld()->GetTimeSeconds():0.f); ServerFire(ShotTime); }
void ASPCharacter::Reload(){ ServerReload(); }
void ASPCharacter::BeginSilent(){ Crouch(); ServerSetSilentMovement(true); }
void ASPCharacter::EndSilent(){ UnCrouch(); ServerSetSilentMovement(false); }
void ASPCharacter::ServerSetSilentMovement_Implementation(bool bEnabled){ bSilentMovement=bEnabled; }
void ASPCharacter::ServerFire_Implementation(float ClientServerTimeSeconds){ if(EquippedWeapon) EquippedWeapon->ServerTryFire(this,ClientServerTimeSeconds); }
void ASPCharacter::ServerReload_Implementation(){ if(EquippedWeapon) EquippedWeapon->ServerReload(); }
void ASPCharacter::BeginAim(){ bAiming=true; bSprinting=false; GetCharacterMovement()->MaxWalkSpeed=420.f; ServerSetAiming(true); ServerSetSprinting(false); }
void ASPCharacter::EndAim(){ bAiming=false; ServerSetAiming(false); }
void ASPCharacter::BeginSprint(){ if(Stamina<=2.f || bSilentMovement || bAiming) return; bSprinting=true; GetCharacterMovement()->MaxWalkSpeed=620.f; ServerSetSprinting(true); }
void ASPCharacter::EndSprint(){ bSprinting=false; GetCharacterMovement()->MaxWalkSpeed=420.f; ServerSetSprinting(false); }
void ASPCharacter::CycleEquipment(){ if(TacticalEquipment) TacticalEquipment->CycleEquipment(); }
void ASPCharacter::ThrowEquipment(){ if(!TacticalEquipment) return; const FVector Target=GetActorLocation()+GetControlRotation().Vector()*800.f; TacticalEquipment->DeploySelected(Target); }
void ASPCharacter::CycleSquadOrder(){ ServerCycleSquadOrder(); }
void ASPCharacter::Fortify(){ ServerFortify(); }
void ASPCharacter::BeginLeanLeft(){ LeanAlpha=-1.f; ServerSetLean(-1.f); }
void ASPCharacter::EndLeanLeft(){ LeanAlpha=0.f; ServerSetLean(0.f); }
void ASPCharacter::BeginLeanRight(){ LeanAlpha=1.f; ServerSetLean(1.f); }
void ASPCharacter::EndLeanRight(){ LeanAlpha=0.f; ServerSetLean(0.f); }
void ASPCharacter::Vault(){ ServerVault(); }
void ASPCharacter::CycleOptic(){ ServerCycleOptic(); }
void ASPCharacter::InspectWeapon(){ if(!bSprinting) BP_InspectWeapon(); }

void ASPCharacter::UpdateCoverStateAuthority()
{
    if(!HasAuthority() || !CoverSystem || !GetWorld()) return;
    const FVector Start=GetActorLocation()+FVector(0,0,62.f);
    const FVector End=Start+GetActorForwardVector()*92.f;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SPCoverProbe),false,this);
    FHitResult Hit;
    const bool bHit=GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Params);
    if(bHit) CoverSystem->ServerSetCoverState(true,Hit.ImpactNormal);
    else CoverSystem->ServerSetCoverState(false,FVector::ZeroVector);
}

void ASPCharacter::ServerSetLean_Implementation(float Value)
{
    LeanAlpha=FMath::Clamp(Value,-1.f,1.f);
    UpdateCoverStateAuthority();
    if(CoverSystem) CoverSystem->ServerSetPeekAlpha(LeanAlpha);
}
void ASPCharacter::ServerCycleOptic_Implementation(){ OpticMode=OpticMode==ESPOpticMode::Reflex1x?ESPOpticMode::Magnifier2x:ESPOpticMode::Reflex1x; }
void ASPCharacter::ServerVault_Implementation(){
    if(bVaulting || bSprinting) return;
    const FVector Start=GetActorLocation()+FVector(0,0,45.f),Forward=GetActorForwardVector(),LowEnd=Start+Forward*120.f,HighStart=GetActorLocation()+FVector(0,0,135.f),HighEnd=HighStart+Forward*120.f;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SPVault),false,this);FHitResult LowHit,HighHit;const bool bLow=GetWorld()->LineTraceSingleByChannel(LowHit,Start,LowEnd,ECC_Visibility,Params);const bool bHigh=GetWorld()->LineTraceSingleByChannel(HighHit,HighStart,HighEnd,ECC_Visibility,Params);
    if(!bLow || bHigh) return;bVaulting=true;LaunchCharacter(Forward*320.f+FVector(0,0,230.f),true,true);FTimerHandle H;GetWorldTimerManager().SetTimer(H,[this](){bVaulting=false;},.55f,false);
}
void ASPCharacter::ServerSetAiming_Implementation(bool bEnabled){ bAiming=bEnabled; if(bAiming){bSprinting=false;GetCharacterMovement()->MaxWalkSpeed=420.f;} }
void ASPCharacter::ServerSetSprinting_Implementation(bool bEnabled){ bSprinting=bEnabled && Stamina>2.f && !bSilentMovement && !bAiming; GetCharacterMovement()->MaxWalkSpeed=bSprinting?620.f:420.f; }
void ASPCharacter::ServerCycleSquadOrder_Implementation(){ SquadOrder = SquadOrder==ESPSquadOrder::Follow ? ESPSquadOrder::Hold : SquadOrder==ESPSquadOrder::Hold ? ESPSquadOrder::Assault : ESPSquadOrder::Follow; }
void ASPCharacter::ServerFortify_Implementation()
{
    if(!BarricadeClass || BarricadesRemaining<=0) return;
    const FVector Start=GetActorLocation()+FVector(0,0,45.f),End=Start+GetActorForwardVector()*180.f;
    FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(SPFortify),false,this);
    if(!GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Params)) return;
    AActor* Point=Hit.GetActor(); if(!Point || !Point->ActorHasTag(TEXT("SP_FortifyPoint"))) return;
    const FRotator Rotation=Point->GetActorRotation();
    FActorSpawnParameters Spawn; Spawn.Owner=this; Spawn.Instigator=this;
    if(ASPBarricade* Barricade=GetWorld()->SpawnActor<ASPBarricade>(BarricadeClass,Point->GetActorLocation(),Rotation,Spawn))
    {
        ESPTeam Team=ESPTeam::None; if(const ASPPlayerState* PS=GetPlayerState<ASPPlayerState>()) Team=PS->Team;
        Barricade->ServerDeploy(Team); --BarricadesRemaining;
    }
}
void ASPCharacter::Tick(float DT){ Super::Tick(DT); if(!HasAuthority()) return; if(bSprinting){Stamina=FMath::Max(0.f,Stamina-DT*22.f);if(Stamina<=0.f){bSprinting=false;GetCharacterMovement()->MaxWalkSpeed=420.f;}}else Stamina=FMath::Min(100.f,Stamina+DT*10.f); }

void ASPCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPCharacter, EquippedWeapon);
    DOREPLIFETIME(ASPCharacter, bSilentMovement);
    DOREPLIFETIME(ASPCharacter, bAiming);
    DOREPLIFETIME(ASPCharacter, bSprinting);
    DOREPLIFETIME(ASPCharacter, Stamina);
    DOREPLIFETIME(ASPCharacter, LeanAlpha);
    DOREPLIFETIME(ASPCharacter, bVaulting);
    DOREPLIFETIME(ASPCharacter, OpticMode);
    DOREPLIFETIME(ASPCharacter, SquadOrder);
    DOREPLIFETIME(ASPCharacter, BarricadesRemaining);
}
