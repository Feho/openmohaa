# Task 2B.3: Complete Profile System

**Status:** Ready to Execute (after Task 2B.2)  
**Duration:** 8 hours  
**Priority:** HIGH  
**Phase:** 2B - Brain & Behavior

---

## Context & Background

### What This Task Achieves
Extends the bot profile system from Phase 2A to include combat, movement, personality, and behavior tree references. This allows complete bot personality definition in YAML without code changes.

### Why This Matters
- **Complete Bot Configuration:** Single YAML file defines all bot behavior
- **Designer Control:** Tweak combat aggressiveness, movement style, aim accuracy
- **Behavior Tree Selection:** Profiles specify which tree to use
- **Difficulty Scaling:** Easy/Medium/Hard/Expert variants via profiles

### What's Already Complete

**From Phase 2A:**
- ✅ BotProfile class with YAML parsing
- ✅ Perception parameters (vision_range, field_of_view, audio_sensitivity)
- ✅ 3 test profiles (sniper.yaml, rusher.yaml, test_perception.yaml)

**From Task 2B.1-2B.2:**
- ✅ Behavior tree framework
- ✅ YAML tree loading
- ✅ 2 behavior trees (engage_enemy, patrol)

### What's Still Needed
- Extend profiles with combat/movement/personality/aim parameters
- Add behavior_tree reference to profiles
- Create 5 complete profiles (aggressive, balanced, defensive, sniper, rusher)
- Update existing 3 profiles

---

## Technical Specification

### Extended Profile Structure

```yaml
# profiles/aggressive.yaml
profile:
  # === METADATA ===
  metadata:
    name: "Aggressive Rusher"
    description: "Charges enemies, high-risk high-reward playstyle"
    difficulty: medium
    author: "OpenMoHAA Team"
    version: "1.0"

  # === PERSONALITY ===
  # Influences utility AI decision-making (Phase 3)
  personality:
    aggression: 0.9       # 0.0 (passive) - 1.0 (aggressive)
    caution: 0.2          # 0.0 (reckless) - 1.0 (careful)
    teamwork: 0.5         # 0.0 (lone wolf) - 1.0 (squad-focused)
    creativity: 0.7       # 0.0 (predictable) - 1.0 (creative)

  # === COMBAT PARAMETERS ===
  combat:
    preferred_range: 256.0       # Ideal combat distance (units)
    fire_discipline: 0.3         # 0.0 (spray) - 1.0 (controlled bursts)
    burst_length: [0.5, 1.5]     # [min, max] seconds of continuous fire
    burst_delay: [0.1, 0.3]      # [min, max] seconds between bursts
    ammo_conservation: 0.2       # 0.0 (spray) - 1.0 (conservative)
    reload_under_fire: true      # Will reload while taking damage?

  # === MOVEMENT PARAMETERS ===
  movement:
    speed_preference: 1.3        # Multiplier on base speed
    crouch_frequency: 0.1        # 0.0 (never) - 1.0 (always)
    jump_frequency: 0.8          # How often to jump obstacles
    strafe_usage: 0.7            # How much to strafe in combat

  # === AIM PARAMETERS ===
  aim:
    reaction_time: [0.15, 0.35]  # [min, max] seconds to acquire target
    tracking_smoothness: 0.6     # 0.0 (instant snap) - 1.0 (smooth)
    spread_multiplier: 1.3       # Higher = less accurate
    headshot_bias: 0.4           # 0.0 (center mass) - 1.0 (headshots)

  # === TACTICAL PARAMETERS ===
  tactics:
    cover_usage: 0.3             # 0.0 (ignore cover) - 1.0 (always use)
    retreat_threshold: 0.15      # HP % before retreating
    flank_preference: 0.8        # Likelihood to flank
    grenade_frequency: 0.6       # How often to use grenades

  # === PERCEPTION (from Phase 2A) ===
  perception:
    vision:
      fov: 85.0
      range: 2048.0
      peripheral_range: 0.6
    hearing:
      range: 1024.0
      priority_threshold: 0.4

  # === BEHAVIOR TREE REFERENCE ===
  behavior_tree: "engage_enemy"  # Which tree to use
```

