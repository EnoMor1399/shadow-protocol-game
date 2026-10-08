# Shadow Protocol v1.0.1 — Trusted Platform Identity Bootstrap

## Goal

The shipped game client must not contain `SESSION_BOOTSTRAP_SECRET`, provider client
secrets, or the backend session-signing key. v1.0.1 now provides a production
handoff that lets a trusted identity gateway verify a platform account and give the
client a short-lived, single-use assertion that can be exchanged for a Shadow
Protocol backend session.

This patch implements the **assertion exchange contract**. It does not claim that
Steam, Epic Online Services, Xbox or PlayStation ticket verification is already
configured. Those provider adapters belong in the trusted identity gateway.

## Trust flow

1. The player signs into the selected platform/provider.
2. A trusted identity gateway validates the provider ticket outside the shipped
   Unreal client.
3. The gateway signs a short-lived assertion containing:
   - unique `jti`;
   - issuer `shadow-protocol-identity-gateway`;
   - audience `shadow-protocol-game-session`;
   - issue/expiry timestamps;
   - provider name;
   - stable provider subject;
   - selected matchmaking region;
   - network build;
   - SHA-256 hash of the device nonce used for this exchange.
4. Unreal receives only that signed assertion.
5. `USPBackendSessionSubsystem::ExchangePlatformIdentityAssertion` posts it with
   a device nonce to `POST /v1/auth/platform-session`.
6. The backend verifies the HMAC, issuer/audience, build, lifetime, nonce binding and replay state.
7. PostgreSQL maps the provider subject hash to a stable internal `users.id`.
8. The assertion `jti` is consumed once and attached to the created game session.
9. The backend returns the normal short-lived signed game-session token.
10. Unreal keeps the backend token, user id, provider, region and expiry in memory
    and continues through allocation/reconnect as before.

## Steam / EOS client bridge

`USPPlatformIdentitySubsystem` now provides the missing Unreal-side provider bridge before the assertion exchange:

1. Read the active world-scoped OnlineSubsystem.
2. Accept only Steam or EOS/EOSPlus provider names. The NULL provider remains a local networking baseline and is rejected for production identity.
3. Require a verified backend compatibility handshake.
4. If local user 0 is not signed in and `bAttemptAutoLogin=true`, call the provider's OnlineSubsystem `AutoLogin` flow and correlate the asynchronous login delegate.
5. Read the provider verification token through `IOnlineIdentity::GetAuthToken(0)`. The token remains memory-only.
6. POST the token to the separately configured trusted identity gateway at `/v1/platform-ticket`.
7. The gateway verifies that provider token with Steam/EOS server-side facilities, derives the trusted provider subject itself, hashes the supplied device nonce, and signs the short-lived assertion.
8. Unreal validates the returned provider/build metadata and passes only the signed assertion plus the same nonce to `ExchangePlatformIdentityAssertion`.

The provider-ticket request body is:

```json
{
  "provider": "steam | eos",
  "authType": "<OnlineSubsystem auth type>",
  "authToken": "<memory-only provider token>",
  "region": "acc",
  "networkBuild": "SP-1.0.1",
  "deviceNonce": "<8-128 character nonce>"
}
```

The expected gateway response is:

```json
{
  "identityAssertion": "<short-lived signed assertion>",
  "provider": "steam",
  "networkBuild": "SP-1.0.1"
}
```

The gateway must never trust a client-supplied provider subject/account id. It derives the subject from successful provider-side token verification. The assertion must include `deviceNonceHash = SHA-256(deviceNonce)`; the backend compares that signed hash with the nonce supplied to `POST /v1/auth/platform-session` using a timing-safe comparison.

Project defaults deliberately leave the gateway unset:

```ini
[/Script/ShadowProtocol.SPPlatformIdentitySubsystem]
IdentityGatewayBaseUrl=
bAttemptAutoLogin=true
```

Shipping builds reject non-HTTPS gateway URLs. Steam/EOS plugin credentials and provider SDK configuration remain platform/deployment configuration and are not embedded in this source-level bridge.

## Backend policy

Production defaults the legacy raw-user bootstrap route off:

```text
ENABLE_LEGACY_SESSION_BOOTSTRAP=false
```

The migration-only endpoint `POST /v1/auth/game-session` can still be enabled
explicitly for a trusted transition environment, but it is no longer the intended
production client flow.

Platform assertion configuration:

```text
PLATFORM_IDENTITY_ASSERTION_SECRET=<identity-gateway/backend verifier secret>
PLATFORM_IDENTITY_ASSERTION_MAX_TTL_MS=120000
```

The assertion signing secret exists only in the identity gateway and backend. It is
never sent to Unreal.

## Database migration

`v101_platform_identity.sql` adds:

- `platform_identities` — provider + SHA-256 subject binding to internal user id;
- `platform_identity_assertions` — one-time assertion consumption/replay audit;
- `game_sessions.auth_provider`;
- `game_sessions.identity_assertion_id`.

Raw provider subjects are not persisted by this migration. PostgreSQL stores the
provider plus a SHA-256 hash of the provider/subject tuple.

Existing accounts can be linked by inserting/updating the provider mapping through
a trusted account service. A new provider identity without a mapping receives a new
internal account with no password and an opaque internal placeholder email because
the legacy `users.email` column is currently non-null.

## Replay and account state

A valid assertion can create only one backend session. Reusing the same `jti`
returns `409 platform-identity-assertion-replayed`.

A second freshly signed assertion for the same provider subject resolves to the
same internal account. If the mapped user is not in `active` state, the exchange
returns `403 account-not-active`; the assertion remains consumed rather than
becoming reusable.

Incompatible network builds receive the existing HTTP 426 compatibility response.

## Unreal client behavior

`ExchangePlatformIdentityAssertion` requires compatibility to be verified first.
The request:

- has a 15-second timeout;
- carries only the assertion and device nonce;
- never adds `x-session-bootstrap-secret`;
- is correlated by request pointer and generation;
- cancels/rejects stale responses when a new sign-in begins, the backend URL
  changes or the subsystem shuts down.

Success emits `OnAuthenticatedSessionEstablished` with non-secret session
metadata. The backend session token remains private subsystem state and is reused by
the existing allocation, ready-state, reconnect and refresh requests.

## Validation

GitHub/PostgreSQL coverage verifies:

- assertion signature rejection;
- incompatible-build rejection;
- signed device-nonce mismatch rejection;
- one-time replay rejection;
- stable provider→internal-user mapping across fresh assertions;
- assertion audit linkage on `game_sessions`;
- production matchmaking/admission/reconnect using platform-issued sessions;
- Unreal source contract for no client bootstrap/verifier secret and stale-response
  correlation.

## Remaining provider work

The Unreal Steam/EOS client handoff is now implemented at source level. To complete a real platform deployment, implement and deploy the trusted identity-gateway adapters that verify Steam/EOS tokens and sign the documented nonce-bound assertion.

UE5.6/UHT, provider plugin/configuration builds, packaged login UI, provider SDK/server verification, account-linking UX and real Steam/EOS sandbox testing remain required before production sign-off.
