# Bot System Guide

This directory contains the current multiplayer bot system. Use this file as the working contract when changing bot behavior in `code/fgame/`.

## Scope

The active bot system lives primarily in:

- `playerbot.h` / `playerbot.cpp`: `BotController`, event hooks, frame updates, weapon selection, spawn/death handling.
- `playerbot_states.cpp`: high-level state machine and state transitions.
- `playerbot_movement.cpp`: pathing, obstacle handling, jump logic, attractive nodes.
- `playerbot_rotation.cpp`: aim and turn dynamics.
- `playerbot_beliefs.h` / `playerbot_beliefs.cpp`: belief-map storage and patrol targeting.
- `playerbot_profile.h` / `playerbot_profile.cpp`: data-only bot profiles loaded from `bots/profiles/*.cfg`.
- `playerbot_strategy.h` / `playerbot_strategy.cpp`: strategy helpers layered on top of controller state.
- `playerbot_master.cpp`: controller manager ownership and ticking.
- `g_bot.cpp` / `g_bot.h`: bot creation, restoration, removal, and game-facing glue.

Local experimental files such as `playerbot_visibility.*` are not part of the established runtime path unless they are wired in explicitly.

## Core Architecture

Each bot is a `Player` plus a `BotController`.

- `g_bot.cpp` creates or restores the bot entity.
- `BotControllerManager` creates one `BotController` per bot player and ticks it every frame.
- `BotController::Think()` builds a `usercmd_t` and `usereyes_t`, then forwards both into `G_ClientThink`.
- `Player` delegates feed lifecycle and combat events back into the controller:
  - `delegate_spawned`
  - `delegate_killed`
  - `delegate_damage`
  - `delegate_gotKill`
  - `delegate_stufftext`

Keep controller logic server-side and deterministic. Avoid UI-style or menu-style flows in bot code unless the `Player` API requires it.

## Data Ownership Rules

Preserve the current separation of responsibilities:

- Per-frame decisions belong in `BotController` and the state files.
- Motion/path execution belongs in `BotMovement`.
- Aim smoothing and turn behavior belong in `BotRotation`.
- Spatial suspicion and patrol memory belong in `BotBeliefMap`.
- Personality tuning belongs in `BotProfile`. Profiles are data only.

Do not write behavior-specific logic into `BotProfile` loading code.

Grouped state structs in `playerbot.h` exist to avoid partial resets. Prefer extending those structs over scattering new ad hoc fields across the controller.

## State Machine Expectations

The main states are initialized in `BotController::Init()` and implemented in `playerbot_states.cpp`.

- Default idle/combat/curious/grenade behavior should continue to flow through `CheckStates()`.
- Event handlers like `Damaged()` and `NoticeEvent()` should write to sense or belief state first.
- State `Think` methods should be the place that converts those inputs into movement, aiming, and firing decisions.

Avoid direct movement or rotation writes from perception/event handlers unless there is a strong reason and the boundary is updated consistently everywhere.

## Spawn, Respawn, and Profiles

Current behavior:

- A bot profile is picked on the first spawn only.
- The chosen profile persists across later respawns within the same map.
- `preferredWeapon` is applied through `EV_Player_PrimaryDMWeapon`.
- The bot keeps its initial model instead of re-randomizing on each death.

If you change spawn behavior, verify all of:

- first spawn
- normal death/respawn
- team join / auto-join
- bot restore paths
- profile override cvars

Weapon preferences must remain compatible with the strings accepted by `Player::EventPrimaryDMWeapon`.

## Belief Map Rules

The belief map is persistent bot memory for likely enemy presence.

- Initialize it from map spawn bounds.
- Seed it from enemy spawn points on spawn.
- Update it from damage/noise/death events.
- Decay and visibility clearing happen every frame.

If you add new perception inputs, prefer feeding the belief map rather than adding separate one-off patrol heuristics.

## Editing Guidance

When changing bot behavior:

- Trace the full lifecycle, not just the local method.
- Check interactions with `Player` respawn and weapon-selection code.
- Preserve debug cvar behavior where it already exists.
- Prefer small, explicit state additions over hidden side effects.
- Keep comments factual and short. The bot code already carries a lot of historical commentary.

## Verification Checklist

After meaningful bot changes, validate at least these cases:

- Bot spawns and joins a team.
- Bot receives a valid weapon and can recover from banned weapon categories.
- Bot respawns without losing required persistent state.
- Bot still acquires enemies, moves, and fires.
- No delegate registration/removal regressions were introduced.
- No belief-map initialization regressions were introduced on fresh map load.

If you touch profile parsing or selection, also verify behavior with:

- no profile files present
- one profile file present
- multiple weighted profiles
- invalid or unsupported `preferredWeapon` values
