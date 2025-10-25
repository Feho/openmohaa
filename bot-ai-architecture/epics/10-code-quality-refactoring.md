# Epic 10: Code Quality & Refactoring

## Overview
Improve code quality through systematic refactoring: extract functions, improve naming, add const-correctness, use modern C++ features.

## Business Value
- **Maintainability:** Easier to understand and modify
- **Onboarding:** New developers productive faster
- **Bugs:** Clean code = fewer bugs
- **Pride:** Enjoyable to work with

## Current State
- 200+ line functions
- Mixed abstraction levels
- Unclear naming (Hungarian notation)
- Manual memory management
- C-style code

## Target State
- Functions < 50 lines
- Clear, self-documenting names
- Smart pointers, RAII
- Modern C++ (auto, lambdas, ranges)
- Const-correctness

## Acceptance Criteria
- [ ] Average function length < 50 lines (target: < 30)
- [ ] All public APIs const-correct
- [ ] Smart pointers for all dynamic allocation
- [ ] Zero compile warnings
- [ ] All magic numbers replaced with named constants
- [ ] Consistent code style (clang-format)

## Refactoring Targets

### 1. Extract Long Functions
**Before:**
```cpp
void BotController::State_Attack() {
    // 200+ lines of mixed logic
}
```

**After:**
```cpp
void BotController::State_Attack() {
    if (!ValidateAttackPreconditions()) return;

    auto* target = SelectBestTarget();
    if (!target) return;

    AimAtTarget(target);
    UpdateAttackMovement(target);
    ExecuteFiring(target);
}
```

### 2. Improve Naming
**Before:**
```cpp
float m_fRecentDamage;
int m_iDamageWindowStart;
SafePtr<Sentient> m_pEnemy;
```

**After:**
```cpp
float recentDamageAccumulated;
TimePoint damageTrackingStartTime;
Sentient* currentEnemy;  // SafePtr internally
```

### 3. Modern C++
**Before:**
```cpp
Container<Sentient*> enemies;
for (int i = 0; i < enemies.NumObjects(); i++) {
    Sentient* sent = enemies.ObjectAt(i);
    if (IsValidEnemy(sent)) {
        // ...
    }
}
```

**After:**
```cpp
std::vector<Sentient*> enemies;
auto validEnemies = enemies
    | std::views::filter([](auto* e) { return IsValidEnemy(e); })
    | std::views::transform([](auto* e) { return EnemyInfo(e); });
```

### 4. Smart Pointers
**Before:**
```cpp
IPather* m_pPath;
~BotMovement() {
    delete m_pPath;
}
```

**After:**
```cpp
std::unique_ptr<IPather> pathfinder;
// Automatically cleaned up
```

### 5. Const-Correctness
```cpp
// Before
bool CanSee(Sentient* enemy);

// After
bool CanSee(const Sentient* enemy) const;
```

## Success Metrics
- **Readability:** New developer understands code in < 1 hour
- **Function Length:** 95% < 50 lines
- **Warnings:** Zero compiler warnings
- **Code Coverage:** Tests cover 80%+ of code

## Related Epics
All other epics benefit from clean code foundation.

## References
- [Clean Code by Robert Martin](https://www.amazon.com/Clean-Code-Handbook-Software-Craftsmanship/dp/0132350882)
- [Effective Modern C++](https://www.oreilly.com/library/view/effective-modern-c/9781491908419/)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
