# Allocation travel validation and connection failures

## Implemented

The client validates host syntax with the shared, engine-independent C++ header
`SPTravelValidation.h` before delivering an allocation and again when constructing
the travel URL. It supports ASCII DNS names, strict dotted-decimal IPv4, bare or
bracketed IPv6, and IPv4-embedded IPv6. It rejects schemes, ports embedded in the
host, URL option/path/fragment injection, whitespace, invalid labels, malformed IP
addresses, IPv6 zone identifiers and ambiguous numeric IPv4 spellings. Unicode
hostnames must be provided as ASCII punycode. This is syntax validation, not DNS
resolution, endpoint authorization or proof of reachability.

Travel also checks allocation expiry, exact network build, token size, integer
port range and the owning local controller's game instance. The backend session
and compatibility check must still be valid when travel starts. An expired or
rejected allocation reports a failure instead of calling ClientTravel. Refresh
serialization also applies to travel through the existing authenticated gate.

The backend subsystem subscribes to engine network/travel failures and removes
both subscriptions at deinitialization. Only failures with a world in its own
game instance, in client/standalone mode, are forwarded. Null-world events are
ignored because their owner cannot be established safely across multiple worlds.
Raw engine error text is never forwarded: it may contain the token-bearing URL.
Generic messages are available via `GetLastConnectionError` and `OnRequestFailed`
with `network` or `travel` context. The next valid travel clears the old message;
custom UI can call `ClearConnectionError`. The ready-room panel also shows it
when that panel is visible. No permanent in-match overlay or automatic map return
is added by this patch.

## Reconnect boundary

The existing `/v1/matches/reconnect` endpoint consumes a reconnect token and
restores a database slot. It does not return a fresh one-time Unreal admission
token or connection allocation. Therefore its successful response must not be
treated as completed packaged-client reconnection. Automatic retry of the old
allocation URL is intentionally absent: its token may already have been spent.
A subsequent backend/Unreal integration must issue and redeem a new credential
for the reserved match, restore the server-side slot and verify the entire flow.

## Tests

CI now compiles and executes the exact shared host-validator implementation:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -I Source/ShadowProtocol/Public Tests/travel-validation.cpp -o /tmp/sp-travel-validation
/tmp/sp-travel-validation
```

This exercises 49 address cases and a disallowed-ASCII injection sweep. Fourteen
Node source-contract checks guard Unreal wiring, including lifetime/build checks,
controller scoping and safe failure forwarding. These do not compile Unreal C++.

UE 5.6/UHT and packaged runtime validation remain required. Test expired allocation
before/after UI delay, invalid host input, IPv4/IPv6 server travel, connection loss,
travel rejection, multiple PIE worlds, and teardown with callbacks pending. Check
that errors never display connect tokens and never trigger automatic token replay.

Fresh admission is now available for planned ready-room reconnect through
`RequestReconnectAllocation` and `ConnectToAllocation`; see
`RECONNECT_RESERVATIONS_V101.md` for prerequisites and remaining runtime gates.
