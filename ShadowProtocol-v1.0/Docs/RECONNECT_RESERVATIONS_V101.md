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

This is a prerequisite for the fresh Unreal admission-token exchange, not that
exchange itself. `/v1/matches/reconnect` still restores database slot state only.
It must not be described as successful packaged-client reconnection. Server-side
slot publication/restoration, fresh transport admission, round binding and runtime
tests remain necessary. The current ticket must be obtained while connected;
unexpected disconnection recovery still needs server-issued reservation support.
If the winning issuance response is lost, the client cannot recover the plaintext
token or renew the reservation; trusted server reconciliation is required.

Integration coverage executes real PostgreSQL updates and HTTP calls: four-way
issuance races, unchanged credentials/deadlines on repeat, wrong-region and ended
match denial without token consumption, four-way redemption races, replay, expiry,
renewal denial and released-allocation denial. No schema migration is required.
