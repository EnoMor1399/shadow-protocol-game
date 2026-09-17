# v0.5 Competitive Match Systems

## Match loop
The prototype now treats PROTOCOL as a match instead of an isolated mission. Scores persist across rounds and the local vertical slice rotates the player's side each round to exercise both tactical responsibilities. The production Unreal value is configurable through `SideSwitchInterval`.

## Attack responsibility
1. Prepare and issue squad orders.
2. Locate communications intelligence.
3. Reveal and secure the true SP data package.
4. Reach extraction.

## Defense responsibility
1. Use the preparation window to fortify selected breachable entries.
2. Deny the attackers' communications-terminal access.
3. Protect the revealed SP data package if intelligence is compromised.
4. Intercept the carrier if the package is stolen.
5. Win through attacking-element elimination or timer expiry.

## Fortification
Barricades are deliberately limited and only reinforce predefined tactical entry points. They do not turn the environment into a construction sandbox. Attackers can breach them, preserving the controlled-destruction philosophy.

## Ballistics
Selected thin/reinforced-entry surfaces can be penetrated once. Damage is reduced after penetration. Near misses can suppress opponents even without a direct hit, allowing LMG/support-style area control to be expanded later.

## Tactical projectiles
Flash and smoke devices now travel as projectiles rather than teleporting to a destination. The Unreal implementation delegates detonation effects to Blueprint so audiovisual polish and gameplay tuning can evolve without moving authority to the client.

## Observer model
Dead players can follow surviving team members or enter a free tactical observer camera in the local prototype. Production ranked rules should restrict free camera visibility to prevent intelligence leakage; the replicated controller state is intentionally policy-neutral so rulesets can enforce the correct visibility scope.

## Network authority
Round scores, team sides, player slots, fortifications, projectile spawn, penetration results, inventory consumption and progression remain server-authoritative. Client input requests actions but does not decide competitive outcomes.
