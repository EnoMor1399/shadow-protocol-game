import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { test } from 'node:test';

async function source(relativePath) {
  return readFile(new URL(relativePath, import.meta.url), 'utf8');
}

test('platform identity bridge accepts only authenticated Steam or EOS providers', async () => {
  const header = await source('../../Source/ShadowProtocol/Public/SPPlatformIdentitySubsystem.h');
  const cpp = await source('../../Source/ShadowProtocol/Private/SPPlatformIdentitySubsystem.cpp');

  assert.match(header, /UCLASS\(Config=Game\)/);
  assert.match(header, /BeginPlatformSession/);
  assert.match(header, /CancelPlatformSession/);

  assert.match(cpp, /Online::GetSubsystem\(World\)/);
  assert.match(cpp, /GetIdentityInterface\(\)/);
  assert.match(cpp, /GetLoginStatus\(0\) == ELoginStatus::LoggedIn/);
  assert.match(cpp, /AutoLogin\(0\)/);
  assert.match(cpp, /GetAuthToken\(0\)/);
  assert.match(cpp, /Raw\.Contains\(TEXT\("STEAM"\)\)/);
  assert.match(cpp, /Raw\.Contains\(TEXT\("EOS"\)\)/);
  assert.match(cpp, /Production identity requires a supported Steam or EOS provider/);
  assert.doesNotMatch(cpp, /PendingProvider == TEXT\("null"\)/);
});

test('provider token travels only to the configured trusted gateway', async () => {
  const cpp = await source('../../Source/ShadowProtocol/Private/SPPlatformIdentitySubsystem.cpp');

  assert.match(cpp, /IdentityGatewayBaseUrl \+ TEXT\("\/v1\/platform-ticket"\)/);
  assert.match(cpp, /SetStringField\(TEXT\("provider"\), PendingProvider\)/);
  assert.match(cpp, /SetStringField\(TEXT\("authType"\), AuthType\)/);
  assert.match(cpp, /SetStringField\(TEXT\("authToken"\), AuthToken\)/);
  assert.match(cpp, /SetStringField\(TEXT\("region"\), PendingRegion\)/);
  assert.match(cpp, /SetStringField\(TEXT\("networkBuild"\), USPBuildInfoLibrary::GetNetworkBuildId\(\)\)/);
  assert.match(cpp, /SetStringField\(TEXT\("deviceNonce"\), PendingDeviceNonce\)/);
  assert.match(cpp, /SetTimeout\(15\.0f\)/);

  assert.doesNotMatch(cpp, /SESSION_BOOTSTRAP_SECRET/);
  assert.doesNotMatch(cpp, /PLATFORM_IDENTITY_ASSERTION_SECRET/);
  assert.doesNotMatch(cpp, /x-session-bootstrap-secret/);
  assert.doesNotMatch(cpp, /\/v1\/auth\/platform-session/);
});

test('shipping provider bridge requires HTTPS and correlates stale async responses', async () => {
  const cpp = await source('../../Source/ShadowProtocol/Private/SPPlatformIdentitySubsystem.cpp');

  assert.match(cpp, /#if UE_BUILD_SHIPPING/);
  assert.match(cpp, /StartsWith\(TEXT\("https:\/\/"\)/);
  assert.match(cpp, /Shipping identity gateway connections require HTTPS/);
  assert.match(cpp, /Generation != RequestGeneration/);
  assert.match(cpp, /Request != GatewayRequest/);
  assert.match(cpp, /ClearOnLoginCompleteDelegate_Handle/);
  assert.match(cpp, /CancelRequest\(\)/);
});

test('gateway assertion is handed to the existing backend assertion exchange', async () => {
  const cpp = await source('../../Source/ShadowProtocol/Private/SPPlatformIdentitySubsystem.cpp');
  const backend = await source('../../Source/ShadowProtocol/Private/SPBackendSessionSubsystem.cpp');

  assert.match(cpp, /TryGetStringField\(TEXT\("identityAssertion"\), Assertion\)/);
  assert.match(cpp, /ResponseProvider\.Equals\(PendingProvider/);
  assert.match(cpp, /ResponseBuild != USPBuildInfoLibrary::GetNetworkBuildId\(\)/);
  assert.match(cpp, /Backend->ExchangePlatformIdentityAssertion\(Assertion, Nonce\)/);

  assert.match(backend, /BuildUrl\(TEXT\("\/v1\/auth\/platform-session"\)\)/);
  assert.match(backend, /SetStringField\(TEXT\("assertion"\), IdentityAssertion\)/);
  assert.doesNotMatch(backend, /SetStringField\(TEXT\("authToken"\)/);
});

test('default project config keeps provider gateway unset and NULL only as local OSS baseline', async () => {
  const ini = await source('../../Config/DefaultEngine.ini');

  assert.match(ini, /DefaultPlatformService=NULL/);
  assert.match(ini, /SPPlatformIdentitySubsystem/);
  assert.match(ini, /IdentityGatewayBaseUrl=\s*\n/);
  assert.match(ini, /bAttemptAutoLogin=true/);
});
