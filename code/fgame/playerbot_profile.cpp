// playerbot_profile.cpp: Bot personality profile loading and selection.

#include "g_local.h"
#include "gamecvars.h"
#include "playerbot_profile.h"

BotProfileManager botProfileManager;

static void ClampVisionProfile(BotProfile& profile);

BotProfileManager::BotProfileManager()
{
    // InitDefault() is deferred to LoadProfiles() so that cvars are already
    // registered when we read them.  Until then, leave m_default zeroed.
}

void BotProfileManager::InitDefault()
{
    // Mirror the live cvar values so that servers which tune bots through
    // g_bot_* cvars continue to see their settings applied to bots that
    // are not covered by any profile file.
    m_default.name                     = "default";
    m_default.weight                   = 1.0f;
    m_default.turnSpeed                = (float)g_bot_turn_speed->integer;
    m_default.aimNoise                 = g_bot_aim_noise->value;
    m_default.aimOvershoot             = g_bot_aim_overshoot->value;
    m_default.aimSettleSpeed           = g_bot_aim_settle_speed->value;
    m_default.aimLerpSpeed             = g_bot_aim_lerp_speed->value;
    m_default.aimSpreadMult            = g_bot_attack_spreadmult->value;
    m_default.reactionMinDelay         = g_bot_attack_react_min_delay->value;
    m_default.reactionRandomDelay      = g_bot_attack_react_random_delay->value;
    m_default.burstMinTime             = g_bot_attack_burst_min_time->value;
    m_default.burstRandomDelay         = g_bot_attack_burst_random_delay->value;
    m_default.continuousFireMinTime    = g_bot_attack_continuousfire_min_firetime->value;
    m_default.continuousFireRandomTime = g_bot_attack_continuousfire_random_firetime->value;
    m_default.crouchChance             = (float)g_bot_crouch_chance->integer;
    m_default.longRangeStrafeChance    = 100.0f;
    m_default.preferredWeapon          = "auto";
    m_default.visionDistanceMult       = 1.0f;
    m_default.spotImmediateFov         = 40.0f;
    m_default.spotLikelyFov            = 80.0f;
    m_default.spotPeripheralFov        = 120.0f;
    m_default.spotAwarenessThreshold   = 1.0f;
    m_default.spotLikelyRate           = 1.8f;
    m_default.spotPeripheralRate       = 0.55f;
    m_default.spotCloseFlankerRange    = 384.0f;
    m_default.spotCloseFlankerRate     = 0.35f;
    ClampVisionProfile(m_default);
}

static void ClampVisionProfile(BotProfile& profile)
{
    profile.visionDistanceMult     = Q_max(0.01f, profile.visionDistanceMult);
    profile.spotImmediateFov       = Q_clamp_float(profile.spotImmediateFov, 1.0f, 360.0f);
    profile.spotLikelyFov          = Q_clamp_float(profile.spotLikelyFov, profile.spotImmediateFov, 360.0f);
    profile.spotPeripheralFov      = Q_clamp_float(profile.spotPeripheralFov, profile.spotLikelyFov, 360.0f);
    profile.spotAwarenessThreshold = Q_max(0.01f, profile.spotAwarenessThreshold);
    profile.spotLikelyRate         = Q_max(0.0f, profile.spotLikelyRate);
    profile.spotPeripheralRate     = Q_max(0.0f, profile.spotPeripheralRate);
    profile.spotCloseFlankerRange  = Q_max(0.0f, profile.spotCloseFlankerRange);
    profile.spotCloseFlankerRate   = Q_max(0.0f, profile.spotCloseFlankerRate);
}

