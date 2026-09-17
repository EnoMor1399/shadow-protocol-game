#include "SPDedicatedServerBackendSubsystem.h"

#include "SPBuildInfoLibrary.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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

FString NormalizeBaseUrl(FString Value)
{
    Value.TrimStartAndEndInline();
    while (Value.EndsWith(TEXT("/")))
    {
        Value.LeftChopInline(1);
    }
    return Value;
}
}

void USPDedicatedServerBackendSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (IsRunningDedicatedServer() && ConfigureFromRuntime())
    {
        RegisterNode();
    }
}

void USPDedicatedServerBackendSubsystem::Deinitialize()
{
    StopHeartbeat();
    SendBestEffortShutdownDrain();
    MatchServerSecret.Empty();
    Super::Deinitialize();
}

bool USPDedicatedServerBackendSubsystem::ConfigureFromRuntime()
{
    bConfigured = false;

    if (!IsRunningDedicatedServer())
    {
        return false;
    }

    FString ParsedValue;
    if (FParse::Value(FCommandLine::Get(), TEXT("SPBackendUrl="), ParsedValue))
    {
        BackendBaseUrl = NormalizeBaseUrl(ParsedValue);
    }
    if (FParse::Value(FCommandLine::Get(), TEXT("SPServerId="), ParsedValue))
    {
        ServerId = ParsedValue.TrimStartAndEnd();
    }
    if (FParse::Value(FCommandLine::Get(), TEXT("SPRegion="), ParsedValue))
    {
        Region = ParsedValue.TrimStartAndEnd();
    }
    if (FParse::Value(FCommandLine::Get(), TEXT("SPPublicHost="), ParsedValue))
    {
        PublicHost = ParsedValue.TrimStartAndEnd();
    }
    if (FParse::Value(FCommandLine::Get(), TEXT("SPPublicPort="), ParsedValue))
    {
        PublicPort = FCString::Atoi(*ParsedValue);
    }
    if (FParse::Value(FCommandLine::Get(), TEXT("SPCapacity="), ParsedValue))
    {
        Capacity = FMath::Clamp(FCString::Atoi(*ParsedValue), 1, 128);
    }

    MatchServerSecret = FPlatformMisc::GetEnvironmentVariable(TEXT("MATCH_SERVER_SECRET"));
    MatchServerSecret.TrimStartAndEndInline();

    BackendBaseUrl = NormalizeBaseUrl(BackendBaseUrl);
    bConfigured = HasRequiredConfiguration();

    if (!bConfigured)
    {
        OnRequestFailed.Broadcast(
            TEXT("dedicated-server-config"),
            TEXT("Dedicated-server backend configuration is incomplete. Required: SPBackendUrl, SPServerId, SPRegion, SPPublicHost, SPPublicPort and MATCH_SERVER_SECRET."));
    }

    return bConfigured;
}

bool USPDedicatedServerBackendSubsystem::HasRequiredConfiguration() const
{
    return IsRunningDedicatedServer()
        && !BackendBaseUrl.IsEmpty()
        && !ServerId.IsEmpty()
        && !Region.IsEmpty()
        && !PublicHost.IsEmpty()
        && PublicPort > 0
        && PublicPort <= 65535
        && Capacity > 0
        && Capacity <= 128
        && !MatchServerSecret.IsEmpty();
}

FString USPDedicatedServerBackendSubsystem::BuildUrl(const FString& Path) const
{
    return Path.StartsWith(TEXT("/")) ? BackendBaseUrl + Path : BackendBaseUrl + TEXT("/") + Path;
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> USPDedicatedServerBackendSubsystem::CreateInfrastructureJsonRequest(const FString& Path, const FString& Verb) const
{
    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BuildUrl(Path));
    Request->SetVerb(Verb);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetHeader(TEXT("x-match-server-secret"), MatchServerSecret);
    return Request;
}

