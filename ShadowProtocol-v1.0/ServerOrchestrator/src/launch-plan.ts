import { signServerAttestation, type ServerLaunchIdentity } from './attestation.js';

export type LaunchPlan = {
  executable: string;
  args: string[];
  childEnv: NodeJS.ProcessEnv;
  identity: ServerLaunchIdentity;
};

function required(env: NodeJS.ProcessEnv, name: string): string {
  const value = (env[name] ?? '').trim();
  if (!value) throw new Error(`${name} is required`);
  return value;
}

function positiveInteger(env: NodeJS.ProcessEnv, name: string, fallback?: number): number {
  const raw = (env[name] ?? (fallback === undefined ? '' : String(fallback))).trim();
  const value = Number(raw);
  if (!Number.isInteger(value)) throw new Error(`${name} must be an integer`);
  return value;
}

function validateBackendUrl(raw: string, env: NodeJS.ProcessEnv): string {
  let url: URL;
  try {
    url = new URL(raw);
  } catch {
    throw new Error('SP_BACKEND_URL is invalid');
  }

  const allowLocalHttp = (env.NODE_ENV ?? 'production') !== 'production'
    && (url.hostname === '127.0.0.1' || url.hostname === 'localhost' || url.hostname === '::1');
  if (url.protocol !== 'https:' && !(allowLocalHttp && url.protocol === 'http:')) {
    throw new Error('SP_BACKEND_URL must use HTTPS outside local development');
  }
  if (url.username || url.password || url.search || url.hash) {
    throw new Error('SP_BACKEND_URL contains unsupported URL components');
  }
  return url.toString().replace(/\/$/, '');
}

function parseExtraArgs(env: NodeJS.ProcessEnv): string[] {
  const raw = (env.SP_SERVER_EXTRA_ARGS_JSON ?? '[]').trim();
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch {
    throw new Error('SP_SERVER_EXTRA_ARGS_JSON must be valid JSON');
  }
  if (!Array.isArray(parsed) || parsed.length > 32 || !parsed.every(v => typeof v === 'string')) {
    throw new Error('SP_SERVER_EXTRA_ARGS_JSON must be a JSON array of at most 32 strings');
  }
  const args = parsed as string[];
  for (const arg of args) {
    if (!arg || arg.length > 512 || /[\r\n\0]/.test(arg)) {
      throw new Error('SP_SERVER_EXTRA_ARGS_JSON contains an invalid argument');
    }
  }
  return args;
}

export function buildLaunchPlan(env: NodeJS.ProcessEnv = process.env, now = Date.now()): LaunchPlan {
  const executable = required(env, 'GAME_SERVER_EXECUTABLE');
  if (/[\r\n\0]/.test(executable)) throw new Error('GAME_SERVER_EXECUTABLE is invalid');

  const backendUrl = validateBackendUrl(required(env, 'SP_BACKEND_URL'), env);
  const registrationSecret = required(env, 'SERVER_REGISTRATION_SECRET');
  if (registrationSecret.length < 32) {
    throw new Error('SERVER_REGISTRATION_SECRET must be at least 32 characters');
  }
  const orchestratorSecret = required(env, 'ORCHESTRATOR_ATTESTATION_SECRET');

  const identity: ServerLaunchIdentity = {
    serverId: required(env, 'SP_SERVER_ID'),
    region: required(env, 'SP_REGION'),
    networkBuild: (env.SP_NETWORK_BUILD ?? 'SP-1.0.1').trim(),
    publicHost: required(env, 'SP_PUBLIC_HOST'),
    publicPort: positiveInteger(env, 'SP_PUBLIC_PORT'),
    capacity: positiveInteger(env, 'SP_CAPACITY', 10)
  };

  const ttlMs = positiveInteger(env, 'SERVER_ATTESTATION_TTL_MS', 60_000);
  const { token } = signServerAttestation(identity, orchestratorSecret, ttlMs, now);

  const map = (env.GAME_SERVER_MAP ?? '/Game/Maps/Embassy/Embassy_P').trim();
  if (!map || map.length > 256 || /[\r\n\0]/.test(map)) throw new Error('GAME_SERVER_MAP is invalid');

  const args = [
    map,
    '-server',
    '-log',
    `-SPBackendUrl=${backendUrl}`,
    `-SPServerId=${identity.serverId}`,
    `-SPRegion=${identity.region}`,
    `-SPPublicHost=${identity.publicHost}`,
    `-SPPublicPort=${identity.publicPort}`,
    `-SPCapacity=${identity.capacity}`,
    ...parseExtraArgs(env)
  ];

  const childEnv: NodeJS.ProcessEnv = { ...env };
  delete childEnv.ORCHESTRATOR_ATTESTATION_SECRET;
  delete childEnv.SP_SERVER_EXTRA_ARGS_JSON;
  childEnv.SERVER_REGISTRATION_SECRET = registrationSecret;
  childEnv.SP_NODE_ATTESTATION = token;

  return { executable, args, childEnv, identity };
}
