import assert from 'node:assert/strict';
import { createHmac } from 'node:crypto';
import { test } from 'node:test';
import { sha256, signIdentityAssertion } from '../src/assertion.js';

test('signs a short-lived backend-compatible nonce-bound assertion', () => {
  const secret = 'test-identity-assertion-secret-0123456789';
  const now = 1_800_000_000_000;
  const signed = signIdentityAssertion({
    provider: 'steam',
    subject: '76561198000000000',
    region: 'acc',
    build: 'SP-1.0.1',
    deviceNonce: 'nonce-for-device-123'
  }, secret, 60_000, now);

  const [encoded, signature] = signed.token.split('.');
  assert.ok(encoded);
  assert.ok(signature);

  const expected = createHmac('sha256', secret).update(encoded).digest('base64url');
  assert.equal(signature, expected);

  const claims = JSON.parse(Buffer.from(encoded, 'base64url').toString('utf8'));
  assert.equal(claims.iss, 'shadow-protocol-identity-gateway');
  assert.equal(claims.aud, 'shadow-protocol-game-session');
  assert.equal(claims.provider, 'steam');
  assert.equal(claims.subject, '76561198000000000');
  assert.equal(claims.region, 'acc');
  assert.equal(claims.build, 'SP-1.0.1');
  assert.equal(claims.deviceNonceHash, sha256('nonce-for-device-123'));
  assert.equal(claims.iat, now);
  assert.equal(claims.exp, now + 60_000);
});

test('bounds assertion lifetime and rejects weak signing secrets', () => {
  const input = {
    provider: 'eos' as const,
    subject: '0123456789abcdef0123456789abcdef',
    region: 'acc',
    build: 'SP-1.0.1',
    deviceNonce: 'nonce-for-device-456'
  };

  assert.throws(() => signIdentityAssertion(input, 'short'), /at least 32 characters/);

  const low = signIdentityAssertion(input, 'long-enough-secret-for-testing-123456', 1, 1000);
  assert.equal(low.claims.exp - low.claims.iat, 30_000);

  const high = signIdentityAssertion(input, 'long-enough-secret-for-testing-123456', 999_999, 1000);
  assert.equal(high.claims.exp - high.claims.iat, 120_000);
});