void USPDedicatedServerBackendSubsystem::RegisterNode()
{
    if (!bConfigured && !ConfigureFromRuntime())
    {
        return;
    }

    const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("serverId"), ServerId);
    Payload->SetStringField(TEXT("region"), Region);
    Payload->SetStringField(TEXT("networkBuild"), USPBuildInfoLibrary::GetNetworkBuildId());
    Payload->SetStringField(TEXT("publicHost"), PublicHost);
    Payload->SetNumberField(TEXT("publicPort"), PublicPort);
    Payload->SetNumberField(TEXT("capacity"), Capacity);

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateInfrastructureJsonRequest(TEXT("/v1/servers/register"), TEXT("POST"));
    Request->SetContentAsString(SerializeJson(Payload));
    Request->OnProcessRequestComplete().BindUObject(this, &USPDedicatedServerBackendSubsystem::HandleRegistrationResponse);

    if (!Request->ProcessRequest())
    {
        OnRequestFailed.Broadcast(TEXT("server-register"), TEXT("Unable to start dedicated-server registration request."));
    }
}

void USPDedicatedServerBackendSubsystem::HandleRegistrationResponse(FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
    {
        BroadcastHttpFailure(TEXT("server-register"), Response, bWasSuccessful);
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    if (!ParseJsonObject(Response->GetContentAsString(), JsonObject))
    {
        OnRequestFailed.Broadcast(TEXT("server-register"), TEXT("Backend returned invalid server-registration JSON."));
        return;
    }

    FSPDedicatedServerRegistration Registration;
    JsonObject->TryGetStringField(TEXT("node_id"), Registration.NodeId);
    JsonObject->TryGetStringField(TEXT("server_id"), Registration.ServerId);
    JsonObject->TryGetStringField(TEXT("region"), Registration.Region);
    JsonObject->TryGetStringField(TEXT("network_build"), Registration.NetworkBuild);
    JsonObject->TryGetStringField(TEXT("status"), Registration.Status);

    double CapacityValue = 0.0;
    double ActiveAllocationsValue = 0.0;
    double HeartbeatTtlValue = 0.0;
    JsonObject->TryGetNumberField(TEXT("capacity"), CapacityValue);
    JsonObject->TryGetNumberField(TEXT("active_allocations"), ActiveAllocationsValue);
    JsonObject->TryGetNumberField(TEXT("heartbeatTtlMs"), HeartbeatTtlValue);
    Registration.Capacity = FMath::RoundToInt(CapacityValue);
    Registration.ActiveAllocations = FMath::RoundToInt(ActiveAllocationsValue);
    Registration.HeartbeatTtlMs = FMath::RoundToInt(HeartbeatTtlValue);

    if (Registration.ServerId.IsEmpty() || Registration.NodeId.IsEmpty())
    {
        OnRequestFailed.Broadcast(TEXT("server-register"), TEXT("Server-registration response is missing node identity."));
        return;
    }

    bRegistered = true;
    bDraining = Registration.Status.Equals(TEXT("draining"), ESearchCase::IgnoreCase);
    StartHeartbeat(Registration.HeartbeatTtlMs);
    OnRegistered.Broadcast(Registration);
}

void USPDedicatedServerBackendSubsystem::StartHeartbeat(int32 HeartbeatTtlMs)
{
    StopHeartbeat();

    if (!bRegistered || bDraining)
    {
        return;
    }

    const float TtlSeconds = HeartbeatTtlMs > 0 ? static_cast<float>(HeartbeatTtlMs) / 1000.0f : 30.0f;
    HeartbeatIntervalSeconds = FMath::Clamp(TtlSeconds / 3.0f, 1.0f, 10.0f);
    HeartbeatTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &USPDedicatedServerBackendSubsystem::TickHeartbeat),
        HeartbeatIntervalSeconds);
}

void USPDedicatedServerBackendSubsystem::StopHeartbeat()
{
    if (HeartbeatTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
        HeartbeatTickerHandle.Reset();
    }
}

bool USPDedicatedServerBackendSubsystem::TickHeartbeat(float)
{
    if (!bRegistered || bDraining)
    {
        return false;
    }

    SendHeartbeat();
    return true;
}

