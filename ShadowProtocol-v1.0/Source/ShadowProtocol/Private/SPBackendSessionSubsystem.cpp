#include "SPBackendSessionSubsystem.h"

#include "SPBuildInfoLibrary.h"
#include "SPTravelValidation.h"
#include "Containers/StringConv.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "GameFramework/PlayerController.h"
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

void USPBackendSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (GEngine)
    {
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &USPBackendSessionSubsystem::HandleNetworkFailure);
        TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &USPBackendSessionSubsystem::HandleTravelFailure);
    }
    ExpiryTicker = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &USPBackendSessionSubsystem::TickSessionExpiry), 1.0f);
}

void USPBackendSessionSubsystem::Deinitialize()
{
    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    }
    NetworkFailureHandle.Reset();
    TravelFailureHandle.Reset();
    FTSTicker::GetCoreTicker().RemoveTicker(ExpiryTicker);
    ExpiryTicker.Reset();
    ClearAuthenticatedSession();
    if (CompatibilityRequest.IsValid())
    {
        CompatibilityRequest->OnProcessRequestComplete().Unbind();
        CompatibilityRequest->CancelRequest();
        CompatibilityRequest.Reset();
    }
    Super::Deinitialize();
}

void USPBackendSessionSubsystem::ReportConnectionFailure(const FString& Context, const FString& Message)
{
    LastConnectionError = Message;
    OnRequestFailed.Broadcast(Context, Message);
}

void USPBackendSessionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver*, ENetworkFailure::Type, const FString&)
{
    if (!World || World->GetGameInstance() != GetGameInstance()
        || (World->GetNetMode() != NM_Client && World->GetNetMode() != NM_Standalone)) return;
    // Engine error strings can contain the token-bearing travel URL. Never relay them.
    ReportConnectionFailure(TEXT("network"), TEXT("Connection to the game server was lost. Return to matchmaking or use a fresh authorized reconnect."));
}

void USPBackendSessionSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type, const FString&)
{
    if (!World || World->GetGameInstance() != GetGameInstance()
        || (World->GetNetMode() != NM_Client && World->GetNetMode() != NM_Standalone)) return;
    ReportConnectionFailure(TEXT("travel"), TEXT("Unable to enter the game server. Request a fresh allocation before trying again."));
}

bool USPBackendSessionSubsystem::SetSessionExpiry(const FString& ExpiresAt)
{
    FDateTime Parsed;
    if (!FDateTime::ParseIso8601(*ExpiresAt, Parsed)) return false;
    const double Remaining = (Parsed - FDateTime::UtcNow()).GetTotalSeconds();
    if (Remaining <= 0.0) return false;
    SessionExpiryUtc = Parsed;
    SessionExpiryMonotonic = FPlatformTime::Seconds() + Remaining;
    SessionExpiresAt = ExpiresAt;
    return true;
}

float USPBackendSessionSubsystem::GetSessionSecondsRemaining() const
{
    if (SessionToken.IsEmpty()) return 0.0f;
    // A backwards wall-clock adjustment must not extend the installed lifetime.
    const double Remaining = FMath::Min((SessionExpiryUtc - FDateTime::UtcNow()).GetTotalSeconds(),
        SessionExpiryMonotonic - FPlatformTime::Seconds());
    return static_cast<float>(FMath::Max(0.0, Remaining));
}

bool USPBackendSessionSubsystem::TickSessionExpiry(float)
{
    if (!SessionToken.IsEmpty() && !HasAuthenticatedSession()) ExpireSession();
    return true;
}

void USPBackendSessionSubsystem::ExpireSession()
{
    if (SessionToken.IsEmpty()) return;
    ClearAuthenticatedSession();
    bSessionExpired = true;
    OnSessionExpired.Broadcast(TEXT("Your backend session expired. Sign in again for matchmaking or reconnect."));
}

void USPBackendSessionSubsystem::CancelAuthenticatedRequests()
{
    // Remove membership first, including for providers which complete during cancellation.
    TArray<FHttpRequestPtr> Requests = MoveTemp(ActiveAuthenticatedRequests);
    ActiveAuthenticatedRequests.Reset();
    for (const FHttpRequestPtr& Request : Requests)
    {
        if (!Request.IsValid()) continue;
        Request->OnProcessRequestComplete().Unbind();
        Request->CancelRequest();
    }
    bRefreshPending = false;
}

bool USPBackendSessionSubsystem::ConsumeAuthenticatedResponse(FHttpRequestPtr Request)
{
    if (!Request.IsValid() || ActiveAuthenticatedRequests.Remove(Request) == 0) return false;
    if (!HasAuthenticatedSession())
    {
        ExpireSession();
        return false;
    }
    return Request->GetHeader(TEXT("Authorization")) == TEXT("Bearer ") + SessionToken;
}

