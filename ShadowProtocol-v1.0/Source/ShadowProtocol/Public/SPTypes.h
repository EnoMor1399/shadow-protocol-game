#pragma once
#include "CoreMinimal.h"
#include "SPTypes.generated.h"

UENUM(BlueprintType)
enum class ESPTeam : uint8 { None, DirectorateNine, Helix };

UENUM(BlueprintType)
enum class ESPTacticalRole : uint8 { Breacher, Recon, Tech, Support, Marksman };

UENUM(BlueprintType)
enum class ESPAwarenessState : uint8 { Patrol, Guard, Investigate, Search, Engage, Retreat };

UENUM(BlueprintType)
enum class ESPMatchPhase : uint8 {
    Planning, Preparation, Infiltration, IntelligenceSearch, ObjectiveIdentified,
    IntelligenceSecured, ExtractionActive, Spectating, RoundComplete, MatchComplete
};

UENUM(BlueprintType)
enum class ESPRoundState : uint8 { Waiting, Preparation, Action, Overtime, PostRound, Complete };

UENUM(BlueprintType)
enum class ESPIntelType : uint8 {
    CommunicationsTerminal, SecuritySystem, EncryptedDevice, SurveillanceData,
    AccessCard, DroneFeed, WeaponCache, ExtractionCoordinates, BuildingSchematic, ElectronicIntelligence
};

UENUM(BlueprintType)
enum class ESPBodyZone : uint8 { Head, Torso, LeftArm, RightArm, LeftLeg, RightLeg };

UENUM(BlueprintType)
enum class ESPScoreEvent : uint8 {
    Elimination, Assist, Objective, IntelligenceRecovery, Revive,
    Reconnaissance, Hack, Defense, CoordinatedAction, Extraction
};

UENUM(BlueprintType)
enum class ESPTacticalEquipment : uint8 { FlashGrenade, SmokeGrenade };

UENUM(BlueprintType)
enum class ESPObserverMode : uint8 { TeamFollow, FreeCamera };

UENUM(BlueprintType)
enum class ESPConnectionState : uint8 { Connected, Reconnecting, Disconnected };

UENUM(BlueprintType)
enum class ESPFriendlyFireState : uint8 { Normal, Warning, ReverseDamage };

UENUM(BlueprintType)
enum class ESPDoorState : uint8 { Closed, Peek, Open, Breached };

UENUM(BlueprintType)
enum class ESPSecurityDeviceType : uint8 { Camera, Light, AlarmPanel };

UENUM(BlueprintType)
enum class ESPOpticMode : uint8 { Reflex1x, Magnifier2x };

USTRUCT(BlueprintType)
struct FSPCompetitivePlayerSlot
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SlotIndex = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerId = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESPTeam Team = ESPTeam::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SpawnGroup = "ALPHA";
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Callsign;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SessionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bReady = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESPConnectionState ConnectionState = ESPConnectionState::Connected;
};

USTRUCT(BlueprintType)
struct FSPSpawnGroup
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName GroupId = "Default";
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESPTeam Team = ESPTeam::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTransform> SpawnTransforms;
};

UENUM(BlueprintType)
enum class ESPSquadOrder : uint8 { Follow, Hold, Assault };

USTRUCT(BlueprintType)
struct FSPTacticalMarker
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector WorldLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Label;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MarkerType = "Route";
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 OwnerPlayerId = -1;
};
