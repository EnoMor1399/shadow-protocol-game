#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SPTypes.h"
#include "SPDedicatedServerBackendSubsystem.h"
#include "SPProtocolGameMode.generated.h"

class ASPPlayerState;
class APlayerController;
class AActor;
class APlayerStart;
class ASPCharacter;
class ASPObjectiveSite;

struct FSPPendingPlayerAdmission
{
    TWeakObjectPtr<APlayerController> Controller;
    FString AllocationId;
    FString MatchId;
    FString ConnectToken;
    double DeadlineRealSeconds = 0.0;
    bool bRequestStarted = false;
};

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
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bRequireDedicatedServerAdmission = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float PendingAdmissionTimeoutSeconds = 12.f;
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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
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
    bool IsDedicatedAdmissionRequired() const;
    USPDedicatedServerBackendSubsystem* GetDedicatedServerBackend() const;
    bool FindPendingAdmissionForController(APlayerController* PlayerController, FString& OutAllocationId) const;
    void AssignCompetitiveTeam(ASPPlayerState* Player);
    void PromoteAdmittedPlayer(APlayerController* PlayerController, const FSPDedicatedServerAdmission& Admission);
    void RejectPendingAdmission(const FString& AllocationId, const FString& Reason);
    void DisconnectPlayer(APlayerController* PlayerController, const FString& Reason);
    void UpdatePendingAdmissions();
    UFUNCTION() void HandleBackendAdmissionCompleted(FSPDedicatedServerAdmission Admission);
    UFUNCTION() void HandleBackendAdmissionFailed(FString AllocationId, FString MatchId, FString ErrorMessage);

    TMap<FString,FSPPendingPlayerAdmission> PendingAdmissions;
    TMap<int32,float> ReconnectDeadlines;
    TMap<TWeakObjectPtr<ASPCharacter>,TMap<TWeakObjectPtr<ASPPlayerState>,float>> DamageLedger;
};