void USPBackendSessionSubsystem::SetBackendBaseUrl(const FString& InBaseUrl)
{
    FString Normalized = InBaseUrl;
    Normalized.TrimStartAndEndInline();
    while (Normalized.EndsWith(TEXT("/")))
    {
        Normalized.LeftChopInline(1);
    }

    if (!Normalized.IsEmpty() && Normalized != BackendBaseUrl)
    {
        ClearAuthenticatedSession();
        bCompatibilityVerified = false;
        LastCompatibility = FSPBackendCompatibility();
        if (CompatibilityRequest.IsValid())
        {
            CompatibilityRequest->OnProcessRequestComplete().Unbind();
            CompatibilityRequest->CancelRequest();
            CompatibilityRequest.Reset();
        }
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
    if (bRefreshPending)
    {
        OnRequestFailed.Broadcast(Context, TEXT("Session refresh is in progress. Retry after it completes."));
        return false;
    }
    if (!bCompatibilityVerified)
    {
        OnRequestFailed.Broadcast(Context, TEXT("Compatibility must be verified before using authenticated match services."));
        return false;
    }

    if (!HasAuthenticatedSession())
    {
        ExpireSession();
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
    Request->SetTimeout(15.0f);
    ActiveAuthenticatedRequests.Add(Request);
    return Request;
}

void USPBackendSessionSubsystem::CheckCompatibility()
{
    bCompatibilityVerified = false;
    if (CompatibilityRequest.IsValid())
    {
        CompatibilityRequest->OnProcessRequestComplete().Unbind();
        CompatibilityRequest->CancelRequest();
    }

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    CompatibilityRequest = Request;
    Request->SetTimeout(15.0f);
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
    if (Request != CompatibilityRequest) return;
    CompatibilityRequest.Reset();
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
    ClearAuthenticatedSession();
    if (InSessionId.IsEmpty() || InSessionToken.IsEmpty() || InRegion.IsEmpty() || !SetSessionExpiry(InExpiresAt))
    {
        OnRequestFailed.Broadcast(TEXT("session"), TEXT("Authenticated session data is incomplete or expired."));
        return;
    }

    SessionId = InSessionId;
    SessionToken = InSessionToken;
    SessionRegion = InRegion;
}

void USPBackendSessionSubsystem::RefreshAuthenticatedSession()
{
    if (bRefreshPending) return;
    if (!ActiveAuthenticatedRequests.IsEmpty())
    {
        OnRequestFailed.Broadcast(TEXT("session-refresh"), TEXT("Wait for active match requests before refreshing the session."));
        return;
    }
    if (!CanUseAuthenticatedMatchEndpoint(TEXT("session-refresh")))
    {
        return;
    }

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateAuthenticatedJsonRequest(TEXT("/v1/auth/refresh"), TEXT("POST"));
    bRefreshPending = true;
    Request->SetContentAsString(TEXT("{}"));
    Request->OnProcessRequestComplete().BindUObject(this, &USPBackendSessionSubsystem::HandleSessionRefreshResponse);

    if (!Request->ProcessRequest())
    {
        ActiveAuthenticatedRequests.Remove(Request);
        bRefreshPending = false;
        OnRequestFailed.Broadcast(TEXT("session-refresh"), TEXT("Unable to start game-session refresh request."));
    }
}

void USPBackendSessionSubsystem::HandleSessionRefreshResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!ConsumeAuthenticatedResponse(Request)) return;
    bRefreshPending = false;
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

    if (!SetSessionExpiry(RefreshedExpiresAt))
    {
        ExpireSession();
        OnRequestFailed.Broadcast(TEXT("session-refresh"), TEXT("Backend returned an invalid or expired session lifetime."));
        return;
    }
    // Token rotation invalidates requests still carrying the previous credential.
    CancelAuthenticatedRequests();
    SessionId = RefreshedSessionId;
    SessionToken = RefreshedToken;
    SessionRegion = RefreshedRegion.IsEmpty() ? SessionRegion : RefreshedRegion;
    SessionExpiresAt = RefreshedExpiresAt;
    OnSessionRefreshed.Broadcast(SessionId, SessionExpiresAt);
}

void USPBackendSessionSubsystem::ClearAuthenticatedSession()
{
    CancelAuthenticatedRequests();
    SessionExpiryMonotonic = 0.0;
    SessionExpiryUtc = FDateTime();
    bSessionExpired = false;
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
        ActiveAuthenticatedRequests.Remove(Request);
        OnRequestFailed.Broadcast(TEXT("allocation"), TEXT("Unable to start server allocation request."));
    }
}

