import Fastify from 'fastify';
import { z } from 'zod';
import { signIdentityAssertion } from './assertion.js';
import { credentialFingerprint, FixedWindowLimiter } from './limiter.js';
import { verifyProviderIdentity } from './providers.js';

const PORT = Number(process.env.PORT ?? 8090);
const HOST = (process.env.HOST ?? '0.0.0.0').trim();
const ASSERTION_SECRET = (process.env.PLATFORM_IDENTITY_ASSERTION_SECRET ?? '').trim();
const assertionTtlCandidate = Number(process.env.PLATFORM_IDENTITY_ASSERTION_TTL_MS ?? 60_000);
const ASSERTION_TTL_MS = Number.isFinite(assertionTtlCandidate)
  ? Math.min(120_000, Math.max(30_000, Math.trunc(assertionTtlCandidate)))
  : 60_000;

const configuredBuilds = (process.env.ACCEPTED_NETWORK_BUILDS ?? 'SP-1.0.1')
  .split(',')
  .map(value => value.trim())
  .filter(Boolean);
const ACCEPTED_NETWORK_BUILDS = new Set(configuredBuilds);

function boundedInteger(name: string, fallback: number, min: number, max: number): number {
  const parsed = Number(process.env[name] ?? fallback);
  return Number.isFinite(parsed) ? Math.min(max, Math.max(min, Math.trunc(parsed))) : fallback;
}

const IP_MAX_ATTEMPTS = boundedInteger('PROVIDER_IP_MAX_ATTEMPTS', 30, 1, 300);
const CREDENTIAL_MAX_ATTEMPTS = boundedInteger('PROVIDER_CREDENTIAL_MAX_ATTEMPTS', 5, 1, 50);
const RATE_WINDOW_MS = boundedInteger('PROVIDER_RATE_WINDOW_MS', 60_000, 10_000, 300_000);

if (ASSERTION_SECRET.length < 32) {
  throw new Error('PLATFORM_IDENTITY_ASSERTION_SECRET must be configured with at least 32 characters');
}
if (ACCEPTED_NETWORK_BUILDS.size === 0) {
  throw new Error('At least one accepted network build must be configured');
}

const ipLimiter = new FixedWindowLimiter(IP_MAX_ATTEMPTS, RATE_WINDOW_MS);
const credentialLimiter = new FixedWindowLimiter(CREDENTIAL_MAX_ATTEMPTS, RATE_WINDOW_MS);

const ticketSchema = z.object({
  provider: z.enum(['steam', 'eos']),
  authType: z.string().trim().min(1).max(64),
  authToken: z.string().trim().min(16).max(32_768),
  region: z.string().trim().min(2).max(16).regex(/^[a-z0-9][a-z0-9_-]*$/),
  networkBuild: z.string().trim().min(2).max(32),
  deviceNonce: z.string().min(8).max(128)
}).strict();

export function buildServer() {
  const app = Fastify({
    bodyLimit: 40 * 1024,
    disableRequestLogging: true,
    logger: {
      level: process.env.LOG_LEVEL ?? 'info',
      redact: {
        paths: ['req.headers.authorization', 'req.body.authToken', 'req.body.deviceNonce'],
        censor: '[REDACTED]'
      }
    }
  });

  app.addHook('onSend', async (_request, reply, payload) => {
    reply.header('x-content-type-options', 'nosniff');
    return payload;
  });

  app.get('/health', async () => ({
    ok: true,
    service: 'shadow-protocol-identity-gateway',
    acceptedNetworkBuilds: [...ACCEPTED_NETWORK_BUILDS],
    steamConfigured: Boolean(process.env.STEAM_WEB_API_KEY && process.env.STEAM_APP_ID && process.env.STEAM_WEB_API_IDENTITY),
    eosConfigured: Boolean(process.env.EOS_VERIFIER_URL && process.env.EOS_VERIFIER_SHARED_SECRET)
  }));

  app.post('/v1/platform-ticket', async (request, reply) => {
    reply.header('cache-control', 'no-store');
    reply.header('pragma', 'no-cache');
    const parsed = ticketSchema.safeParse(request.body);
    if (!parsed.success) return reply.code(400).send({ error: 'invalid-platform-ticket-request' });

    const input = parsed.data;
    if (!ACCEPTED_NETWORK_BUILDS.has(input.networkBuild)) {
      return reply.code(426).send({ error: 'client-build-incompatible' });
    }

    if (!ipLimiter.consume(request.ip)) {
      return reply.code(429).send({ error: 'platform-ticket-rate-limited' });
    }
    const fingerprint = credentialFingerprint(input.provider, input.authToken, ASSERTION_SECRET);
    if (!credentialLimiter.consume(fingerprint)) {
      return reply.code(429).send({ error: 'platform-ticket-rate-limited' });
    }

    let identity;
    try {
      identity = await verifyProviderIdentity({
        provider: input.provider,
        authType: input.authType,
        authToken: input.authToken
      });
    } catch {
      return reply.code(401).send({ error: 'platform-credential-verification-failed' });
    }

    if (identity.provider !== input.provider) {
      return reply.code(401).send({ error: 'platform-provider-mismatch' });
    }

    const signed = signIdentityAssertion({
      provider: identity.provider,
      subject: identity.subject,
      region: input.region,
      build: input.networkBuild,
      deviceNonce: input.deviceNonce
    }, ASSERTION_SECRET, ASSERTION_TTL_MS);

    return reply.code(201).send({
      identityAssertion: signed.token,
      provider: identity.provider,
      networkBuild: input.networkBuild,
      expiresAt: new Date(signed.expiresAt).toISOString()
    });
  });

  return app;
}

if (process.env.NODE_ENV !== 'test') {
  const app = buildServer();
  app.listen({ host: HOST, port: PORT }).catch(error => {
    app.log.error({ err: error }, 'identity gateway startup failed');
    process.exit(1);
  });
}
