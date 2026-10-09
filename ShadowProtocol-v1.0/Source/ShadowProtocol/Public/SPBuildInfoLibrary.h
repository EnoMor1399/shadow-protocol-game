#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SPBuildInfoLibrary.generated.h"

/**
 * Single source of build identity for menus, session handshakes and diagnostics.
 * Ranked compatibility is intentionally exact-match until a server-driven
 * compatibility policy is introduced.
 */
UCLASS()
class SHADOWPROTOCOL_API USPBuildInfoLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Build")
    static FString GetReleaseVersion();

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Build")
    static FString GetNetworkBuildId();

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Build")
    static FString GetContentRevision();

    UFUNCTION(BlueprintPure, Category="Shadow Protocol|Build")
    static bool IsNetworkBuildCompatible(const FString& RemoteBuildId, bool bRankedMatch = true);
};
