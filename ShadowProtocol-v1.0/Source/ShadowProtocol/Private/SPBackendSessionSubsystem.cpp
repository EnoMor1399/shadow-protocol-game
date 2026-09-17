#include "SPBackendSessionSubsystem.h"

#include "SPBuildInfoLibrary.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
bool ParseJsonObject(const FString& JsonText, TSharedPtr<FJsonObject>& OutObject)
{
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
}
}

void USPBackendSessionSubsystem::SetBackendBaseUrl(const FString& InBaseUrl)
{
    FString Normalized = InBaseUrl;
    Normalized.TrimStartAndEndInline();
    while (Normalized.EndsWith(TEXT("/")))
    {
        Normalized.LeftChopInline(1);
    }

    if (!Normalized.IsEmpty())
    {
        BackendBaseUrl = Normalized;
    }
}

FString USPBackendSessionSubsystem::BuildUrl(const FString& Path) const
{
    if (Path.StartsWith(TEXT("/")))
    {
        return BackendBaseUrl + Path;
    }
    return BackendBaseUrl + TEXT("/") + Path;
}

void USPBackendSessionSubsystem::CheckCompatibility()
{
    bCompatibilityVerified = false;

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BuildUrl(TEXT("/v1/compatibility")));
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->OnProcessRequestComplete().BindUObject(this, &USPBackendSessionSubsystem::HandleCompatibilityResponse);

    if (!Request->ProcessRequest())
    {
        OnRequestFailed.Broadcast(TEXT("compatibility"), TEXT("Unable to start compatibility request."));
    }
}

void USPBackendSessionSubsystem::HandleCompatibilityResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        BroadcastHttpFailure(TEXT("compatibility"), Response, bWasSuccessful);
        return;
    }

    const int32 StatusCode = Response->GetResponseCode();
    if (StatusCode < 200 || StatusCode >= 300)
    {
        BroadcastHttpFailure(TEXT("compatibility"), Response, bWasSuccessful);
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    if (!ParseJsonObject(Response->GetContentAsString(), JsonObject))
    {
        OnRequestFailed.Broadcast(TEXT("compatibility"), TEXT("Backend returned invalid compatibility JSON."));
        return;
    }

    FSPBackendCompatibility Compatibility;
    if (!ParseCompatibility(JsonObject, Compatibility))
    {
        OnRequestFailed.Broadcast(TEXT("compatibility"), TEXT("Compatibility response is missing required build metadata."));
        return;
    }

    LastCompatibility = Compatibility;
    const FString LocalBuild = USPBuildInfoLibrary::GetNetworkBuildId();
    const bool bServerAcceptsLocal = Compatibility.AcceptedNetworkBuilds.IsEmpty()
        ? USPBuildInfoLibrary::IsNetworkBuildCompatible(Compatibility.NetworkBuild, true)
        : Compatibility.AcceptedNetworkBuilds.Contains(LocalBuild);
    bCompatibilityVerified = bServerAcceptsLocal;

    OnCompatibilityChecked.Broadcast(bCompatibilityVerified, LastCompatibility);

    if (!bCompatibilityVerified)
    {
        OnUpgradeRequired.Broadcast(Compatibility.NetworkBuild, Compatibility.BackendProtocol, TEXT("Client network build is not accepted by the backend."));
    }
}