void USPDedicatedServerBackendSubsystem::SendHeartbeat()
{
    if (!bConfigured || !bRegistered || bDraining)
    {
        return;
    }

    const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("serverId"), ServerId);
    Payload->SetStringField(TEXT("status"), TEXT("ready"));

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateInfrastructureJsonRequest(TEXT("/v1/servers/heartbeat"), TEXT("POST"));
    Request->SetContentAsString(SerializeJson(Payload));
    Request->OnProcessRequestComplete().BindUObject(this, &USPDedicatedServerBackendSubsystem::HandleHeartbeatResponse);

    if (!Request->ProcessRequest())
    {
        OnRequestFailed.Broadcast(TEXT("server-heartbeat"), TEXT("Unable to start dedicated-server heartbeat request."));
    }
}

void USPDedicatedServerBackendSubsystem::HandleHeartbeatResponse(FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
    {
        BroadcastHttpFailure(TEXT("server-heartbeat"), Response, bWasSuccessful);
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    if (!ParseJsonObject(Response->GetContentAsString(), JsonObject))
    {
        OnRequestFailed.Broadcast(TEXT("server-heartbeat"), TEXT("Backend returned invalid heartbeat JSON."));
        return;
    }

    FString ReturnedServerId;
    FString Status;
    double ActiveAllocationsValue = 0.0;
    JsonObject->TryGetStringField(TEXT("server_id"), ReturnedServerId);
    JsonObject->TryGetStringField(TEXT("status"), Status);
    JsonObject->TryGetNumberField(TEXT("active_allocations"), ActiveAllocationsValue);
    OnHeartbeat.Broadcast(ReturnedServerId, Status, FMath::RoundToInt(ActiveAllocationsValue));
}

void USPDedicatedServerBackendSubsystem::MarkDraining()
{
    if (!bConfigured || !bRegistered)
    {
        return;
    }

    StopHeartbeat();

    const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("serverId"), ServerId);

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateInfrastructureJsonRequest(TEXT("/v1/servers/drain"), TEXT("POST"));
    Request->SetContentAsString(SerializeJson(Payload));
    Request->OnProcessRequestComplete().BindUObject(this, &USPDedicatedServerBackendSubsystem::HandleDrainResponse);

    if (!Request->ProcessRequest())
    {
        OnRequestFailed.Broadcast(TEXT("server-drain"), TEXT("Unable to start dedicated-server drain request."));
    }
}

void USPDedicatedServerBackendSubsystem::HandleDrainResponse(FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
    {
        BroadcastHttpFailure(TEXT("server-drain"), Response, bWasSuccessful);
        return;
    }

    bDraining = true;
    StopHeartbeat();
    OnDrainChanged.Broadcast(true);
}

void USPDedicatedServerBackendSubsystem::AdmitConnection(const FString& AllocationId, const FString& MatchId, const FString& ConnectToken)
{
    if (!bConfigured || !bRegistered || bDraining || AllocationId.IsEmpty() || MatchId.IsEmpty() || ConnectToken.IsEmpty())
    {
        OnRequestFailed.Broadcast(TEXT("connection-admission"), TEXT("Dedicated server is not ready for connection admission or admission data is incomplete."));
        return;
    }

    const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("allocationId"), AllocationId);
    Payload->SetStringField(TEXT("matchId"), MatchId);
    Payload->SetStringField(TEXT("connectToken"), ConnectToken);

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateInfrastructureJsonRequest(TEXT("/v1/matches/admit"), TEXT("POST"));
    Request->SetContentAsString(SerializeJson(Payload));
    Request->OnProcessRequestComplete().BindUObject(this, &USPDedicatedServerBackendSubsystem::HandleAdmissionResponse);

    if (!Request->ProcessRequest())
    {
        OnRequestFailed.Broadcast(TEXT("connection-admission"), TEXT("Unable to start dedicated-server admission request."));
    }
}