void USPBackendSessionSubsystem::HandleAllocationResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!ConsumeAuthenticatedResponse(Request)) return;
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
    const bool bHasConnectHost = JsonObject->TryGetStringField(TEXT("connectHost"), Allocation.ConnectHost);
    JsonObject->TryGetStringField(TEXT("region"), Allocation.Region);
    JsonObject->TryGetStringField(TEXT("connectToken"), Allocation.ConnectToken);
    JsonObject->TryGetStringField(TEXT("expiresAt"), Allocation.ExpiresAt);
    JsonObject->TryGetStringField(TEXT("networkBuild"), Allocation.NetworkBuild);
    JsonObject->TryGetStringField(TEXT("backendProtocol"), Allocation.BackendProtocol);

    JsonObject->TryGetStringField(TEXT("reconnectGrantId"), Allocation.ReconnectGrantId);
    FGuid ReconnectGuid;
    if (Request->GetURL().EndsWith(TEXT("/reconnect-allocation"))
        && !FGuid::Parse(Allocation.ReconnectGrantId, ReconnectGuid))
    {
        OnRequestFailed.Broadcast(TEXT("reconnect-allocation"), TEXT("Missing reconnect admission grant."));
        return;
    }

    double TickRate = 0.0;
    if (JsonObject->TryGetNumberField(TEXT("tickRate"), TickRate))
    {
        Allocation.TickRate = FMath::RoundToInt(TickRate);
    }

    double ConnectPort = 0.0;
    const bool bHasConnectPort = JsonObject->TryGetNumberField(TEXT("connectPort"), ConnectPort);
    if (bHasConnectPort && FMath::IsFinite(ConnectPort) && ConnectPort >= 1.0 && ConnectPort <= 65535.0
        && ConnectPort == static_cast<double>(static_cast<int32>(ConnectPort)))
    {
        Allocation.ConnectPort = static_cast<int32>(ConnectPort);
    }

    if (!bHasAllocation || !bHasMatch || !bHasServer || Allocation.ConnectToken.IsEmpty() || !bHasConnectHost || Allocation.ConnectHost.IsEmpty() || !bHasConnectPort || Allocation.ConnectPort < 1 || Allocation.ConnectPort > 65535)
    {
        OnRequestFailed.Broadcast(TEXT("allocation"), TEXT("Allocation response is missing a valid server connection target."));
        return;
    }

    if (!USPBuildInfoLibrary::IsNetworkBuildCompatible(Allocation.NetworkBuild, bPendingRankedAllocation))
    {
        OnUpgradeRequired.Broadcast(Allocation.NetworkBuild, Allocation.BackendProtocol, TEXT("Allocated server build does not match the local client."));
        return;
    }

    if (BuildAllocationTravelUrl(Allocation).IsEmpty())
    {
        OnRequestFailed.Broadcast(TEXT("allocation"), TEXT("The backend returned an invalid or expired connection target."));
        return;
    }
    OnAllocationCompleted.Broadcast(Allocation);
}

FString USPBackendSessionSubsystem::BuildAllocationTravelUrl(const FSPMatchAllocation& Allocation) const
{
    FGuid AllocationGuid;
    FGuid MatchGuid;
    FGuid ReconnectGuid;
    if (!Allocation.ReconnectGrantId.IsEmpty()
        && (!FGuid::Parse(Allocation.ReconnectGrantId, ReconnectGuid)
            || Allocation.ReconnectGrantId != ReconnectGuid.ToString(EGuidFormats::DigitsWithHyphens))) return FString();
    FDateTime AllocationExpiry;
    const FTCHARToUTF8 HostUtf8(*Allocation.ConnectHost);
    if (!SPTravelValidation::IsValidHost(std::string_view(HostUtf8.Get(), HostUtf8.Length()))
        || !FDateTime::ParseIso8601(*Allocation.ExpiresAt, AllocationExpiry)
        || AllocationExpiry <= FDateTime::UtcNow()
        || !USPBuildInfoLibrary::IsNetworkBuildCompatible(Allocation.NetworkBuild, true)) return FString();
    const auto IsSafeOption = [](const FString& Value)
    {
        if (Value.IsEmpty()) return false;
        for (int32 Index = 0; Index < Value.Len(); ++Index)
        {
            const TCHAR Character = Value[Index];
            if (!FChar::IsAlnum(Character)
                && Character != TEXT('-')
                && Character != TEXT('_')
                && Character != TEXT('.'))
            {
                return false;
            }
        }
        return true;
    };

    if (!FGuid::Parse(Allocation.AllocationId, AllocationGuid)
        || !FGuid::Parse(Allocation.MatchId, MatchGuid)
        || Allocation.ConnectHost.IsEmpty()
        || Allocation.ConnectPort < 1
        || Allocation.ConnectPort > 65535
        || Allocation.ConnectToken.Len() < 24
        || Allocation.ConnectToken.Len() > 256
        || !IsSafeOption(Allocation.ConnectToken)
        || !IsSafeOption(Allocation.ServerId)
        || !IsSafeOption(Allocation.NetworkBuild))
    {
        return FString();
    }

    FString Host = Allocation.ConnectHost.TrimStartAndEnd();
    if (Host.Contains(TEXT(":")) && !Host.StartsWith(TEXT("[")) && !Host.EndsWith(TEXT("]")))
    {
        Host = TEXT("[") + Host + TEXT("]");
    }

    FString TravelUrl = FString::Printf(
        TEXT("%s:%d?spAllocationId=%s?spMatchId=%s?spConnectToken=%s?spServerId=%s?spNetworkBuild=%s"),
        *Host,
        Allocation.ConnectPort,
        *Allocation.AllocationId,
        *Allocation.MatchId,
        *Allocation.ConnectToken,
        *Allocation.ServerId,
        *Allocation.NetworkBuild);
    if (!Allocation.ReconnectGrantId.IsEmpty()) TravelUrl += TEXT("?spReconnectGrantId=") + Allocation.ReconnectGrantId;
    return TravelUrl;
}