bool USPBackendSessionSubsystem::ParseCompatibility(const TSharedPtr<FJsonObject>& JsonObject, FSPBackendCompatibility& OutCompatibility) const
{
    if (!JsonObject.IsValid())
    {
        return false;
    }

    const bool bHasRelease = JsonObject->TryGetStringField(TEXT("gameRelease"), OutCompatibility.GameRelease);
    const bool bHasNetworkBuild = JsonObject->TryGetStringField(TEXT("networkBuild"), OutCompatibility.NetworkBuild);
    const bool bHasContentRevision = JsonObject->TryGetStringField(TEXT("contentRevision"), OutCompatibility.ContentRevision);
    const bool bHasProtocol = JsonObject->TryGetStringField(TEXT("backendProtocol"), OutCompatibility.BackendProtocol);
    JsonObject->TryGetStringField(TEXT("enforcement"), OutCompatibility.Enforcement);

    const TArray<TSharedPtr<FJsonValue>>* AcceptedValues = nullptr;
    if (JsonObject->TryGetArrayField(TEXT("acceptedNetworkBuilds"), AcceptedValues) && AcceptedValues)
    {
        for (const TSharedPtr<FJsonValue>& Value : *AcceptedValues)
        {
            FString BuildId;
            if (Value.IsValid() && Value->TryGetString(BuildId) && !BuildId.IsEmpty())
            {
                OutCompatibility.AcceptedNetworkBuilds.Add(BuildId);
            }
        }
    }

    return bHasRelease && bHasNetworkBuild && bHasContentRevision && bHasProtocol;
}

void USPBackendSessionSubsystem::InstallAuthenticatedSession(const FString& InSessionId, const FString& InSessionToken, const FString& InRegion, const FString& InExpiresAt)
{
    if (InSessionId.IsEmpty() || InSessionToken.IsEmpty() || InRegion.IsEmpty())
    {
        OnRequestFailed.Broadcast(TEXT("session"), TEXT("Authenticated session data is incomplete."));
        return;
    }

    SessionId = InSessionId;
    SessionToken = InSessionToken;
    SessionRegion = InRegion;
    SessionExpiresAt = InExpiresAt;
}

void USPBackendSessionSubsystem::ClearAuthenticatedSession()
{
    SessionId.Reset();
    SessionToken.Reset();
    SessionRegion.Reset();
    SessionExpiresAt.Reset();
}

void USPBackendSessionSubsystem::AllocateProtocolServer(const FString& Region, bool bRanked)
{
    if (!bCompatibilityVerified)
    {
        OnRequestFailed.Broadcast(TEXT("allocation"), TEXT("Compatibility must be verified before server allocation."));
        return;
    }

    if (!HasAuthenticatedSession())
    {
        OnRequestFailed.Broadcast(TEXT("allocation"), TEXT("An authenticated game session is required before server allocation."));
        return;
    }

    const FString EffectiveRegion = Region.IsEmpty() ? SessionRegion : Region;
    if (EffectiveRegion.IsEmpty())
    {
        OnRequestFailed.Broadcast(TEXT("allocation"), TEXT("A matchmaking region is required."));
        return;
    }

    if (!SessionRegion.IsEmpty() && !EffectiveRegion.Equals(SessionRegion, ESearchCase::CaseSensitive))
    {
        OnRequestFailed.Broadcast(TEXT("allocation"), TEXT("Allocation region does not match the authenticated session region."));
        return;
    }

    bPendingRankedAllocation = bRanked;

    const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("region"), EffectiveRegion);
    Payload->SetStringField(TEXT("mode"), TEXT("PROTOCOL"));
    Payload->SetStringField(TEXT("map"), TEXT("EMBASSY"));
    Payload->SetBoolField(TEXT("ranked"), bRanked);

    FString Body;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
    FJsonSerializer::Serialize(Payload, Writer);

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BuildUrl(TEXT("/v1/matches/allocate")));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + SessionToken);
    Request->SetContentAsString(Body);
    Request->OnProcessRequestComplete().BindUObject(this, &USPBackendSessionSubsystem::HandleAllocationResponse);

    if (!Request->ProcessRequest())
    {
        OnRequestFailed.Broadcast(TEXT("allocation"), TEXT("Unable to start server allocation request."));
    }
}