### Extend BotProfile Class

```cpp
// code/fgame/bot_profile.h (extend existing)

class BotProfile {
public:
    static BotProfile* LoadFromFile(const char* filepath);
    
    // === METADATA ===
    const char* GetName() const { return metadata.name.c_str(); }
    const char* GetDescription() const { return metadata.description.c_str(); }
    
    // === PERSONALITY ===
    float GetAggression() const { return personality.aggression; }
    float GetCaution() const { return personality.caution; }
    float GetTeamwork() const { return personality.teamwork; }
    float GetCreativity() const { return personality.creativity; }
    
    // === COMBAT ===
    float GetPreferredRange() const { return combat.preferredRange; }
    float GetFireDiscipline() const { return combat.fireDiscipline; }
    float GetBurstLengthMin() const { return combat.burstLength.first; }
    float GetBurstLengthMax() const { return combat.burstLength.second; }
    float GetBurstDelayMin() const { return combat.burstDelay.first; }
    float GetBurstDelayMax() const { return combat.burstDelay.second; }
    float GetAmmoConservation() const { return combat.ammoConservation; }
    bool GetReloadUnderFire() const { return combat.reloadUnderFire; }
    
    // === MOVEMENT ===
    float GetSpeedPreference() const { return movement.speedPreference; }
    float GetCrouchFrequency() const { return movement.crouchFrequency; }
    float GetJumpFrequency() const { return movement.jumpFrequency; }
    float GetStrafeUsage() const { return movement.strafeUsage; }
    
    // === AIM ===
    float GetReactionTimeMin() const { return aim.reactionTime.first; }
    float GetReactionTimeMax() const { return aim.reactionTime.second; }
    float GetTrackingSmoothness() const { return aim.trackingSmoothness; }
    float GetSpreadMultiplier() const { return aim.spreadMultiplier; }
    float GetHeadshotBias() const { return aim.headshotBias; }
    
    // === TACTICS ===
    float GetCoverUsage() const { return tactics.coverUsage; }
    float GetRetreatThreshold() const { return tactics.retreatThreshold; }
    float GetFlankPreference() const { return tactics.flankPreference; }
    float GetGrenadeFrequency() const { return tactics.grenadeFrequency; }
    
    // === PERCEPTION (existing from Phase 2A) ===
    float GetVisionRange() const { return perception.vision.range; }
    float GetVisionFOV() const { return perception.vision.fov; }
    // ... existing perception getters
    
    // === BEHAVIOR TREE ===
    const char* GetBehaviorTree() const { return behaviorTree.c_str(); }

private:
    struct Metadata {
        std::string name;
        std::string description;
        std::string difficulty;
        std::string author;
        std::string version;
    } metadata;
    
    struct Personality {
        float aggression = 0.5f;
        float caution = 0.5f;
        float teamwork = 0.5f;
        float creativity = 0.5f;
    } personality;
    
    struct Combat {
        float preferredRange = 512.0f;
        float fireDiscipline = 0.5f;
        std::pair<float, float> burstLength = {0.3f, 0.8f};
        std::pair<float, float> burstDelay = {0.2f, 0.5f};
        float ammoConservation = 0.5f;
        bool reloadUnderFire = false;
    } combat;
    
    struct Movement {
        float speedPreference = 1.0f;
        float crouchFrequency = 0.3f;
        float jumpFrequency = 0.3f;
        float strafeUsage = 0.5f;
    } movement;
    
    struct Aim {
        std::pair<float, float> reactionTime = {0.2f, 0.5f};
        float trackingSmoothness = 0.7f;
        float spreadMultiplier = 1.0f;
        float headshotBias = 0.5f;
    } aim;
    
    struct Tactics {
        float coverUsage = 0.5f;
        float retreatThreshold = 0.25f;
        float flankPreference = 0.5f;
        float grenadeFrequency = 0.3f;
    } tactics;
    
    struct Perception {
        // Existing from Phase 2A
        struct Vision {
            float fov = 80.0f;
            float range = 2048.0f;
            float peripheralRange = 0.6f;
        } vision;
        
        struct Hearing {
            float range = 1024.0f;
            float priorityThreshold = 0.5f;
        } hearing;
    } perception;
    
    std::string behaviorTree = "engage_enemy";
};
```

