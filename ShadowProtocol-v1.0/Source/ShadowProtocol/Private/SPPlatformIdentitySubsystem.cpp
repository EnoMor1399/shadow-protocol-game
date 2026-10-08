#include "SPPlatformIdentitySubsystem.h"

#include "SPBackendSessionSubsystem.h"
#include "SPBuildInfoLibrary.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
bool ParseIdentityJson(const FString& Text, TSharedPtr<FJsonObject>& OutObject)
{
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
}

FString SerializeIdentityJson(const TSharedRef<FJsonObject>& Payload)
{
    FString Body;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
    FJsonSerializer::Serialize(Payload, Writer);
    return Body;
}

bool IsSafeRegion(const FString& Region)
{
    if (Region.Len() < 2 || Region.Len() > 32) return false;
    for (const TCHAR Ch : Region)
    {
        if (!FChar::IsAlnum(Ch) && Ch != TEXT('-') && Ch != TEXT('_')) return false;
    }
    return true;
}
}

void USPPlatformIdentitySubsystem::Deinitialize()
{
    CancelPlatformSession();
    Super::Deinitialize();
}

void USPPlatformIdentitySubsystem::SetIdentityGatewayBaseUrl(const FString& InBaseUrl)
{
    FString Normalized = InBaseUrl;
    Normalized.TrimStartAndEndInline();
    while (Normalized.EndsWith(TEXT("/"))) Normalized.LeftChopInline(1);

    if (Normalized != IdentityGatewayBaseUrl)
    {
        CancelPlatformSession();
        IdentityGatewayBaseUrl = Normalized;
    }
}

FString USPPlatformIdentitySubsystem::NormalizeProviderName(FName SubsystemName)
{
    const FString Raw = SubsystemName.ToString().ToUpper();
    if (Raw.Contains(TEXT("STEAM"))) return TEXT("steam");
    if (Raw.Contains(TEXT("EOS"))) return TEXT("eos");
    if (Raw == TEXT("NULL")) return TEXT("null");
    return FString();
}

FString USPPlatformIdentitySubsystem::GetDetectedProvider() const
{
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    IOnlineSubsystem* Subsystem = World ? Online::GetSubsystem(World) : nullptr;
    return Subsystem ? NormalizeProviderName(Subsystem->GetSubsystemName()) : FString();
}

bool USPPlatformIdentitySubsystem::ValidateGatewayUrl(FString& OutError) const
{
    if (IdentityGatewayBaseUrl.IsEmpty() || IdentityGatewayBaseUrl.Contains(TEXT("?"))
        || IdentityGatewayBaseUrl.Contains(TEXT("#")) || IdentityGatewayBaseUrl.Contains(TEXT("\\"))
        || IdentityGatewayBaseUrl.Contains(TEXT(" ")) || IdentityGatewayBaseUrl.Contains(TEXT("@")))
    {
        OutError = TEXT("Identity gateway URL is missing or invalid.");
        return false;
    }

#if UE_BUILD_SHIPPING
    if (!IdentityGatewayBaseUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Shipping identity gateway connections require HTTPS.");
        return false;
    }
#else
    if (!IdentityGatewayBaseUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase)
        && !IdentityGatewayBaseUrl.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Identity gateway URL must use HTTP or HTTPS.");
        return false;
    }
#endif

    OutError.Reset();
    return true;
}

