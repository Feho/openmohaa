# User Stories: Data-Driven Configuration

## Epic: Data-Driven Configuration
Related to: `epics/04-data-driven-configuration.md`

---

## Story 1: YAML Bot Profiles
**As a** game designer
**I want** to create bot personalities via YAML files
**So that** I can tweak bot behavior without programming

### Acceptance Criteria
- [ ] Bot profiles loaded from `profiles/*.yaml`
- [ ] Covers: personality, combat, movement, aim, tactics
- [ ] Clear documentation of all parameters
- [ ] Validation with helpful error messages

### Priority: HIGH
### Estimate: 3 days

---

## Story 2: Pre-Built Profile Library
**As a** game designer
**I want** 5-10 pre-built bot profiles
**So that** I have good starting points for customization

### Acceptance Criteria
- [ ] Aggressive profile (high aggression, low caution)
- [ ] Defensive profile (high caution, uses cover)
- [ ] Balanced profile (middle ground)
- [ ] Sniper profile (long range, patient)
- [ ] Rusher profile (close range, fast, reckless)
- [ ] Each profile documented with playstyle description

### Priority: MEDIUM
### Estimate: 2 days

---

## Story 3: Hot-Reload Profiles
**As a** game designer
**I want** profile changes to apply immediately
**So that** I can iterate quickly during tuning

### Acceptance Criteria
- [ ] File watcher detects profile changes
- [ ] Bots reload profiles without restart
- [ ] Console command to force reload
- [ ] Validation errors shown without crashing

### Priority: MEDIUM
### Estimate: 2 days

---

## Story 4: Runtime Profile Switching
**As a** developer
**I want** to change bot profile via console command
**So that** I can test different profiles easily

### Acceptance Criteria
- [ ] `bot_setprofile <botnum> <profile_name>`
- [ ] Lists available profiles
- [ ] Immediate behavior change
- [ ] Works on dedicated servers

### Priority: MEDIUM
### Estimate: 1 day

---

## Story 5: Difficulty Scaling
**As a** player
**I want** different difficulty levels to use different bot profiles
**So that** easy bots are forgiving, hard bots are challenging

### Acceptance Criteria
- [ ] Easy: slower reactions, worse aim, predictable
- [ ] Medium: balanced reactions and aim
- [ ] Hard: fast reactions, good aim, tactical
- [ ] Expert: near-perfect reactions, excellent aim, creative tactics
- [ ] `g_bot_difficulty` cvar selects profile set

### Priority: HIGH
### Estimate: 2 days

---

## Story 6: Per-Bot Profile Variation
**As a** player
**I want** bots in same match to have different personalities
**So that** combat feels varied and interesting

### Acceptance Criteria
- [ ] Bots randomly assigned profiles from pool
- [ ] Mix of aggressive/defensive/balanced in same match
- [ ] Configurable profile distribution (e.g., 30% aggressive, 50% balanced, 20% defensive)

### Priority: MEDIUM
### Estimate: 2 days

---

## Story 7: Profile Inheritance
**As a** game designer
**I want** profiles to inherit from base profiles
**So that** I can create variations without duplicating everything

### Acceptance Criteria
- [ ] YAML supports `inherits: base_profile`
- [ ] Override specific parameters
- [ ] Multiple inheritance levels supported
- [ ] Example: AggressiveSniper inherits Aggressive + Sniper

### Priority: LOW
### Estimate: 2 days

---

## Story 8: Validation & Error Handling
**As a** game designer
**I want** clear error messages when profiles are invalid
**So that** I can fix issues quickly

### Acceptance Criteria
- [ ] Type validation (e.g., aggression must be 0.0-1.0)
- [ ] Required field validation
- [ ] Helpful error messages with line numbers
- [ ] Fallback to default profile if load fails

### Priority: MEDIUM
### Estimate: 2 days

---

## Story 9: Profile Editor Tool (Stretch Goal)
**As a** game designer
**I want** a GUI tool to edit bot profiles
**So that** I don't have to hand-edit YAML

### Acceptance Criteria
- [ ] Sliders for numeric values
- [ ] Dropdowns for enums
- [ ] Live preview (spawn test bot)
- [ ] Save to YAML
- [ ] Load existing profiles

### Priority: LOW
### Estimate: 5 days

---

## Story 10: Documentation
**As a** game designer
**I want** comprehensive documentation of all profile parameters
**So that** I understand what each setting does

### Acceptance Criteria
- [ ] Reference doc listing all parameters
- [ ] Each parameter has description, range, effect
- [ ] Examples showing common configurations
- [ ] Tuning guide (how to achieve specific playstyles)

### Priority: MEDIUM
### Estimate: 2 days

---

## Total Stories: 10
## Total Estimated Time: 23 days (excluding stretch goal editor)