### Extended YAML Parsing

```cpp
// code/fgame/bot_profile.cpp (extend existing LoadFromFile)

BotProfile* BotProfile::LoadFromFile(const char* filepath) {
    try {
        YAML::Node root = YAML::LoadFile(filepath);
        
        if (!root["profile"]) {
            gi.Printf("ERROR: Profile missing 'profile' root: %s\n", filepath);
            return nullptr;
        }
        
        YAML::Node profile = root["profile"];
        BotProfile* bp = new BotProfile();
        
        // === METADATA ===
        if (profile["metadata"]) {
            YAML::Node meta = profile["metadata"];
            if (meta["name"]) bp->metadata.name = meta["name"].as<std::string>();
            if (meta["description"]) bp->metadata.description = meta["description"].as<std::string>();
            if (meta["difficulty"]) bp->metadata.difficulty = meta["difficulty"].as<std::string>();
            if (meta["author"]) bp->metadata.author = meta["author"].as<std::string>();
            if (meta["version"]) bp->metadata.version = meta["version"].as<std::string>();
        }
        
        // === PERSONALITY ===
        if (profile["personality"]) {
            YAML::Node pers = profile["personality"];
            if (pers["aggression"]) bp->personality.aggression = pers["aggression"].as<float>();
            if (pers["caution"]) bp->personality.caution = pers["caution"].as<float>();
            if (pers["teamwork"]) bp->personality.teamwork = pers["teamwork"].as<float>();
            if (pers["creativity"]) bp->personality.creativity = pers["creativity"].as<float>();
        }
        
        // === COMBAT ===
        if (profile["combat"]) {
            YAML::Node combat = profile["combat"];
            if (combat["preferred_range"]) 
                bp->combat.preferredRange = combat["preferred_range"].as<float>();
            if (combat["fire_discipline"]) 
                bp->combat.fireDiscipline = combat["fire_discipline"].as<float>();
            if (combat["burst_length"]) {
                auto bl = combat["burst_length"];
                bp->combat.burstLength = {bl[0].as<float>(), bl[1].as<float>()};
            }
            if (combat["burst_delay"]) {
                auto bd = combat["burst_delay"];
                bp->combat.burstDelay = {bd[0].as<float>(), bd[1].as<float>()};
            }
            if (combat["ammo_conservation"]) 
                bp->combat.ammoConservation = combat["ammo_conservation"].as<float>();
            if (combat["reload_under_fire"]) 
                bp->combat.reloadUnderFire = combat["reload_under_fire"].as<bool>();
        }
        
        // === MOVEMENT ===
        if (profile["movement"]) {
            YAML::Node move = profile["movement"];
            if (move["speed_preference"]) 
                bp->movement.speedPreference = move["speed_preference"].as<float>();
            if (move["crouch_frequency"]) 
                bp->movement.crouchFrequency = move["crouch_frequency"].as<float>();
            if (move["jump_frequency"]) 
                bp->movement.jumpFrequency = move["jump_frequency"].as<float>();
            if (move["strafe_usage"]) 
                bp->movement.strafeUsage = move["strafe_usage"].as<float>();
        }
        
        // === AIM ===
        if (profile["aim"]) {
            YAML::Node aim = profile["aim"];
            if (aim["reaction_time"]) {
                auto rt = aim["reaction_time"];
                bp->aim.reactionTime = {rt[0].as<float>(), rt[1].as<float>()};
            }
            if (aim["tracking_smoothness"]) 
                bp->aim.trackingSmoothness = aim["tracking_smoothness"].as<float>();
            if (aim["spread_multiplier"]) 
                bp->aim.spreadMultiplier = aim["spread_multiplier"].as<float>();
            if (aim["headshot_bias"]) 
                bp->aim.headshotBias = aim["headshot_bias"].as<float>();
        }
        
        // === TACTICS ===
        if (profile["tactics"]) {
            YAML::Node tac = profile["tactics"];
            if (tac["cover_usage"]) 
                bp->tactics.coverUsage = tac["cover_usage"].as<float>();
            if (tac["retreat_threshold"]) 
                bp->tactics.retreatThreshold = tac["retreat_threshold"].as<float>();
            if (tac["flank_preference"]) 
                bp->tactics.flankPreference = tac["flank_preference"].as<float>();
            if (tac["grenade_frequency"]) 
                bp->tactics.grenadeFrequency = tac["grenade_frequency"].as<float>();
        }
        
        // === PERCEPTION (existing from Phase 2A) ===
        if (profile["perception"]) {
            // ... existing perception parsing code
        }
        
        // === BEHAVIOR TREE ===
        if (profile["behavior_tree"]) {
            bp->behaviorTree = profile["behavior_tree"].as<std::string>();
        }
        
        // Validate profile
        if (!ValidateProfile(bp)) {
            delete bp;
            return nullptr;
        }
        
        gi.DPrintf("Loaded profile: %s\n", bp->GetName());
        return bp;
        
    } catch (const YAML::Exception& e) {
        gi.Printf("ERROR: YAML parse error in %s: %s\n", filepath, e.what());
        return nullptr;
    }
}

// Validate profile values are in expected ranges
bool BotProfile::ValidateProfile(BotProfile* profile) {
    bool valid = true;
    
    // Validate personality (0.0 - 1.0)
    if (profile->personality.aggression < 0.0f || profile->personality.aggression > 1.0f) {
        gi.Printf("WARNING: aggression out of range [0-1]: %.2f\n", profile->personality.aggression);
        valid = false;
    }
    
    // Validate combat
    if (profile->combat.preferredRange < 0.0f) {
        gi.Printf("WARNING: preferred_range cannot be negative: %.2f\n", profile->combat.preferredRange);
        valid = false;
    }
    
    // Validate burst times
    if (profile->combat.burstLength.first > profile->combat.burstLength.second) {
        gi.Printf("WARNING: burst_length min > max\n");
        valid = false;
    }
    
    // Add more validations as needed...
    
    return valid;
}
```

