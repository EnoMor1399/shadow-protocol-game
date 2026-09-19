# Bounded reconnect reservations

Reconnect-ticket issuance now atomically transitions an owned connected slot to
reconnecting only when it has no existing ticket or deadline. PostgreSQL sets the
90-second deadline. Concurrent or repeated client requests cannot replace the
winning token or extend the deadline. Only the hash is stored. An expired
reservation cannot be renewed by the client.

Issuance and redemption require an unfinished match with the session's region
and network build, plus a live server allocation. Redemption additionally requires
a reconnecting slot and the exact unexpired token. Successful redemption clears
the credential/deadline and keeps ready=false. Concurrent redemption and replay
therefore have one winner. A later connected slot may reserve again after a
successful restore, but cannot renew the same outstanding reservation.

The APIs return generic 403 denial for ineligible reservations without exposing
whether another user's slot exists. Issuance now uses `reconnect-reservation-denied`
for this condition. Callers must treat non-2xx responses as failure.

## Fresh transport admission

Run `npm run db:upgrade:v101` before deploying this change (fresh installs use
`db:init`). The idempotent `v101_reconnect_admission.sql` adds a grant UUID and
hashed transport credential to the reserved slot.

For a planned ready-room disconnect, obtain a ticket while connected. After
leaving the server, call `RequestReconnectAllocation(matchId, round, slot, ticket)`.
Its `/v1/matches/reconnect-allocation` request atomically exchanges the ticket for
a fresh connect token and grant UUID, leaving the slot reconnecting. The original
90-second deadline is preserved. Concurrent exchange has one winner. The response
uses the existing live allocation and endpoint; capacity is not incremented.

Pass `OnAllocationCompleted` to `ConnectToAllocation`. The validated travel URL
adds `spReconnectGrantId`. GameMode submits the grant and its own current round
using node-authenticated `/v1/matches/admit`. The backend atomically consumes the
transport credential and restores the slot unready. It checks the owning live
allocation, node identity, unfinished match, user, round, grant, hash and deadline.
A wrong node, stale round, expired credential or replay cannot restore the slot.

GameMode correlates both admission success and failure with a unique local request
ID so a delayed response cannot affect a newer attempt. Promotion also requires
an unexpired local reservation for the same backend user, match and slot, no live
duplicate identity, and the ready-room phase. It restores the server's reserved
team and spawn selection, preserves roster indices and clears readiness.

## Scope and remaining integration

This is source-level **planned ready-room reconnect**, not full mid-round recovery.
The authoritative slot must already be published to the backend with matching
round/index; automatic GameMode slot publication and account/session bootstrap
remain pending. Unexpected disconnects still need server-issued reservations.
Mid-round reconnect is denied until pawn, health and elimination state can be
restored without granting an extra life. A server restart loses local reservations.

The legacy `/v1/matches/reconnect` endpoint remains database-only. Do not call it
before transport exchange: it consumes the reservation without issuing admission.
If an exchange/admission response is lost or GameMode rejects promotion after
backend redemption, fail closed; no automatic retry/credential renewal is provided.
Trusted server reconciliation is still needed for this failure case.

PostgreSQL integration tests cover concurrent ticket issuance/exchange/admission,
wrong tokens, node and round, expiry, ended matches, replay, credential clearing
and unchanged capacity. Unreal source checks are not compilation or runtime tests.
UE5.6/UHT and packaged server/client ready-room reconnect still require validation.
