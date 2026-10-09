import { createHmac, randomUUID } from 'node:crypto';

export const SERVER_ATTESTATION_ISSUER = 'shadow-protocol-orchestrator';
export const SERVER_ATTESTATION_AUDIENCE = 'shadow-protocol-server-registration';

export type ServerLaunchIdentity = {
  serverId: string;
  region: string;
  networkBuild: string;
  publicHost: string;
  publicPort: number;
  capacity: number;
};

export type ServerAttestationClaims = ServerLaunchIdentity & {
  jti: string;
  iss: typeof SERVER_ATTESTATION_ISSUER;
  aud: typeof SERVER_ATTESTATION_AUDIENCE;
  iat: number;
  exp: number;
};

function assertText(name: string, value: string, min: number, max: number): string {
  const normalized = value.trim();
  if (normalized.length < min || normalized.length > max) {
    throw new Error(`${name} must be between ${min} and ${max} characters`);
  }
  return normalized;
}

export function validateServerLaunchIdentity(input: ServerLaunchIdentity): ServerLaunchIdentity {
  const serverId = assertText('SP_SERVER_ID', input.serverId, 2, 64);
  if (!/^[A-Za-z0-9][A-Za-z0-9._-]+$/.test(serverId)) {
    throw new Error('SP_SERVER_ID contains unsupported characters');
  }

  const region = assertText('SP_REGION', input.region, 2, 16);
  if (!/^[A-Za-z0-9][A-Za-z0-9_-]*$/.test(region)) {
    throw new Error('SP_REGION contains unsupported characters');
  }

  const networkBuild = assertText('SP_NETWORK_BUILD', input.networkBuild, 2, 32);
  if (!/^[A-Za-z0-9][A-Za-z0-9._-]*$/.test(networkBuild)) {
    throw new Error('SP_NETWORK_BUILD contains unsupported characters');
  }

  const publicHost = assertText('SP_PUBLIC_HOST', input.publicHost, 1, 255);
  if (/\s|[/?#@\\]/.test(publicHost) || publicHost.includes('://')) {
    throw new Error('SP_PUBLIC_HOST must be a hostname or IP address without a scheme, path, credentials, or whitespace');
  }

  if (!Number.isInteger(input.publicPort) || input.publicPort < 1 || input.publicPort > 65535) {
    throw new Error('SP_PUBLIC_PORT must be an integer from 1 to 65535');
  }
  if (!Number.isInteger(input.capacity) || input.capacity < 1 || input.capacity > 128) {
    throw new Error('SP_CAPACITY must be an integer from 1 to 128');
  }

  return {
    serverId,
    region,
    networkBuild,
    publicHost,
    publicPort: input.publicPort,
    capacity: input.capacity
  };
}

export function signServerAttestation(
  identity: ServerLaunchIdentity,
  secret: string,
  ttlMs = 60_000,
  now = Date.now()
): { token: string; claims: ServerAttestationClaims } {
  const normalizedSecret = secret.trim();
  if (normalizedSecret.length < 32) {
    throw new Error('ORCHESTRATOR_ATTESTATION_SECRET must be at least 32 characters');
  }

  const validated = validateServerLaunchIdentity(identity);
  const boundedTtl = Math.min(120_000, Math.max(30_000, Math.trunc(ttlMs)));
  const claims: ServerAttestationClaims = {
    jti: randomUUID(),
    iss: SERVER_ATTESTATION_ISSUER,
    aud: SERVER_ATTESTATION_AUDIENCE,
    iat: Math.trunc(now),
    exp: Math.trunc(now) + boundedTtl,
    ...validated
  };

  const encoded = Buffer.from(JSON.stringify(claims)).toString('base64url');
  const signature = createHmac('sha256', normalizedSecret).update(encoded).digest('base64url');
  return { token: `${encoded}.${signature}`, claims };
}