---

## Implementation Steps

### Step 1: Extend bot_profile.h (2 hours)
1. Add new structs: Personality, Combat, Movement, Aim, Tactics
2. Add getter methods for all parameters
3. Update class structure

### Step 2: Extend bot_profile.cpp (3 hours)
1. Update LoadFromFile() to parse new sections
2. Add default values for all parameters
3. Implement ValidateProfile()
4. Add helpful error messages

### Step 3: Create 5 Complete Profiles (2 hours)
1. profiles/aggressive.yaml - Close range, high aggression
2. profiles/balanced.yaml - Medium all-around
3. profiles/defensive.yaml - Cautious, cover-focused
4. profiles/sniper.yaml - Long range, patient (update existing)
5. profiles/rusher.yaml - Fast, aggressive (update existing)

### Step 4: Testing (1 hour)
1. Load each profile
2. Verify all parameters parsed correctly
3. Test validation (invalid values)
4. Test missing sections (use defaults)

---

## Complete Profile Examples

### aggressive.yaml
```yaml
profile:
  metadata:
    name: "Aggressive Rusher"
    description: "Charges enemies, high-risk playstyle"
    difficulty: medium

  personality:
    aggression: 0.9
    caution: 0.2
    teamwork: 0.5
    creativity: 0.7

  combat:
    preferred_range: 256.0
    fire_discipline: 0.3
    burst_length: [0.5, 1.5]
    burst_delay: [0.1, 0.3]
    ammo_conservation: 0.2
    reload_under_fire: true

  movement:
    speed_preference: 1.3
    crouch_frequency: 0.1
    jump_frequency: 0.8
    strafe_usage: 0.7

  aim:
    reaction_time: [0.15, 0.35]
    tracking_smoothness: 0.6
    spread_multiplier: 1.3
    headshot_bias: 0.4

  tactics:
    cover_usage: 0.3
    retreat_threshold: 0.15
    flank_preference: 0.8
    grenade_frequency: 0.6

  perception:
    vision:
      fov: 85.0
      range: 2048.0
      peripheral_range: 0.6
    hearing:
      range: 1024.0
      priority_threshold: 0.4

  behavior_tree: "engage_enemy"
```