void USPDedicatedServerBackendSubsystem::HandleAdmissionResponse(FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
    {
        BroadcastHttpFailure(TEXT("connection-admission"), Response, bWasSuccessful);
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    if (!ParseJsonObject(Response->GetContentAsString(), JsonObject))
    {
        OnRequestFailed.Broadcast(TEXT("connection-admission"), TEXT("Backend returned invalid admission JSON."));
        return;
    }

    FSPDedicatedServerAdmission Admission;
    JsonObject->TryGetBoolField(TEXT("admitted"), Admission.bAdmitted);
    JsonObject->TryGetStringField(TEXT("allocationId"), Admission.AllocationId);
    JsonObject->TryGetStringField(TEXT("matchId"), Admission.MatchId);
    JsonObject->TryGetStringField(TEXT("serverId"), Admission.ServerId);
    JsonObject->TryGetStringField(TEXT("region"), Admission.Region);
    JsonObject->TryGetStringField(TEXT("userId"), Admission.UserId);
    JsonObject->TryGetStringField(TEXT("networkBuild"), Admission.NetworkBuild);
    JsonObject->TryGetStringField(TEXT("backendProtocol"), Admission.BackendProtocol);

    if (!Admission.bAdmitted || Admission.ServerId != ServerId || Admission.NetworkBuild != USPBuildInfoLibrary::GetNetworkBuildId())
    {
        OnRequestFailed.Broadcast(TEXT("connection-admission"), TEXT("Admission response did not match this dedicated server/build."));
        return;
    }

    OnAdmissionCompleted.Broadcast(Admission);
}

void USPDedicatedServerBackendSubsystem::ReleaseAllocation(const FString& AllocationId, const FString& MatchId, bool bFailed)
{
    if (!bConfigured || !bRegistered || AllocationId.IsEmpty() || MatchId.IsEmpty())
    {
        OnRequestFailed.Broadcast(TEXT("allocation-release"), TEXT("Dedicated-server allocation release data is incomplete."));
        return;
    }

    const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("allocationId"), AllocationId);
    Payload->SetStringField(TEXT("matchId"), MatchId);
    Payload->SetStringField(TEXT("outcome"), bFailed ? TEXT("failed") : TEXT("closed"));

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateInfrastructureJsonRequest(TEXT("/v1/servers/release-allocation"), TEXT("POST"));
    Request->SetContentAsString(SerializeJson(Payload));
    Request->OnProcessRequestComplete().BindUObject(this, &USPDedicatedServerBackendSubsystem::HandleReleaseResponse);

    if (!Request->ProcessRequest())
    {
        OnRequestFailed.Broadcast(TEXT("allocation-release"), TEXT("Unable to start allocation release request."));
    }
}

void USPDedicatedServerBackendSubsystem::HandleReleaseResponse(FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
    {
        BroadcastHttpFailure(TEXT("allocation-release"), Response, bWasSuccessful);
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    if (!ParseJsonObject(Response->GetContentAsString(), JsonObject))
    {
        OnRequestFailed.Broadcast(TEXT("allocation-release"), TEXT("Backend returned invalid allocation-release JSON."));
        return;
    }

    FString AllocationId;
    FString MatchId;
    FString Status;
    double ActiveAllocationsValue = 0.0;
    JsonObject->TryGetStringField(TEXT("allocationId"), AllocationId);
    JsonObject->TryGetStringField(TEXT("matchId"), MatchId);
    JsonObject->TryGetStringField(TEXT("status"), Status);
    JsonObject->TryGetNumberField(TEXT("activeAllocations"), ActiveAllocationsValue);
    OnAllocationReleased.Broadcast(AllocationId, MatchId, Status, FMath::RoundToInt(ActiveAllocationsValue));
}

void USPDedicatedServerBackendSubsystem::SendBestEffortShutdownDrain()
{
    if (!bConfigured || !bRegistered || bDraining || MatchServerSecret.IsEmpty())
    {
        return;
    }

    const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("serverId"), ServerId);

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateInfrastructureJsonRequest(TEXT("/v1/servers/drain"), TEXT("POST"));
    Request->SetContentAsString(SerializeJson(Payload));
    Request->ProcessRequest();
    bDraining = true;
}

void USPDedicatedServerBackendSubsystem::BroadcastHttpFailure(const FString& Context, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        OnRequestFailed.Broadcast(Context, TEXT("Backend request failed before a valid HTTP response was received."));
        return;
    }

    FString Message = FString::Printf(TEXT("HTTP %d"), Response->GetResponseCode());
    TSharedPtr<FJsonObject> JsonObject;
    if (ParseJsonObject(Response->GetContentAsString(), JsonObject))
    {
        FString Error;
        if (JsonObject->TryGetStringField(TEXT("error"), Error) && !Error.IsEmpty())
        {
            Message += TEXT(": ") + Error;
        }
    }

    OnRequestFailed.Broadcast(Context, Message);
}