void USPBackendSessionSubsystem::HandleAllocationResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        BroadcastHttpFailure(TEXT("allocation"), Response, bWasSuccessful);
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    const bool bParsedJson = ParseJsonObject(Response->GetContentAsString(), JsonObject);
    const int32 StatusCode = Response->GetResponseCode();

    if (StatusCode == 426)
    {
        HandleUpgradeResponse(JsonObject, TEXT("Backend rejected the current network build."));
        return;
    }

    if (StatusCode < 200 || StatusCode >= 300)
    {
        BroadcastHttpFailure(TEXT("allocation"), Response, bWasSuccessful);
        return;
    }

    if (!bParsedJson || !JsonObject.IsValid())
    {
        OnRequestFailed.Broadcast(TEXT("allocation"), TEXT("Backend returned invalid allocation JSON."));
        return;
    }

    FSPMatchAllocation Allocation;
    const bool bHasAllocation = JsonObject->TryGetStringField(TEXT("allocationId"), Allocation.AllocationId);
    const bool bHasMatch = JsonObject->TryGetStringField(TEXT("matchId"), Allocation.MatchId);
    const bool bHasServer = JsonObject->TryGetStringField(TEXT("serverId"), Allocation.ServerId);
    JsonObject->TryGetStringField(TEXT("region"), Allocation.Region);
    JsonObject->TryGetStringField(TEXT("connectToken"), Allocation.ConnectToken);
    JsonObject->TryGetStringField(TEXT("expiresAt"), Allocation.ExpiresAt);
    JsonObject->TryGetStringField(TEXT("networkBuild"), Allocation.NetworkBuild);
    JsonObject->TryGetStringField(TEXT("backendProtocol"), Allocation.BackendProtocol);

    double TickRate = 0.0;
    if (JsonObject->TryGetNumberField(TEXT("tickRate"), TickRate))
    {
        Allocation.TickRate = FMath::RoundToInt(TickRate);
    }

    if (!bHasAllocation || !bHasMatch || !bHasServer || Allocation.ConnectToken.IsEmpty())
    {
        OnRequestFailed.Broadcast(TEXT("allocation"), TEXT("Allocation response is missing required connection data."));
        return;
    }

    if (!USPBuildInfoLibrary::IsNetworkBuildCompatible(Allocation.NetworkBuild, bPendingRankedAllocation))
    {
        OnUpgradeRequired.Broadcast(Allocation.NetworkBuild, Allocation.BackendProtocol, TEXT("Allocated server build does not match the local client."));
        return;
    }

    OnAllocationCompleted.Broadcast(Allocation);
}

void USPBackendSessionSubsystem::BroadcastHttpFailure(const FString& Context, FHttpResponsePtr Response, bool bWasSuccessful)
{
    FString Message;
    if (!bWasSuccessful)
    {
        Message = TEXT("HTTP transport request failed.");
    }
    else if (Response.IsValid())
    {
        Message = FString::Printf(TEXT("HTTP %d: %s"), Response->GetResponseCode(), *Response->GetContentAsString());
    }
    else
    {
        Message = TEXT("Backend response was not available.");
    }

    OnRequestFailed.Broadcast(Context, Message);
}

void USPBackendSessionSubsystem::HandleUpgradeResponse(const TSharedPtr<FJsonObject>& JsonObject, const FString& FallbackReason)
{
    FString RequiredBuild = USPBuildInfoLibrary::GetNetworkBuildId();
    FString BackendProtocol;
    FString Reason = FallbackReason;

    if (JsonObject.IsValid())
    {
        JsonObject->TryGetStringField(TEXT("networkBuild"), RequiredBuild);
        JsonObject->TryGetStringField(TEXT("backendProtocol"), BackendProtocol);

        FString ErrorCode;
        if (JsonObject->TryGetStringField(TEXT("error"), ErrorCode) && !ErrorCode.IsEmpty())
        {
            Reason = ErrorCode;
        }
    }

    bCompatibilityVerified = false;
    OnUpgradeRequired.Broadcast(RequiredBuild, BackendProtocol, Reason);
}
