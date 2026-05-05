# Bot System Guide

This directory contains the current multiplayer bot system. Use this file as the working contract when changing bot behavior in `code/fgame/`.

## Scope

The active bot system lives primarily in:

- `playerbot.h` / `playerbot.cpp`: `BotController`, intent types, mode transitions, event hooks, frame updates, weapon selection, spawn/death handling.
- `playerbot_movement.cpp`: pathing, obstacle handling, jump logic, attractive nodes.
- `playerbot_rotation.cpp`: aim and turn dynamics.
- `playerbot_beliefs.h` / `playerbot_beliefs.cpp`: belief-map storage and patrol targeting.
- `playerbot_strategy.h` / `playerbot_strategy.cpp`: strategy helpers layered on top of controller state.
- `playerbot_profile.h` / `playerbot_profile.cpp`: `BotProfile` and `BotProfileManager` — per-bot behavioral parameters loaded from `main/bots/profiles/*.cfg`.
- `playerbot_tactical_memory.h` / `playerbot_tactical_memory.cpp`: `BotTacticalMemory` — overwatch spot storage and scoring, shared across both teams per map.
- `playerbot_master.cpp`: controller manager ownership and ticking.
- `g_bot.cpp` / `g_bot.h`: bot creation, restoration, removal, and game-facing glue.

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

- Durable combat/tactical/perception state updates belong in `BotController` refresh/advance stages.
- Per-frame decisions belong in `BotController` intent builders and resolver code.
- Motion/path execution belongs in `BotMovement`.
- Aim smoothing and turn behavior belong in `BotRotation`.
- Spatial suspicion and patrol memory belong in `BotBeliefMap`.
- Weapon selection and spawn persistence belong in `BotController` and `g_bot.cpp` (via userinfo).

Grouped state structs in `playerbot.h` exist to avoid partial resets. Prefer extending those structs over scattering new ad hoc fields across the controller.

## Intent Pipeline Expectations

The current runtime path is layered:

- `RefreshPerceptionState()` is the durable state update pass for attack/curious/grenade/overwatch tracking.
- `BuildPerceptionSnapshot()` is read-only and gathers condition results plus short-lived reaction data from current state.
- `UpdateModeTransitions()` updates explicit engagement/tactical/hazard modes for debug visibility and state cleanup.
- `AdvanceCombatStateAndBuildIntent()`, `BuildHazardIntent()`, and `AdvanceTacticalStateAndBuildIntent()` each produce a per-frame intent value while advancing their owned controller state.
- `ResolveIntents()` is the single place that decides precedence between layers.
- `ExecuteResolvedCommand()` is the only place that should write final move goals, aim directives, lean/run flags, and attack buttons.

Avoid direct movement or rotation writes from perception/event handlers. Event handlers should update durable state, belief state, enemies, timers, or short-lived reaction inputs, then let the next frame's resolver decide the final command.

## Layer Ownership Rules

Keep layer responsibilities explicit:

- Combat owns target choice, fire requests, combat strafing, crouch/lean preferences, and combat-facing aim targets.
- Tactical owns idle patrol, curious investigation movement, overwatch anchoring, and non-combat look behavior.
- Hazard owns safety-driven movement overrides such as grenade avoidance.
- Movement and rotation remain execution backends; they should consume resolved requests, not absorb competing decisions from multiple call sites.

If two features need the same output, resolve the conflict in `ResolveIntents()` instead of letting both write directly to `movement`, `rotation`, or `m_botCmd`.

For bot-controller movement changes, run the guarded helper from the repository root:

```bash
./misc/check-bot-build.sh
```

The script enforces the direct `movement.*` side-effect allowlist before building `game` with Ninja and redirecting compiler output to `build.log`. Update the allowlist only when a new direct movement mutation is intentionally part of the execution/script/lifecycle boundary.

## Spawn, Respawn, and Persistence

Current behavior:

- Weapon selection is handled in `BotController` (`CheckValidWeapon`, `FindWeaponWithAmmo`).
- Model and team are persisted across map restarts via `G_SaveBots`/`G_RestoreBots` in `g_bot.cpp`, which saves and restores the full `userinfo`.
- The bot keeps its initial model instead of re-randomizing on each death.

