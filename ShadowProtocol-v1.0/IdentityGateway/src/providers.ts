export type ProviderName = 'steam' | 'eos';

export type ProviderVerificationInput = {
  provider: ProviderName;
  authType: string;
  authToken: string;
};

export type VerifiedProviderIdentity = {
  provider: ProviderName;
  subject: string;
};

export type GatewayEnvironment = NodeJS.ProcessEnv;

type FetchLike = typeof fetch;

function required(env: GatewayEnvironment, name: string): string {
  const value = (env[name] ?? '').trim();
  if (!value) throw new Error(`${name} is not configured`);
  return value;
}

function requireHttpsUrl(raw: string, name: string, env: GatewayEnvironment): URL {
  let url: URL;
  try {
    url = new URL(raw);
  } catch {
    throw new Error(`${name} is invalid`);
  }
  const isLocalDev = env.NODE_ENV !== 'production'
    && (url.hostname === '127.0.0.1' || url.hostname === 'localhost' || url.hostname === '::1');
  if (url.protocol !== 'https:' && !(isLocalDev && url.protocol === 'http:')) {
    throw new Error(`${name} must use HTTPS`);
  }
  if (url.username || url.password || url.hash) throw new Error(`${name} contains unsupported URL components`);
  return url;
}

function safeSteamId(value: unknown): string {
  const steamId = String(value ?? '');
  if (!/^[0-9]{17,20}$/.test(steamId)) throw new Error('Steam verification returned an invalid SteamID');
  return steamId;
}

export async function verifySteam(
  input: ProviderVerificationInput,
  env: GatewayEnvironment = process.env,
  fetchImpl: FetchLike = fetch
): Promise<VerifiedProviderIdentity> {
  const apiKey = required(env, 'STEAM_WEB_API_KEY');
  const appId = required(env, 'STEAM_APP_ID');
  const identity = required(env, 'STEAM_WEB_API_IDENTITY');

  if (!/^[0-9]{1,10}$/.test(appId)) throw new Error('STEAM_APP_ID is invalid');
  if (!/^[A-Za-z0-9._:-]{2,64}$/.test(identity)) throw new Error('STEAM_WEB_API_IDENTITY is invalid');

  // AuthenticateUserTicket expects the binary Web API ticket encoded as hex.
  if (!/^[0-9a-fA-F]{32,32768}$/.test(input.authToken) || input.authToken.length % 2 !== 0) {
    throw new Error('Steam auth token is not a hexadecimal Web API ticket');
  }

  const url = new URL('https://partner.steam-api.com/ISteamUserAuth/AuthenticateUserTicket/v1/');
  url.searchParams.set('key', apiKey);
  url.searchParams.set('appid', appId);
  url.searchParams.set('ticket', input.authToken);
  url.searchParams.set('identity', identity);

  let response: Response;
  try {
    response = await fetchImpl(url, {
      method: 'GET',
      headers: { accept: 'application/json' },
      redirect: 'error',
      signal: AbortSignal.timeout(8_000)
    });
  } catch {
    throw new Error('Steam verification service is unavailable');
  }

  if (!response.ok) throw new Error('Steam rejected the authentication ticket');

  let json: any;
  try {
    json = await response.json();
  } catch {
    throw new Error('Steam verification returned invalid JSON');
  }

  const params = json?.response?.params;
  if (!params || String(params.result ?? '').toUpperCase() !== 'OK') {
    throw new Error('Steam authentication ticket is invalid');
  }

  return { provider: 'steam', subject: safeSteamId(params.steamid) };
}

export async function verifyEos(
  input: ProviderVerificationInput,
  env: GatewayEnvironment = process.env,
  fetchImpl: FetchLike = fetch
): Promise<VerifiedProviderIdentity> {
  const verifierUrl = requireHttpsUrl(required(env, 'EOS_VERIFIER_URL'), 'EOS_VERIFIER_URL', env);
  const verifierSecret = required(env, 'EOS_VERIFIER_SHARED_SECRET');
  if (verifierSecret.length < 32) throw new Error('EOS_VERIFIER_SHARED_SECRET must be at least 32 characters');

  let response: Response;
  try {
    response = await fetchImpl(verifierUrl, {
      method: 'POST',
      headers: {
        'content-type': 'application/json',
        accept: 'application/json',
        authorization: `Bearer ${verifierSecret}`
      },
      body: JSON.stringify({ authType: input.authType, idToken: input.authToken }),
      redirect: 'error',
      signal: AbortSignal.timeout(8_000)
    });
  } catch {
    throw new Error('EOS verification service is unavailable');
  }

  if (!response.ok) throw new Error('EOS verification rejected the identity token');

  let json: any;
  try {
    json = await response.json();
  } catch {
    throw new Error('EOS verification returned invalid JSON');
  }

  const subject = String(json?.subject ?? '').trim();
  if (!/^[A-Za-z0-9._:-]{8,256}$/.test(subject)) throw new Error('EOS verification returned an invalid subject');

  const expectedProductId = (env.EOS_EXPECTED_PRODUCT_ID ?? '').trim();
  const expectedSandboxId = (env.EOS_EXPECTED_SANDBOX_ID ?? '').trim();
  const expectedDeploymentId = (env.EOS_EXPECTED_DEPLOYMENT_ID ?? '').trim();
  if (expectedProductId && json?.productId !== expectedProductId) throw new Error('EOS product mismatch');
  if (expectedSandboxId && json?.sandboxId !== expectedSandboxId) throw new Error('EOS sandbox mismatch');
  if (expectedDeploymentId && json?.deploymentId !== expectedDeploymentId) throw new Error('EOS deployment mismatch');

  return { provider: 'eos', subject };
}

export async function verifyProviderIdentity(
  input: ProviderVerificationInput,
  env: GatewayEnvironment = process.env,
  fetchImpl: FetchLike = fetch
): Promise<VerifiedProviderIdentity> {
  if (input.provider === 'steam') return verifySteam(input, env, fetchImpl);
  if (input.provider === 'eos') return verifyEos(input, env, fetchImpl);
  throw new Error('Unsupported identity provider');
}
