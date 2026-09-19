#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SPTypes.h"
#include "SPHeldLeanState.h"
#include "SPCharacter.generated.h"

class USPHealthComponent;
class UCameraComponent;
class ASPWeaponBase;
class USPTacticalEquipmentComponent;
class ASPBarricade;
class USPLagCompensationComponent;
class USPCombatFeedbackComponent;
class USPCoverSystemComponent;

UCLASS()
class SHADOWPROTOCOL_API ASPCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ASPCharacter();
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<UCameraComponent> FirstPersonCamera;
    void ReleaseHeldControls();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USPHealthComponent> Health;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USPTacticalEquipmentComponent> TacticalEquipment;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USPLagCompensationComponent> LagCompensation;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USPCombatFeedbackComponent> CombatFeedback;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USPCoverSystemComponent> CoverSystem;
    UPROPERTY(Replicated, BlueprintReadOnly) TObjectPtr<ASPWeaponBase> EquippedWeapon;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bSilentMovement = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bAiming = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bSprinting = false;
    UPROPERTY(Replicated, BlueprintReadOnly) float Stamina = 100.f;
    UPROPERTY(Replicated, BlueprintReadOnly) float LeanAlpha = 0.f;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bVaulting = false;
    UPROPERTY(Replicated, BlueprintReadOnly) ESPOpticMode OpticMode = ESPOpticMode::Reflex1x;
    UPROPERTY(Replicated, BlueprintReadOnly) ESPSquadOrder SquadOrder = ESPSquadOrder::Follow;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 BarricadesRemaining = 3;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fortification") TSubclassOf<ASPBarricade> BarricadeClass;

    UFUNCTION(Server, Reliable) void ServerSetSilentMovement(bool bEnabled);
    UFUNCTION(Server, Reliable) void ServerFire(float ClientServerTimeSeconds);
    UFUNCTION(Server, Reliable) void ServerReload();
    UFUNCTION(Server, Reliable) void ServerSetAiming(bool bEnabled);
    UFUNCTION(Server, Reliable) void ServerSetSprinting(bool bEnabled);
    UFUNCTION(Server, Reliable) void ServerCycleSquadOrder();
    UFUNCTION(Server, Reliable) void ServerFortify();
    UFUNCTION(Server, Unreliable) void ServerSetLean(float Value);
    UFUNCTION(Server, Reliable) void ServerVault();
    UFUNCTION(Server, Reliable) void ServerCycleOptic();
    UFUNCTION(BlueprintImplementableEvent, Category="Weapon") void BP_InspectWeapon();
    UFUNCTION(BlueprintImplementableEvent, Category="Combat") void BP_OnSuppressed(float Intensity);

protected:
    void MoveForward(float V); void MoveRight(float V); void Turn(float V); void LookUp(float V);
    void Fire(); void Reload(); void BeginSilent(); void EndSilent();
    void BeginAim(); void EndAim(); void BeginSprint(); void EndSprint();
    void CycleEquipment(); void ThrowEquipment(); void CycleSquadOrder(); void Fortify();
    void BeginLeanLeft(); void EndLeanLeft(); void BeginLeanRight(); void EndLeanRight(); void Vault(); void CycleOptic(); void InspectWeapon();
    void UpdateCoverStateAuthority();
    bool CanUseLocalControls() const;
    FSPHeldLeanState HeldLean;
    void UpdateHeldLean();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
