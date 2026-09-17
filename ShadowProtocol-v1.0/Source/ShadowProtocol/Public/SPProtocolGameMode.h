#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SPTypes.h"
#include "SPProtocolGameMode.generated.h"

class ASPPlayerState;
class APlayerController;
class AActor;
class APlayerStart;
class ASPCharacter;
class ASPObjectiveSite;

UCLASS()
class SHADOWPROTOCOL_API ASPProtocolGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASPProtocolGameMode();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float PlanningDuration = 45.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float PreparationDuration = 8.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float RoundDuration = 900.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float PostRoundDuration = 8.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 RoundsToWin = 5;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 MaxRounds = 9;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 SideSwitchInterval = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 ExpectedCompetitivePlayers = 10;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float OvertimeDuration = 30.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float ReconnectGraceSeconds = 90.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bRequireAuthenticatedSessions = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float MaxAcceptedShotAgeSeconds = 0.25f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float FriendlyDamageScale = 0.35f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 ReverseFriendlyFireAfterIncidents = 2;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 DedicatedServerTickRate = 60;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<FSPSpawnGroup> SpawnGroups;

    UFUNCTION(BlueprintCallable) void BeginPreparation();
    UFUNCTION(BlueprintCallable) void BeginDeployment();
    UFUNCTION(BlueprintCallable) void StartNextRound();
    UFUNCTION(BlueprintCallable) void MarkObjectiveSecured(ASPPlayerState* Player);
    UFUNCTION(BlueprintCallable) void CompleteExtraction(ASPPlayerState* Player);
    void ApplyIntelligenceEffect(ESPIntelType Type, FName EffectTag, ASPPlayerState* Player);
    UFUNCTION(BlueprintCallable) void EvaluateEliminationWin();
    UFUNCTION(BlueprintCallable) void SetPlayerReady(ASPPlayerState* Player, bool bReady);
    UFUNCTION(BlueprintCallable) bool SelectSpawnGroup(ASPPlayerState* Player, FName SpawnGroupId);
    UFUNCTION(BlueprintPure) bool CanStartCompetitiveMatch() const;
    UFUNCTION(BlueprintCallable) bool AuthorizePlayerSession(ASPPlayerState* Player, const FString& SessionId);
    bool ValidateClientShotTimestamp(float ClientServerTimeSeconds) const;
    float ResolveCompetitiveDamage(ASPCharacter* Shooter, ASPCharacter* Victim, float RawDamage, bool& bOutReverseDamage);
    void RegisterDamageContribution(ASPCharacter* Shooter, ASPCharacter* Victim, float AppliedDamage);
    void RegisterElimination(ASPCharacter* Killer, ASPCharacter* Victim, bool bHeadshot);
protected:
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    virtual void Tick(float DeltaSeconds) override;
    void ResetRoundState();
    void FinishRound(ESPTeam Winner, const FString& Reason);
    void RotateSidesIfRequired();
    void RefreshCompetitiveSlots();
    void BeginOvertime();
    void UpdateMatchPointState();
    void SelectObjectiveSiteForRound();
    bool IsSpawnStartValid(const APlayerStart* Start, const ASPPlayerState* Player) const;
    void UpdateReconnectReservations();
    TMap<int32,float> ReconnectDeadlines;
    TMap<TWeakObjectPtr<ASPCharacter>,TMap<TWeakObjectPtr<ASPPlayerState>,float>> DamageLedger;
};