bool USPBackendSessionSubsystem::ConnectToAllocation(APlayerController* PlayerController, const FSPMatchAllocation& Allocation)
{
    if (!CanUseAuthenticatedMatchEndpoint(TEXT("travel"))) return false;
    if (!PlayerController || !PlayerController->IsLocalController()
        || PlayerController->GetGameInstance() != GetGameInstance())
    {
        return false;
    }

    const FString TravelUrl = BuildAllocationTravelUrl(Allocation);
    if (TravelUrl.IsEmpty())
    {
        ReportConnectionFailure(TEXT("travel"), TEXT("The server allocation is invalid, expired or incompatible. Request a fresh allocation."));
        return false;
    }

    ClearConnectionError();
    PlayerController->ClientTravel(TravelUrl, TRAVEL_Absolute);
    return true;
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
        ActiveAuthenticatedRequests.Remove(Request);
        OnRequestFailed.Broadcast(TEXT("reconnect-ticket"), TEXT("Unable to start reconnect ticket request."));
    }
}

void USPBackendSessionSubsystem::HandleReconnectTicketResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!ConsumeAuthenticatedResponse(Request)) return;
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

void USPBackendSessionSubsystem::RequestReconnectAllocation(const FString& MatchId, int32 RoundNumber, int32 SlotIndex, const FString& ReconnectToken)
{
    if (!CanUseAuthenticatedMatchEndpoint(TEXT("reconnect-allocation")))
    {
        return;
    }

    if (MatchId.IsEmpty() || RoundNumber < 1 || RoundNumber > 9 || SlotIndex < 0 || SlotIndex > 9 || ReconnectToken.IsEmpty())
    {
        OnRequestFailed.Broadcast(TEXT("reconnect-allocation"), TEXT("Reconnect request contains invalid match, round, slot or token data."));
        return;
    }

    const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("matchId"), MatchId);
    Payload->SetNumberField(TEXT("roundNumber"), RoundNumber);
    Payload->SetNumberField(TEXT("slotIndex"), SlotIndex);
    Payload->SetStringField(TEXT("reconnectToken"), ReconnectToken);

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateAuthenticatedJsonRequest(TEXT("/v1/matches/reconnect-allocation"), TEXT("POST"));
    Request->SetContentAsString(SerializeJson(Payload));
    Request->OnProcessRequestComplete().BindUObject(this, &USPBackendSessionSubsystem::HandleAllocationResponse);

    if (!Request->ProcessRequest())
    {
        ActiveAuthenticatedRequests.Remove(Request);
        OnRequestFailed.Broadcast(TEXT("reconnect-allocation"), TEXT("Unable to start reconnect request."));
    }
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
        ActiveAuthenticatedRequests.Remove(Request);
        OnRequestFailed.Broadcast(TEXT("reconnect"), TEXT("Unable to start reconnect request."));
    }
}

void USPBackendSessionSubsystem::HandleReconnectResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!ConsumeAuthenticatedResponse(Request)) return;
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
    if (Context != TEXT("compatibility") && Response.IsValid()
        && (Response->GetResponseCode() == 401
            || (Context == TEXT("session-refresh") && Response->GetResponseCode() == 403))) ExpireSession();
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
