import assert from 'node:assert/strict';
import { test } from 'node:test';

process.env.NODE_ENV = 'test';
process.env.PLATFORM_IDENTITY_ASSERTION_SECRET = 'gateway-server-test-secret-0123456789012345';
process.env.ACCEPTED_NETWORK_BUILDS = 'SP-1.0.1';
process.env.PROVIDER_CREDENTIAL_MAX_ATTEMPTS = '2';
process.env.PROVIDER_IP_MAX_ATTEMPTS = '20';
process.env.PROVIDER_RATE_WINDOW_MS = '60000';

test('gateway health exposes configuration state without secrets', async () => {
  const { buildServer } = await import('../src/server.js');
  const app = buildServer();
  const response = await app.inject({ method: 'GET', url: '/health' });
  assert.equal(response.statusCode, 200);
  const body = response.json();
  assert.equal(body.ok, true);
  assert.equal(body.service, 'shadow-protocol-identity-gateway');
  assert.equal(JSON.stringify(body).includes(process.env.PLATFORM_IDENTITY_ASSERTION_SECRET!), false);
  await app.close();
});

test('gateway rate-limits repeated use of the same provider credential fingerprint', async () => {
  const { buildServer } = await import('../src/server.js');
  const app = buildServer();
  const payload = {
    provider: 'steam',
    authType: 'webapi-ticket',
    authToken: 'ab'.repeat(16),
    region: 'acc',
    networkBuild: 'SP-1.0.1',
    deviceNonce: 'device-nonce-rate-limit'
  };

  const first = await app.inject({ method: 'POST', url: '/v1/platform-ticket', payload });
  const second = await app.inject({ method: 'POST', url: '/v1/platform-ticket', payload });
  const third = await app.inject({ method: 'POST', url: '/v1/platform-ticket', payload });

  assert.equal(first.statusCode, 401);
  assert.equal(second.statusCode, 401);
  assert.equal(third.statusCode, 429);
  assert.equal(third.json().error, 'platform-ticket-rate-limited');
  await app.close();
});

test('gateway rejects incompatible builds before provider verification', async () => {
  const { buildServer } = await import('../src/server.js');
  const app = buildServer();
  const response = await app.inject({
    method: 'POST',
    url: '/v1/platform-ticket',
    payload: {
      provider: 'eos',
      authType: 'id-token',
      authToken: 'eos-token-not-sent-to-provider',
      region: 'acc',
      networkBuild: 'SP-0.9.0',
      deviceNonce: 'device-nonce-old-build'
    }
  });
  assert.equal(response.statusCode, 426);
  assert.equal(response.json().error, 'client-build-incompatible');
  await app.close();
});
