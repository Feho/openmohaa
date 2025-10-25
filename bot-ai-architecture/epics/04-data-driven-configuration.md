# Epic 4: Data-Driven Configuration

## Overview
Move bot behavior parameters from hardcoded cvars to YAML configuration files, enabling designers to create/modify bot profiles without programming.

## Business Value
- **Designer Empowerment:** Non-programmers create bot variations
- **Rapid Iteration:** Change configs without recompile
- **Version Control:** Track balance changes in git
- **A/B Testing:** Easy to test different profiles
- **Hot-Reload:** Tweak values in real-time during testing

## Current State
```cpp
// Hardcoded cvars scattered throughout code
cvar_t* g_bot_attack_react_min_delay;
cvar_t* g_bot_attack_react_random_delay;
cvar_t* g_bot_attack_spreadmult;
cvar_t* g_bot_turn_speed;
// ... 20+ more
```

**Problems:**
- All bots share same cvars (no personality variation)
- Changing values requires restart
- No grouping (combat params mixed with movement params)
- Magic numbers in code

## Target State
```yaml
# profiles/aggressive.yaml
profile:
  name: "Aggressive"

  personality:
    aggression: 0.9
    caution: 0.2
    teamwork: 0.6

  combat:
    preferred_range: 256
    fire_discipline: 0.3
    reaction_time: [0.1, 0.3]
    spread_multiplier: 1.2

  movement:
    speed_preference: 1.2
    jump_frequency: 0.8
    crouch_usage: 0.1

  aim:
    tracking_smoothness: 0.6
    headshot_bias: 0.4
```

```cpp
class BotProfile {
    static BotProfile LoadFromFile(const char* path);

    float GetAggression() const;
    float GetReactionTime() const;
    // ...
};
```

## Acceptance Criteria
- [ ] YAML parsing library integrated
- [ ] BotProfile class loads from files
- [ ] 5 default profiles (aggressive, balanced, defensive, sniper, rusher)
- [ ] Runtime profile switching via console command
- [ ] Hot-reload on file change (dev mode)
- [ ] All behavior parameters use profile (no hardcoded cvars)

## Technical Components

### Profile Categories
- **Personality:** Core behavioral traits (aggression, caution, creativity)
- **Combat:** Fighting parameters (range, discipline, burst timing)
- **Movement:** Mobility preferences (speed, jump, crouch)
- **Aim:** Targeting parameters (reaction time, accuracy, bias)
- **Tactics:** Strategic choices (cover usage, flank preference, retreat threshold)

### YAML Structure
```yaml
profile:
  metadata:
    name: "Profile Name"
    description: "What this bot is good at"
    difficulty: medium  # easy, medium, hard, expert

  personality:
    aggression: 0.5       # 0.0 (passive) - 1.0 (aggressive)
    caution: 0.5          # 0.0 (reckless) - 1.0 (careful)
    teamwork: 0.5         # 0.0 (solo) - 1.0 (squad-focused)
    creativity: 0.5       # 0.0 (predictable) - 1.0 (creative)

  combat:
    preferred_range: 512
    min_range_factor: 0.5
    max_range_factor: 1.5
    fire_discipline: 0.5
    burst_length: [0.5, 1.5]
    burst_delay: [0.1, 0.5]
    ammo_conservation: 0.5

  # ... more categories
```

## Dependencies
- YAML parsing library (yaml-cpp recommended)
- File watching for hot-reload (optional)

## Related Epics
- Epic 1 (Behavior Trees can be defined in YAML)
- Epic 3 (Utility AI curves defined in YAML)

## References
- yaml-cpp library documentation
- [Data-Driven Game Development (GDC)](https://www.gdcvault.com/)