void USPPlatformIdentitySubsystem::BeginPlatformSession(const FString& Region, const FString& DeviceNonce)
{
    CancelPlatformSession();

    FString GatewayError;
    if (!ValidateGatewayUrl(GatewayError))
    {
        Fail(GatewayError);
        return;
    }
    if (!IsSafeRegion(Region) || DeviceNonce.Len() < 8 || DeviceNonce.Len() > 128)
    {
        Fail(TEXT("Region or device nonce is invalid."));
        return;
    }

    UGameInstance* Instance = GetGameInstance();
    auto* Backend = Instance ? Instance->GetSubsystem<USPBackendSessionSubsystem>() : nullptr;
    if (!Backend || !Backend->HasVerifiedCompatibility())
    {
        Fail(TEXT("Backend compatibility must be verified before platform sign-in."));
        return;
    }

    UWorld* World = Instance ? Instance->GetWorld() : nullptr;
    IOnlineSubsystem* Subsystem = World ? Online::GetSubsystem(World) : nullptr;
    if (!Subsystem)
    {
        Fail(TEXT("No OnlineSubsystem provider is available."));
        return;
    }

    PendingProvider = NormalizeProviderName(Subsystem->GetSubsystemName());
    if (PendingProvider != TEXT("steam") && PendingProvider != TEXT("eos"))
    {
        const FString Unsupported = PendingProvider.IsEmpty() ? Subsystem->GetSubsystemName().ToString() : PendingProvider;
        ClearPendingState();
        OnBridgeFailed.Broadcast(Unsupported, TEXT("Production identity requires a supported Steam or EOS provider; NULL and unknown providers are rejected."));
        return;
    }

    PendingIdentity = Subsystem->GetIdentityInterface();
    if (!PendingIdentity.IsValid())
    {
        Fail(TEXT("The active online provider does not expose an identity interface."));
        return;
    }

    PendingRegion = Region;
    PendingDeviceNonce = DeviceNonce;
    const uint64 Generation = RequestGeneration;

    if (PendingIdentity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
    {
        ContinueWithLoggedInIdentity(Generation);
        return;
    }

    if (!bAttemptAutoLogin)
    {
        Fail(TEXT("The platform account is not signed in."));
        return;
    }

    LoginCompleteHandle = PendingIdentity->AddOnLoginCompleteDelegate_Handle(
        0,
        FOnLoginCompleteDelegate::CreateUObject(this, &USPPlatformIdentitySubsystem::HandleLoginComplete, Generation));

    if (!PendingIdentity->AutoLogin(0))
    {
        ClearLoginDelegate();
        Fail(TEXT("The platform sign-in flow could not be started."));
    }
}

void USPPlatformIdentitySubsystem::HandleLoginComplete(
    int32 LocalUserNum,
    bool bWasSuccessful,
    const FUniqueNetId&,
    const FString&,
    uint64 Generation)
{
    if (Generation != RequestGeneration || LocalUserNum != 0) return;
    ClearLoginDelegate();
    if (!bWasSuccessful)
    {
        Fail(TEXT("Platform sign-in failed."));
        return;
    }
    ContinueWithLoggedInIdentity(Generation);
}

void USPPlatformIdentitySubsystem::ContinueWithLoggedInIdentity(uint64 Generation)
{
    if (Generation != RequestGeneration || !PendingIdentity.IsValid()) return;
    if (PendingIdentity->GetLoginStatus(0) != ELoginStatus::LoggedIn
        || !PendingIdentity->GetUniquePlayerId(0).IsValid())
    {
        Fail(TEXT("The online provider did not establish a valid local identity."));
        return;
    }

    FString AuthToken = PendingIdentity->GetAuthToken(0);
    AuthToken.TrimStartAndEndInline();
    if (AuthToken.Len() < 16 || AuthToken.Len() > 16384)
    {
        Fail(TEXT("The online provider did not return a usable verification token."));
        return;
    }

    const FString AuthType = PendingIdentity->GetAuthType();
    SubmitProviderToken(AuthToken, AuthType, Generation);
    AuthToken.Reset();
}

void USPPlatformIdentitySubsystem::SubmitProviderToken(const FString& AuthToken, const FString& AuthType, uint64 Generation)
{
    if (Generation != RequestGeneration || PendingProvider.IsEmpty()) return;

    const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("provider"), PendingProvider);
    Payload->SetStringField(TEXT("authType"), AuthType);
    Payload->SetStringField(TEXT("authToken"), AuthToken);
    Payload->SetStringField(TEXT("region"), PendingRegion);
    Payload->SetStringField(TEXT("networkBuild"), USPBuildInfoLibrary::GetNetworkBuildId());
    Payload->SetStringField(TEXT("deviceNonce"), PendingDeviceNonce);

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    GatewayRequest = Request;
    Request->SetTimeout(15.0f);
    Request->SetURL(IdentityGatewayBaseUrl + TEXT("/v1/platform-ticket"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetContentAsString(SerializeIdentityJson(Payload));
    Request->OnProcessRequestComplete().BindUObject(
        this,
        &USPPlatformIdentitySubsystem::HandleGatewayResponse,
        Generation);

    OnBridgeStarted.Broadcast(PendingProvider);

    if (!Request->ProcessRequest())
    {
        if (GatewayRequest == Request) GatewayRequest.Reset();
        Fail(TEXT("Unable to start the trusted identity gateway request."));
    }
}

void USPPlatformIdentitySubsystem::HandleGatewayResponse(
    FHttpRequestPtr Request,
    FHttpResponsePtr Response,
    bool bWasSuccessful,
    uint64 Generation)
{
    if (Generation != RequestGeneration || !Request.IsValid() || Request != GatewayRequest) return;
    GatewayRequest.Reset();

    if (!bWasSuccessful || !Response.IsValid()
        || Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
    {
        Fail(TEXT("The trusted identity gateway rejected or could not verify the platform credential."));
        return;
    }

    TSharedPtr<FJsonObject> Json;
    if (!ParseIdentityJson(Response->GetContentAsString(), Json))
    {
        Fail(TEXT("The trusted identity gateway returned invalid JSON."));
        return;
    }

    FString Assertion;
    FString ResponseProvider;
    FString ResponseBuild;
    Json->TryGetStringField(TEXT("identityAssertion"), Assertion);
    Json->TryGetStringField(TEXT("provider"), ResponseProvider);
    Json->TryGetStringField(TEXT("networkBuild"), ResponseBuild);

    if (Assertion.Len() < 32)
    {
        Fail(TEXT("The trusted identity gateway did not return a signed identity assertion."));
        return;
    }
    if (!ResponseProvider.IsEmpty() && !ResponseProvider.Equals(PendingProvider, ESearchCase::IgnoreCase))
    {
        Fail(TEXT("The identity gateway response provider does not match the active online provider."));
        return;
    }
    if (!ResponseBuild.IsEmpty() && ResponseBuild != USPBuildInfoLibrary::GetNetworkBuildId())
    {
        Fail(TEXT("The identity gateway assertion targets a different network build."));
        return;
    }

    UGameInstance* Instance = GetGameInstance();
    auto* Backend = Instance ? Instance->GetSubsystem<USPBackendSessionSubsystem>() : nullptr;
    if (!Backend)
    {
        Fail(TEXT("Backend session service is unavailable."));
        return;
    }

    const FString Nonce = PendingDeviceNonce;
    ClearLoginDelegate();
    PendingIdentity.Reset();
    PendingProvider.Reset();
    PendingRegion.Reset();
    PendingDeviceNonce.Reset();

    Backend->ExchangePlatformIdentityAssertion(Assertion, Nonce);
}

void USPPlatformIdentitySubsystem::ClearLoginDelegate()
{
    if (PendingIdentity.IsValid() && LoginCompleteHandle.IsValid())
        PendingIdentity->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteHandle);
    LoginCompleteHandle.Reset();
}

void USPPlatformIdentitySubsystem::ClearPendingState()
{
    ClearLoginDelegate();
    PendingIdentity.Reset();
    PendingProvider.Reset();
    PendingRegion.Reset();
    PendingDeviceNonce.Reset();
}

void USPPlatformIdentitySubsystem::CancelPlatformSession()
{
    ++RequestGeneration;
    ClearLoginDelegate();
    if (GatewayRequest.IsValid())
    {
        GatewayRequest->OnProcessRequestComplete().Unbind();
        GatewayRequest->CancelRequest();
        GatewayRequest.Reset();
    }
    ClearPendingState();
}

void USPPlatformIdentitySubsystem::Fail(const FString& ErrorMessage)
{
    const FString Provider = PendingProvider;
    if (GatewayRequest.IsValid())
    {
        GatewayRequest->OnProcessRequestComplete().Unbind();
        GatewayRequest->CancelRequest();
        GatewayRequest.Reset();
    }
    ClearPendingState();
    OnBridgeFailed.Broadcast(Provider, ErrorMessage);
}
