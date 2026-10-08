import { createHash, createHmac, randomUUID } from 'node:crypto';

export const IDENTITY_ISSUER = 'shadow-protocol-identity-gateway';
export const IDENTITY_AUDIENCE = 'shadow-protocol-game-session';

export type AssertionInput = {
  provider: 'steam' | 'eos';
  subject: string;
  region: string;
  build: string;
  deviceNonce: string;
};

export type SignedAssertion = {
  token: string;
  expiresAt: number;
  claims: {
    jti: string;
    iss: string;
    aud: string;
    iat: number;
    exp: number;
    provider: 'steam' | 'eos';
    subject: string;
    region: string;
    build: string;
    deviceNonceHash: string;
  };
};

export function sha256(value: string): string {
  return createHash('sha256').update(value).digest('hex');
}

export function signIdentityAssertion(
  input: AssertionInput,
  secret: string,
  ttlMs = 60_000,
  now = Date.now()
): SignedAssertion {
  if (secret.trim().length < 32) throw new Error('PLATFORM_IDENTITY_ASSERTION_SECRET must be at least 32 characters');
  const boundedTtl = Math.min(120_000, Math.max(30_000, Math.trunc(ttlMs)));
  const claims = {
    jti: randomUUID(),
    iss: IDENTITY_ISSUER,
    aud: IDENTITY_AUDIENCE,
    iat: now,
    exp: now + boundedTtl,
    provider: input.provider,
    subject: input.subject,
    region: input.region,
    build: input.build,
    deviceNonceHash: sha256(input.deviceNonce)
  };
  const encoded = Buffer.from(JSON.stringify(claims)).toString('base64url');
  const signature = createHmac('sha256', secret).update(encoded).digest('base64url');
  return { token: `${encoded}.${signature}`, expiresAt: claims.exp, claims };
}