If you change spawn behavior, verify all of:

- first spawn
- normal death/respawn
- team join / auto-join
- bot restore paths

Weapon selection must remain compatible with the strings accepted by `Player::EventPrimaryDMWeapon`.

## Belief Map Rules

The belief map is persistent bot memory for likely enemy presence.

- Initialize it from map spawn bounds.
- Seed it from enemy spawn points on spawn.
- Update it from damage/noise/death events.
- Decay and visibility clearing happen every frame.

If you add new perception inputs, prefer feeding the belief map rather than adding separate one-off patrol heuristics.

## Stuck Detection and Zone Banning

`BotMovement` runs a sliding-window stuck check every second. When the bot's total displacement over the window falls below a threshold it is considered stuck. On getting stuck the movement layer calls back into `BotController`, which records a `BotBannedZone` (`m_bannedZones[]`) centered on the current position. Path planning skips waypoints whose closest approach falls inside any active banned zone. Bans expire after `BOT_BANNED_ZONE_DURATION_MS`; the oldest slot is recycled when the fixed-size array is full.

Key invariants:
- Stuck detection lives in `BotMovement`; zone recording lives in `BotController`.
- Do not add new stuck workarounds outside this path — feed them through the same ban mechanism.
- Chase recovery (re-engaging an enemy after losing LOS) is separate from stuck recovery and is managed in `AdvanceCombatStateAndBuildIntent()`.

## Weapon Selection API

`Player::EventPrimaryDMWeapon` accepts these strings: `"shotgun"`, `"rifle"`, `"sniper"`, `"smg"`, `"mg"`, `"heavy"`, `"landmine"`, `"auto"`. A banned category leaves `pers.dm_primary` empty without error — callers must retry with `"auto"` if the first attempt fails (check `pers.dm_primary[0]` after the event).

## Cvar-Dependent Initialization

Global constructors in fgame run before `G_InitCvars()` registers cvars. Any class that reads cvar values at construction time will see null pointers. Defer cvar reads to a method called from `BotManager::Init()` (or equivalent post-cvar hook), not from the constructor.

## Bot Profile System

Each bot is assigned a `BotProfile` at first spawn (`m_bFirstSpawn`) and retains it for its lifetime. Profiles are loaded by `BotProfileManager::LoadProfiles("bots/profiles")` called from `BotManager::Init()`.

- Profile files live in `main/bots/profiles/*.cfg` — flat `key value` format, `//` and `#` comments
- `BotProfileManager::PickProfile()` does weighted random selection; `g_bot_profile_override` forces a named profile for testing
- The default profile (used when no files exist) is populated from live `g_bot_*` cvar values inside `LoadProfiles()`, preserving any server-operator tuning
- Profile fields cover: aim tuning (fed into `BotRotation` via `SetAimParameters()`), combat timing, weapon preference, crouch chance, and aim spread
- Weapon preference falls back to `"auto"` if the preferred category is banned — see Weapon Selection API above

## Editing Guidance

When changing bot behavior:

- Trace the full lifecycle, not just the local method.
- Check interactions with `Player` respawn and weapon-selection code.
- Preserve debug cvar behavior where it already exists.
- Prefer small, explicit state additions or intent fields over hidden side effects.
- Keep snapshot construction read-only. If state must advance, do it in the explicit refresh/advance stages instead of query helpers.
- Keep decisions as data where practical: compute intent first, mutate engine-facing objects at the edge.
- Make impossible combinations harder to represent by extending the explicit mode/intent structs instead of adding unrelated controller booleans.
- Keep comments factual and short. The bot code already carries a lot of historical commentary.

## Verification Checklist

After meaningful bot changes, validate at least these cases:

- `./misc/check-bot-build.sh` passes for bot-controller movement changes.
- Bot spawns and joins a team.
- Bot receives a valid weapon and can recover from banned weapon categories.
- Bot respawns without losing required persistent state.
- Bot still acquires enemies, moves, and fires.
- No delegate registration/removal regressions were introduced.
- No belief-map initialization regressions were introduced on fresh map load.

If you touch weapon selection or bot restore, also verify invalid or unsupported weapon strings in `CheckValidWeapon`.