void BotProfileManager::ParseProfileFile(const char *filename, const char *basedir)
{
    char  *buf    = nullptr;
    long   len    = gi.FS_ReadFile(filename, (void **)&buf, qtrue);

    if (len <= 0 || !buf) {
        return;
    }

    BotProfile profile = m_default; // start from defaults so partial files work
    profile.name       = "";
    profile.weight     = 1.0f;

    const char *p = buf;
    char        line[256];

    while (*p) {
        // Read one line
        int i = 0;
        while (*p && *p != '\n' && *p != '\r' && i < (int)sizeof(line) - 1) {
            line[i++] = *p++;
        }
        line[i] = '\0';
        while (*p == '\n' || *p == '\r') {
            p++;
        }

        // Strip comments
        char *comment = strstr(line, "//");
        if (!comment) {
            comment = strstr(line, "#");
        }
        if (comment) {
            *comment = '\0';
        }

        // Parse key value
        char key[64] = {0};
        char val[192] = {0};
        if (sscanf(line, "%63s %191s", key, val) != 2) {
            continue;
        }

        float fval = (float)atof(val);

        if (!Q_stricmp(key, "name")) {
            profile.name = val;
        } else if (!Q_stricmp(key, "weight")) {
            profile.weight = fval;
        } else if (!Q_stricmp(key, "turnSpeed")) {
            profile.turnSpeed = fval;
        } else if (!Q_stricmp(key, "aimNoise")) {
            profile.aimNoise = fval;
        } else if (!Q_stricmp(key, "aimOvershoot")) {
            profile.aimOvershoot = fval;
        } else if (!Q_stricmp(key, "aimSettleSpeed")) {
            profile.aimSettleSpeed = fval;
        } else if (!Q_stricmp(key, "aimLerpSpeed")) {
            profile.aimLerpSpeed = fval;
        } else if (!Q_stricmp(key, "aimSpreadMult")) {
            profile.aimSpreadMult = fval;
        } else if (!Q_stricmp(key, "reactionMinDelay")) {
            profile.reactionMinDelay = fval;
        } else if (!Q_stricmp(key, "reactionRandomDelay")) {
            profile.reactionRandomDelay = fval;
        } else if (!Q_stricmp(key, "burstMinTime")) {
            profile.burstMinTime = fval;
        } else if (!Q_stricmp(key, "burstRandomDelay")) {
            profile.burstRandomDelay = fval;
        } else if (!Q_stricmp(key, "continuousFireMinTime")) {
            profile.continuousFireMinTime = fval;
        } else if (!Q_stricmp(key, "continuousFireRandomTime")) {
            profile.continuousFireRandomTime = fval;
        } else if (!Q_stricmp(key, "crouchChance")) {
            profile.crouchChance = fval;
        } else if (!Q_stricmp(key, "longRangeStrafeChance")) {
            profile.longRangeStrafeChance = Q_clamp_float(fval, 0.0f, 100.0f);
        } else if (!Q_stricmp(key, "preferredWeapon")) {
            profile.preferredWeapon = val;
        } else if (!Q_stricmp(key, "visionDistanceMult")) {
            profile.visionDistanceMult = fval;
        } else if (!Q_stricmp(key, "spotImmediateFov")) {
            profile.spotImmediateFov = fval;
        } else if (!Q_stricmp(key, "spotLikelyFov")) {
            profile.spotLikelyFov = fval;
        } else if (!Q_stricmp(key, "spotPeripheralFov")) {
            profile.spotPeripheralFov = fval;
        } else if (!Q_stricmp(key, "spotAwarenessThreshold")) {
            profile.spotAwarenessThreshold = fval;
        } else if (!Q_stricmp(key, "spotLikelyRate")) {
            profile.spotLikelyRate = fval;
        } else if (!Q_stricmp(key, "spotPeripheralRate")) {
            profile.spotPeripheralRate = fval;
        } else if (!Q_stricmp(key, "spotCloseFlankerRange")) {
            profile.spotCloseFlankerRange = fval;
        } else if (!Q_stricmp(key, "spotCloseFlankerRate")) {
            profile.spotCloseFlankerRate = fval;
        }
    }

    gi.FS_FreeFile(buf);
    ClampVisionProfile(profile);

    if (profile.name.length() == 0) {
        gi.Printf("^3WARNING: bot profile in '%s' has no name field, skipping\n", filename);
        return;
    }

    m_profiles.AddObject(profile);
}

void BotProfileManager::LoadProfiles(const char *directory)
{
    // Snapshot current cvar values into the default profile now that all
    // g_bot_* cvars have been registered by G_InitCvars().
    InitDefault();

    m_profiles.ClearObjectList();

    int        numFiles = 0;
    char     **files    = gi.FS_ListFiles(directory, "cfg", qfalse, &numFiles);

    if (!files || numFiles == 0) {
        gi.Printf("Bot profiles: no profiles found in '%s', using defaults\n", directory);
        if (files) {
            gi.FS_FreeFileList(files);
        }
        return;
    }

    for (int i = 0; i < numFiles; i++) {
        char filepath[MAX_QPATH];
        Com_sprintf(filepath, sizeof(filepath), "%s/%s", directory, files[i]);
        ParseProfileFile(filepath, directory);
    }

    gi.FS_FreeFileList(files);

    gi.Printf("Bot profiles: loaded %d profile(s) from '%s'\n", m_profiles.NumObjects(), directory);
}

const BotProfile *BotProfileManager::FindByName(const char *name) const
{
    if (!name || !name[0]) {
        return nullptr;
    }

    for (int i = 1; i <= m_profiles.NumObjects(); i++) {
        const BotProfile& p = m_profiles.ObjectAt(i);
        if (!Q_stricmp(p.name.c_str(), name)) {
            return &p;
        }
    }

    return nullptr;
}

const BotProfile& BotProfileManager::PickProfile(const char *overrideName) const
{
    if (m_profiles.NumObjects() == 0) {
        return m_default;
    }

    // Override: return named profile if it exists
    if (overrideName && overrideName[0]) {
        const BotProfile *found = FindByName(overrideName);
        if (found) {
            return *found;
        }
        gi.Printf("^3Bot profile override '%s' not found, using random selection\n", overrideName);
    }

    // Weighted random selection
    float totalWeight = 0.0f;
    for (int i = 1; i <= m_profiles.NumObjects(); i++) {
        totalWeight += m_profiles.ObjectAt(i).weight;
    }

    if (totalWeight <= 0.0f) {
        return m_profiles.ObjectAt(1);
    }

    float pick = G_Random(totalWeight);
    float accumulated = 0.0f;

    for (int i = 1; i <= m_profiles.NumObjects(); i++) {
        const BotProfile& p = m_profiles.ObjectAt(i);
        accumulated += p.weight;
        if (pick < accumulated) {
            return p;
        }
    }

    // Fallback (floating point edge case)
    return m_profiles.ObjectAt(m_profiles.NumObjects());
}

const BotProfile& BotProfileManager::GetDefault() const
{
    return m_default;
}

int BotProfileManager::NumProfiles() const
{
    return m_profiles.NumObjects();
}
