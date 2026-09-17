#include "SPWeaponBase.h"
#include "SPCharacter.h"
#include "SPHealthComponent.h"
#include "SPLagCompensationComponent.h"
#include "SPProtocolGameMode.h"
#include "SPPlayerState.h"
#include "SPTypes.h"
#include "SPCombatFeedbackComponent.h"
#include "SPSecurityDevice.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"

ASPWeaponBase::ASPWeaponBase(){ bReplicates=true; SetReplicateMovement(true); AmmoInMagazine=MagazineSize; }

void ASPWeaponBase::ServerTryFire(ASPCharacter* Shooter,float ClientServerTimeSeconds)
{
    if(!HasAuthority() || !Shooter || AmmoInMagazine<=0) return;
    ASPProtocolGameMode* GM=GetWorld()?GetWorld()->GetAuthGameMode<ASPProtocolGameMode>():nullptr;
    if(GM && !GM->ValidateClientShotTimestamp(ClientServerTimeSeconds)) return;

    --AmmoInMagazine;
    if(Shooter->CombatFeedback) Shooter->CombatFeedback->MulticastWeaponFired(false);
    FVector Start; FRotator ViewRot;
    Shooter->GetActorEyesViewPoint(Start, ViewRot);
    const float Stability = Shooter->Health ? Shooter->Health->ArmStabilityMultiplier : 1.f;
    const float AimMultiplier = Shooter->bAiming ? 0.45f : 1.f;
    const float SprintMultiplier = Shooter->bSprinting ? 1.8f : 1.f;
    const float Spread = (AccuracySpreadDegrees / FMath::Max(Stability, 0.2f)) * AimMultiplier * SprintMultiplier;
    const FVector Dir = FMath::VRandCone(ViewRot.Vector(), FMath::DegreesToRadians(Spread));
    const FVector End = Start + Dir*Range;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SPWeaponFire), true, Shooter);
    TArray<FHitResult> Hits;
    GetWorld()->LineTraceMultiByChannel(Hits,Start,End,ECC_Visibility,Params);

    ASPCharacter* CharacterHit=nullptr;
    ESPBodyZone HitZone=ESPBodyZone::Torso;
    bool bHeadshot=false;
    float DamageScale=1.f;
    int32 PenetrationsUsed=0;
    TSet<AActor*> DamagedActors;

    for(const FHitResult& Hit : Hits)
    {
        if(ASPSecurityDevice* Device=Cast<ASPSecurityDevice>(Hit.GetActor()))
        {
            Device->ApplyAuthoritativeDamage(Damage*DamageScale,Shooter->GetController());
            break;
        }
        if(ASPCharacter* Victim=Cast<ASPCharacter>(Hit.GetActor()))
        {
            if(DamagedActors.Contains(Victim)) continue;
            DamagedActors.Add(Victim);
            CharacterHit=Victim;
            const FString Bone=Hit.BoneName.ToString().ToLower();
            if(Bone.Contains("head")){HitZone=ESPBodyZone::Head;bHeadshot=true;}
            else if(Bone.Contains("arm") || Bone.Contains("hand")) HitZone=ESPBodyZone::LeftArm;
            else if(Bone.Contains("leg") || Bone.Contains("foot")) HitZone=ESPBodyZone::LeftLeg;
            break;
        }
        const AActor* SurfaceActor=Hit.GetActor();
        const bool bPenetrable=SurfaceActor && SurfaceActor->ActorHasTag(TEXT("SP_Penetrable"));
        if(!bPenetrable || PenetrationsUsed>=MaxPenetrations) break;
        ++PenetrationsUsed;
        DamageScale*=PenetrationDamageMultiplier;
    }

    if(!CharacterHit && ClientServerTimeSeconds>0.f)
    {
        float BestDistance=LagCompensationRadius;
        for(TActorIterator<ASPCharacter> It(GetWorld()); It; ++It)
        {
            ASPCharacter* Candidate=*It;
            if(!Candidate || Candidate==Shooter || !Candidate->Health || !Candidate->Health->IsAlive() || !Candidate->LagCompensation) continue;
            const FVector Rewound=Candidate->LagCompensation->SampleLocationAt(ClientServerTimeSeconds)+FVector(0,0,50.f);
            const FVector Closest=FMath::ClosestPointOnSegment(Rewound,Start,End);
            const float D=FVector::Dist(Rewound,Closest);
            if(D>=BestDistance) continue;
            FCollisionQueryParams VisibilityParams(SCENE_QUERY_STAT(SPLagVisibility),true,Shooter);
            VisibilityParams.AddIgnoredActor(Candidate);
            FHitResult Block;
            const bool bBlocked=GetWorld()->LineTraceSingleByChannel(Block,Start,Rewound,ECC_Visibility,VisibilityParams);
            if(bBlocked && Block.GetActor() && !Block.GetActor()->ActorHasTag(TEXT("SP_Penetrable"))) continue;
            CharacterHit=Candidate;HitZone=ESPBodyZone::Torso;bHeadshot=false;BestDistance=D;
        }
    }

    if(CharacterHit && CharacterHit->Health)
    {
        const float ZoneMultiplier=bHeadshot?2.4f:1.f;
        const float RawDamage=Damage*DamageScale*ZoneMultiplier;
        bool bReverse=false;
        const float AppliedDamage=GM?GM->ResolveCompetitiveDamage(Shooter,CharacterHit,RawDamage,bReverse):RawDamage;
        ASPCharacter* DamageTarget=bReverse?Shooter:CharacterHit;
        if(DamageTarget && DamageTarget->Health)
        {
            const bool bWasAlive=DamageTarget->Health->IsAlive();
            DamageTarget->Health->ApplyLocalizedDamage(AppliedDamage,bReverse?ESPBodyZone::Torso:HitZone,false);
            if(GM && !bReverse)
            {
                GM->RegisterDamageContribution(Shooter,CharacterHit,AppliedDamage);
                if(bWasAlive && !CharacterHit->Health->IsAlive()) GM->RegisterElimination(Shooter,CharacterHit,bHeadshot);
            }
        }
    }

    for(TActorIterator<ASPCharacter> It(GetWorld()); It; ++It)
    {
        ASPCharacter* Other=*It; if(!Other || Other==Shooter || !Other->Health || !Other->Health->IsAlive()) continue;
        const FVector P=Other->GetActorLocation();
        const FVector Closest=FMath::ClosestPointOnSegment(P,Start,End);
        const float D=FVector::Dist(P,Closest);
        if(D<=SuppressionRadius){const float Intensity=1.f-FMath::Clamp(D/SuppressionRadius,0.f,1.f);Other->BP_OnSuppressed(Intensity);if(Other->CombatFeedback) Other->CombatFeedback->MulticastSuppression(Intensity);}
    }
}
void ASPWeaponBase::ServerReload(){ if(HasAuthority()) AmmoInMagazine=MagazineSize; }
void ASPWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ASPWeaponBase, AmmoInMagazine);
}
