import assert from 'node:assert/strict';
import { createHmac } from 'node:crypto';
import { test } from 'node:test';
import {
  SERVER_ATTESTATION_AUDIENCE,
  SERVER_ATTESTATION_ISSUER,
  signServerAttestation
} from '../src/attestation.js';

const identity = {
  serverId: 'ACC-PROTOCOL-01',
  region: 'acc',
  networkBuild: 'SP-1.0.1',
  publicHost: 'game-01.example.net',
  publicPort: 7777,
  capacity: 10
};

test('signs backend-compatible short-lived server attestations', () => {
  const secret = 'orchestrator-secret-that-is-definitely-long-enough';
  const now = 1_800_000_000_000;
  const signed = signServerAttestation(identity, secret, 60_000, now);
  const [encoded, signature] = signed.token.split('.');

  assert.ok(encoded);
  assert.ok(signature);
  const expected = createHmac('sha256', secret).update(encoded).digest('base64url');
  assert.equal(signature, expected);

  const claims = JSON.parse(Buffer.from(encoded, 'base64url').toString('utf8'));
  assert.equal(claims.iss, SERVER_ATTESTATION_ISSUER);
  assert.equal(claims.aud, SERVER_ATTESTATION_AUDIENCE);
  assert.equal(claims.iat, now);
  assert.equal(claims.exp, now + 60_000);
  assert.equal(claims.serverId, identity.serverId);
  assert.equal(claims.region, identity.region);
  assert.equal(claims.networkBuild, identity.networkBuild);
  assert.equal(claims.publicHost, identity.publicHost);
  assert.equal(claims.publicPort, identity.publicPort);
  assert.equal(claims.capacity, identity.capacity);
  assert.match(claims.jti, /^[0-9a-f-]{36}$/);
});

test('bounds lifetime and rejects weak or malformed launch identity', () => {
  const secret = 'another-orchestrator-secret-that-is-long-enough';
  const short = signServerAttestation(identity, secret, 1, 1000);
  assert.equal(short.claims.exp - short.claims.iat, 30_000);
  const long = signServerAttestation(identity, secret, 999_999, 1000);
  assert.equal(long.claims.exp - long.claims.iat, 120_000);

  assert.throws(() => signServerAttestation(identity, 'too-short', 60_000, 1000), /at least 32/);
  assert.throws(() => signServerAttestation({ ...identity, serverId: '../bad' }, secret), /SP_SERVER_ID/);
  assert.throws(() => signServerAttestation({ ...identity, publicHost: 'https://example.net/path' }, secret), /SP_PUBLIC_HOST/);
  assert.throws(() => signServerAttestation({ ...identity, publicPort: 70000 }, secret), /SP_PUBLIC_PORT/);
});