### balanced.yaml
```yaml
profile:
  metadata:
    name: "Balanced Soldier"
    description: "Well-rounded, no extreme behaviors"
    difficulty: medium

  personality:
    aggression: 0.5
    caution: 0.5
    teamwork: 0.6
    creativity: 0.5

  combat:
    preferred_range: 512.0
    fire_discipline: 0.5
    burst_length: [0.3, 0.8]
    burst_delay: [0.2, 0.5]
    ammo_conservation: 0.5
    reload_under_fire: false

  movement:
    speed_preference: 1.0
    crouch_frequency: 0.4
    jump_frequency: 0.4
    strafe_usage: 0.5

  aim:
    reaction_time: [0.2, 0.5]
    tracking_smoothness: 0.7
    spread_multiplier: 1.0
    headshot_bias: 0.5

  tactics:
    cover_usage: 0.5
    retreat_threshold: 0.25
    flank_preference: 0.5
    grenade_frequency: 0.4

  perception:
    vision:
      fov: 80.0
      range: 2048.0
      peripheral_range: 0.6
    hearing:
      range: 1024.0
      priority_threshold: 0.5

  behavior_tree: "engage_enemy"
```

### defensive.yaml
```yaml
profile:
  metadata:
    name: "Defensive Guardian"
    description: "Cautious, cover-focused, patient"
    difficulty: medium

  personality:
    aggression: 0.3
    caution: 0.9
    teamwork: 0.7
    creativity: 0.4

  combat:
    preferred_range: 768.0
    fire_discipline: 0.8
    burst_length: [0.2, 0.5]
    burst_delay: [0.3, 0.7]
    ammo_conservation: 0.7
    reload_under_fire: false

  movement:
    speed_preference: 0.8
    crouch_frequency: 0.7
    jump_frequency: 0.2
    strafe_usage: 0.4

  aim:
    reaction_time: [0.3, 0.6]
    tracking_smoothness: 0.8
    spread_multiplier: 0.8
    headshot_bias: 0.6

  tactics:
    cover_usage: 0.9
    retreat_threshold: 0.4
    flank_preference: 0.2
    grenade_frequency: 0.3

  perception:
    vision:
      fov: 90.0
      range: 2500.0
      peripheral_range: 0.7
    hearing:
      range: 1200.0
      priority_threshold: 0.4

  behavior_tree: "engage_enemy"
```

### sniper.yaml (updated)
```yaml
profile:
  metadata:
    name: "Sniper"
    description: "Long range, patient, accurate"
    difficulty: hard

  personality:
    aggression: 0.4
    caution: 0.8
    teamwork: 0.5
    creativity: 0.6

  combat:
    preferred_range: 1536.0
    fire_discipline: 0.9
    burst_length: [0.1, 0.3]
    burst_delay: [0.5, 1.0]
    ammo_conservation: 0.8
    reload_under_fire: false

  movement:
    speed_preference: 0.9
    crouch_frequency: 0.8
    jump_frequency: 0.1
    strafe_usage: 0.3

  aim:
    reaction_time: [0.2, 0.4]
    tracking_smoothness: 0.9
    spread_multiplier: 0.6
    headshot_bias: 0.9

  tactics:
    cover_usage: 0.8
    retreat_threshold: 0.3
    flank_preference: 0.4
    grenade_frequency: 0.2

  perception:
    vision:
      fov: 60.0
      range: 3072.0
      peripheral_range: 0.5
    hearing:
      range: 1024.0
      priority_threshold: 0.6

  behavior_tree: "engage_enemy"
```

