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

FString SerializeJson(const TSharedRef<FJsonObject>& Payload)
{
    FString Body;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
    FJsonSerializer::Serialize(Payload, Writer);
    return Body;
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

bool USPBackendSessionSubsystem::CanUseAuthenticatedMatchEndpoint(const FString& Context)
{
    if (!bCompatibilityVerified)
    {
        OnRequestFailed.Broadcast(Context, TEXT("Compatibility must be verified before using authenticated match services."));
        return false;
    }

    if (!HasAuthenticatedSession())
    {
        OnRequestFailed.Broadcast(Context, TEXT("An authenticated game session is required."));
        return false;
    }

    return true;
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> USPBackendSessionSubsystem::CreateAuthenticatedJsonRequest(const FString& Path, const FString& Verb)
{
    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BuildUrl(Path));
    Request->SetVerb(Verb);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + SessionToken);
    return Request;
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

void USPBackendSessionSubsystem::HandleCompatibilityResponse(FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
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

void USPBackendSessionSubsystem::RefreshAuthenticatedSession()
{
    if (!CanUseAuthenticatedMatchEndpoint(TEXT("session-refresh")))
    {
        return;
    }

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateAuthenticatedJsonRequest(TEXT("/v1/auth/refresh"), TEXT("POST"));
    Request->SetContentAsString(TEXT("{}"));
    Request->OnProcessRequestComplete().BindUObject(this, &USPBackendSessionSubsystem::HandleSessionRefreshResponse);

    if (!Request->ProcessRequest())
    {
        OnRequestFailed.Broadcast(TEXT("session-refresh"), TEXT("Unable to start game-session refresh request."));
    }
}

void USPBackendSessionSubsystem::HandleSessionRefreshResponse(FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        BroadcastHttpFailure(TEXT("session-refresh"), Response, bWasSuccessful);
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    const bool bParsedJson = ParseJsonObject(Response->GetContentAsString(), JsonObject);
    const int32 StatusCode = Response->GetResponseCode();

    if (StatusCode == 426)
    {
        HandleUpgradeResponse(JsonObject, TEXT("Backend rejected session refresh because the client build is no longer compatible."));
        return;
    }

    if (StatusCode == 401 || StatusCode == 403)
    {
        ClearAuthenticatedSession();
        BroadcastHttpFailure(TEXT("session-refresh"), Response, bWasSuccessful);
        return;
    }

    if (StatusCode < 200 || StatusCode >= 300)
    {
        BroadcastHttpFailure(TEXT("session-refresh"), Response, bWasSuccessful);
        return;
    }

    if (!bParsedJson || !JsonObject.IsValid())
    {
        OnRequestFailed.Broadcast(TEXT("session-refresh"), TEXT("Backend returned invalid session refresh JSON."));
        return;
    }

    FString RefreshedSessionId;
    FString RefreshedToken;
    FString RefreshedRegion;
    FString RefreshedExpiresAt;
    JsonObject->TryGetStringField(TEXT("sessionId"), RefreshedSessionId);
    JsonObject->TryGetStringField(TEXT("sessionToken"), RefreshedToken);
    JsonObject->TryGetStringField(TEXT("region"), RefreshedRegion);
    JsonObject->TryGetStringField(TEXT("expiresAt"), RefreshedExpiresAt);

    if (RefreshedSessionId.IsEmpty() || RefreshedToken.IsEmpty() || RefreshedExpiresAt.IsEmpty())
    {
        OnRequestFailed.Broadcast(TEXT("session-refresh"), TEXT("Session refresh response is missing required token data."));
        return;
    }

    if (!SessionId.IsEmpty() && !RefreshedSessionId.Equals(SessionId, ESearchCase::CaseSensitive))
    {
        ClearAuthenticatedSession();
        OnRequestFailed.Broadcast(TEXT("session-refresh"), TEXT("Backend returned a different session identity during token rotation."));
        return;
    }

    SessionId = RefreshedSessionId;
    SessionToken = RefreshedToken;
    SessionRegion = RefreshedRegion.IsEmpty() ? SessionRegion : RefreshedRegion;
    SessionExpiresAt = RefreshedExpiresAt;
    OnSessionRefreshed.Broadcast(SessionId, SessionExpiresAt);
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
    if (!CanUseAuthenticatedMatchEndpoint(TEXT("allocation")))
    {
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

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateAuthenticatedJsonRequest(TEXT("/v1/matches/allocate"), TEXT("POST"));
    Request->SetContentAsString(SerializeJson(Payload));
    Request->OnProcessRequestComplete().BindUObject(this, &USPBackendSessionSubsystem::HandleAllocationResponse);

    if (!Request->ProcessRequest())
    {
        OnRequestFailed.Broadcast(TEXT("allocation"), TEXT("Unable to start server allocation request."));
    }
}

void USPBackendSessionSubsystem::HandleAllocationResponse(FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
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

void USPBackendSessionSubsystem::RequestReconnectTicket(const FString& MatchId, int32 RoundNumber, int32 SlotIndex)
{
    if (!CanUseAuthenticatedMatchEndpoint(TEXT("reconnect-ticket")))
    {
        return;
    }

    if (MatchId.IsEmpty() || RoundNumber < 1 || RoundNumber > 9 || SlotIndex < 0 || SlotIndex > 9)
    {
        OnRequestFailed.Broadcast(TEXT("reconnect-ticket"), TEXT("Reconnect ticket request contains invalid match, round or slot data."));
        return;
    }

    const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("matchId"), MatchId);
    Payload->SetNumberField(TEXT("roundNumber"), RoundNumber);
    Payload->SetNumberField(TEXT("slotIndex"), SlotIndex);

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateAuthenticatedJsonRequest(TEXT("/v1/matches/reconnect-ticket"), TEXT("POST"));
    Request->SetContentAsString(SerializeJson(Payload));
    Request->OnProcessRequestComplete().BindUObject(this, &USPBackendSessionSubsystem::HandleReconnectTicketResponse);

    if (!Request->ProcessRequest())
    {
        OnRequestFailed.Broadcast(TEXT("reconnect-ticket"), TEXT("Unable to start reconnect ticket request."));
    }
}

void USPBackendSessionSubsystem::HandleReconnectTicketResponse(FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        BroadcastHttpFailure(TEXT("reconnect-ticket"), Response, bWasSuccessful);
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    const bool bParsedJson = ParseJsonObject(Response->GetContentAsString(), JsonObject);
    const int32 StatusCode = Response->GetResponseCode();

    if (StatusCode == 426)
    {
        HandleUpgradeResponse(JsonObject, TEXT("Backend rejected reconnect because the client build is no longer compatible."));
        return;
    }

    if (StatusCode < 200 || StatusCode >= 300)
    {
        BroadcastHttpFailure(TEXT("reconnect-ticket"), Response, bWasSuccessful);
        return;
    }

    if (!bParsedJson || !JsonObject.IsValid())
    {
        OnRequestFailed.Broadcast(TEXT("reconnect-ticket"), TEXT("Backend returned invalid reconnect ticket JSON."));
        return;
    }

    FString ReconnectToken;
    FString ReconnectDeadline;
    FString NetworkBuild;
    FString BackendProtocol;
    double GraceSeconds = 0.0;
    JsonObject->TryGetStringField(TEXT("reconnectToken"), ReconnectToken);
    JsonObject->TryGetStringField(TEXT("reconnectDeadline"), ReconnectDeadline);
    JsonObject->TryGetStringField(TEXT("networkBuild"), NetworkBuild);
    JsonObject->TryGetStringField(TEXT("backendProtocol"), BackendProtocol);
    JsonObject->TryGetNumberField(TEXT("graceSeconds"), GraceSeconds);

    if (ReconnectToken.IsEmpty() || ReconnectDeadline.IsEmpty())
    {
        OnRequestFailed.Broadcast(TEXT("reconnect-ticket"), TEXT("Reconnect ticket response is missing token or deadline data."));
        return;
    }

    if (!NetworkBuild.IsEmpty() && !USPBuildInfoLibrary::IsNetworkBuildCompatible(NetworkBuild, true))
    {
        OnUpgradeRequired.Broadcast(NetworkBuild, BackendProtocol, TEXT("Reconnect reservation targets an incompatible network build."));
        return;
    }

    OnReconnectTicketIssued.Broadcast(ReconnectToken, ReconnectDeadline, FMath::RoundToInt(GraceSeconds));
}

void USPBackendSessionSubsystem::ReconnectToReservedSlot(const FString& MatchId, int32 RoundNumber, int32 SlotIndex, const FString& ReconnectToken)
{
    if (!CanUseAuthenticatedMatchEndpoint(TEXT("reconnect")))
    {
        return;
    }

    if (MatchId.IsEmpty() || RoundNumber < 1 || RoundNumber > 9 || SlotIndex < 0 || SlotIndex > 9 || ReconnectToken.IsEmpty())
    {
        OnRequestFailed.Broadcast(TEXT("reconnect"), TEXT("Reconnect request contains invalid match, round, slot or token data."));
        return;
    }

    const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("matchId"), MatchId);
    Payload->SetNumberField(TEXT("roundNumber"), RoundNumber);
    Payload->SetNumberField(TEXT("slotIndex"), SlotIndex);
    Payload->SetStringField(TEXT("reconnectToken"), ReconnectToken);

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateAuthenticatedJsonRequest(TEXT("/v1/matches/reconnect"), TEXT("POST"));
    Request->SetContentAsString(SerializeJson(Payload));
    Request->OnProcessRequestComplete().BindUObject(this, &USPBackendSessionSubsystem::HandleReconnectResponse);

    if (!Request->ProcessRequest())
    {
        OnRequestFailed.Broadcast(TEXT("reconnect"), TEXT("Unable to start reconnect request."));
    }
}

void USPBackendSessionSubsystem::HandleReconnectResponse(FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        BroadcastHttpFailure(TEXT("reconnect"), Response, bWasSuccessful);
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    const bool bParsedJson = ParseJsonObject(Response->GetContentAsString(), JsonObject);
    const int32 StatusCode = Response->GetResponseCode();

    if (StatusCode == 426)
    {
        HandleUpgradeResponse(JsonObject, TEXT("Backend rejected reconnect because the client build is no longer compatible."));
        return;
    }

    if (StatusCode < 200 || StatusCode >= 300)
    {
        BroadcastHttpFailure(TEXT("reconnect"), Response, bWasSuccessful);
        return;
    }

    if (!bParsedJson || !JsonObject.IsValid())
    {
        OnRequestFailed.Broadcast(TEXT("reconnect"), TEXT("Backend returned invalid reconnect JSON."));
        return;
    }

    FSPReconnectResult Result;
    JsonObject->TryGetBoolField(TEXT("reconnected"), Result.bReconnected);
    JsonObject->TryGetStringField(TEXT("networkBuild"), Result.NetworkBuild);
    JsonObject->TryGetStringField(TEXT("backendProtocol"), Result.BackendProtocol);

    if (JsonObject->HasTypedField<EJson::Object>(TEXT("slot")))
    {
        const TSharedPtr<FJsonObject> SlotObject = JsonObject->GetObjectField(TEXT("slot"));
        if (SlotObject.IsValid())
        {
            double SlotIndex = -1.0;
            if (SlotObject->TryGetNumberField(TEXT("slot_index"), SlotIndex))
            {
                Result.SlotIndex = FMath::RoundToInt(SlotIndex);
            }
            SlotObject->TryGetStringField(TEXT("user_id"), Result.UserId);
            SlotObject->TryGetStringField(TEXT("team"), Result.Team);
            SlotObject->TryGetStringField(TEXT("spawn_group"), Result.SpawnGroup);
        }
    }

    if (!Result.bReconnected)
    {
        OnRequestFailed.Broadcast(TEXT("reconnect"), TEXT("Backend response did not confirm reconnection."));
        return;
    }

    if (!Result.NetworkBuild.IsEmpty() && !USPBuildInfoLibrary::IsNetworkBuildCompatible(Result.NetworkBuild, true))
    {
        OnUpgradeRequired.Broadcast(Result.NetworkBuild, Result.BackendProtocol, TEXT("Reconnected slot targets an incompatible network build."));
        return;
    }

    OnReconnectCompleted.Broadcast(Result);
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
