import assert from 'node:assert/strict';
import { test } from 'node:test';
import { credentialFingerprint, FixedWindowLimiter } from '../src/limiter.js';

test('fixed-window limiter blocks excess attempts and resets after the window', () => {
  const limiter = new FixedWindowLimiter(2, 1000, 100);
  assert.equal(limiter.consume('key', 1000), true);
  assert.equal(limiter.consume('key', 1001), true);
  assert.equal(limiter.consume('key', 1002), false);
  assert.equal(limiter.consume('key', 2000), true);
});

test('credential fingerprint is stable, provider-scoped and does not contain raw token', () => {
  const secret = 'limiter-secret-that-is-long-enough-1234';
  const token = 'sensitive-provider-token-value';
  const a = credentialFingerprint('steam', token, secret);
  const b = credentialFingerprint('steam', token, secret);
  const c = credentialFingerprint('eos', token, secret);
  assert.equal(a, b);
  assert.notEqual(a, c);
  assert.equal(a.length, 64);
  assert.equal(a.includes(token), false);
});