### rusher.yaml (updated)
```yaml
profile:
  metadata:
    name: "Close Quarters Rusher"
    description: "Fast, aggressive, close range"
    difficulty: medium

  personality:
    aggression: 0.95
    caution: 0.1
    teamwork: 0.4
    creativity: 0.8

  combat:
    preferred_range: 192.0
    fire_discipline: 0.2
    burst_length: [0.8, 2.0]
    burst_delay: [0.1, 0.2]
    ammo_conservation: 0.1
    reload_under_fire: true

  movement:
    speed_preference: 1.5
    crouch_frequency: 0.05
    jump_frequency: 0.9
    strafe_usage: 0.9

  aim:
    reaction_time: [0.1, 0.25]
    tracking_smoothness: 0.5
    spread_multiplier: 1.5
    headshot_bias: 0.3

  tactics:
    cover_usage: 0.2
    retreat_threshold: 0.1
    flank_preference: 0.9
    grenade_frequency: 0.7

  perception:
    vision:
      fov: 90.0
      range: 1536.0
      peripheral_range: 0.7
    hearing:
      range: 800.0
      priority_threshold: 0.3

  behavior_tree: "engage_enemy"
```

---

## Testing Requirements

### Unit Tests
```cpp
// tests/test_bot_profile.cpp (add to existing)

TEST(BotProfileTest, LoadCompleteProfile) {
    BotProfile* profile = BotProfile::LoadFromFile("profiles/aggressive.yaml");
    
    ASSERT_NE(profile, nullptr);
    
    // Test metadata
    EXPECT_STREQ(profile->GetName(), "Aggressive Rusher");
    
    // Test personality
    EXPECT_FLOAT_EQ(profile->GetAggression(), 0.9f);
    EXPECT_FLOAT_EQ(profile->GetCaution(), 0.2f);
    
    // Test combat
    EXPECT_FLOAT_EQ(profile->GetPreferredRange(), 256.0f);
    EXPECT_FLOAT_EQ(profile->GetFireDiscipline(), 0.3f);
    
    // Test movement
    EXPECT_FLOAT_EQ(profile->GetSpeedPreference(), 1.3f);
    
    // Test aim
    EXPECT_FLOAT_EQ(profile->GetSpreadMultiplier(), 1.3f);
    
    // Test tactics
    EXPECT_FLOAT_EQ(profile->GetCoverUsage(), 0.3f);
    
    // Test behavior tree reference
    EXPECT_STREQ(profile->GetBehaviorTree(), "engage_enemy");
    
    delete profile;
}

TEST(BotProfileTest, ValidationDetectsErrors) {
    // Create profile with invalid values
    // ... test validation
}

TEST(BotProfileTest, MissingSectionsUseDefaults) {
    // Create profile with missing sections
    // Verify defaults are used
}
```

---

## Files to Create/Modify

### New Files
- `profiles/aggressive.yaml`
- `profiles/balanced.yaml`
- `profiles/defensive.yaml`

### Modified Files
- `code/fgame/bot_profile.h` - Add new parameter structs and getters
- `code/fgame/bot_profile.cpp` - Extend YAML parsing
- `profiles/sniper.yaml` - Add new sections
- `profiles/rusher.yaml` - Add new sections
- `tests/test_bot_profile.cpp` - Add tests for new parameters

---

## Acceptance Criteria

- [ ] BotProfile class extended with 5 parameter categories
- [ ] All parameters have getters
- [ ] YAML parsing handles all sections
- [ ] 5 complete profiles created
- [ ] Profile validation detects invalid values
- [ ] Missing sections use sensible defaults
- [ ] 3 new unit tests pass (load, validate, defaults)
- [ ] All profiles load without errors
- [ ] Behavior tree reference properly stored

---

## Next Steps (Task 2B.4)

With complete profiles, Task 2B.4 will:
1. Integrate profiles with BotController
2. Load behavior tree specified in profile
3. Use profile parameters in bot behavior
4. Add console commands (bot_setprofile, bot_listprofiles)
5. Add hot-reload support

This completes the data-driven configuration system - bots are now fully configurable via YAML without code changes.
