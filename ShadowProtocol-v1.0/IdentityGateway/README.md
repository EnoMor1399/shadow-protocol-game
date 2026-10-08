# Shadow Protocol Identity Gateway

This service is the trust boundary between shipped Unreal clients and the gameplay backend.

## Flow

1. Unreal signs in through its active Steam or EOS OnlineSubsystem.
2. Unreal sends a provider credential to this gateway only.
3. The gateway verifies the credential server-side.
4. The gateway derives the provider subject from the verified provider response.
5. It signs a short-lived assertion containing provider, subject, region, network build and SHA-256(device nonce).
6. Unreal exchanges that assertion with the gameplay backend.
7. The gameplay backend rechecks the assertion signature, lifetime, replay state, build and device-nonce binding.

Never deploy the provider publisher/API keys or the assertion signing secret in the game client.

## Steam

The Steam adapter calls the official server-side `ISteamUserAuth/AuthenticateUserTicket/v1/` endpoint. Configure:

- `STEAM_WEB_API_KEY`
- `STEAM_APP_ID`
- `STEAM_WEB_API_IDENTITY`

The client credential must be a hexadecimal Steam Web API authentication ticket for the configured identity.

## EOS

EOS token verification is provided by the EOS SDK. The Node gateway therefore delegates EOS verification to an HTTPS, server-controlled verifier service configured with:

- `EOS_VERIFIER_URL`
- `EOS_VERIFIER_SHARED_SECRET`
- optional expected product/sandbox/deployment IDs.

That service must use `EOS_Auth_VerifyIdToken` or `EOS_Connect_VerifyIdToken` and return a subject only after an EOS success result. The gateway fails closed when the verifier is unavailable or metadata does not match.

## Local commands

```bash
npm install
npm run typecheck
npm test
npm start
```

Copy `.env.example` into your secret-management/deployment system. Do not commit populated secrets.
