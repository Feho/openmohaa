// playerbot_profile.cpp: Per-bot personality profile loading and management.

#include "g_local.h"
#include "playerbot_profile.h"
#include "gamecvars.h"
#include "g_bot.h"

BotProfileManager botProfileManager;

// Added in OPM
//  Populate profile from current cvar defaults so the game behaves
//  identically when no profile files are present.
void BotProfile::setDefaults()
{
    name = "default";

    reactionMinDelay    = g_bot_attack_react_min_delay->value;
    reactionRandomDelay = g_bot_attack_react_random_delay->value;
    aimSpreadMult       = g_bot_attack_spreadmult->value;
    aimSettleSpeed      = g_bot_aim_settle_speed->value;
    aimOvershoot        = g_bot_aim_overshoot->value;
    aimNoise            = g_bot_aim_noise->value;
    aimLerpSpeed        = g_bot_aim_lerp_speed->value;

    preferredRangeMin = 128;
    preferredRangeMax = 800;
    riskTolerance     = 0.5f;
    aggression        = 0.5f;

    burstMinTime             = g_bot_attack_burst_min_time->value;
    burstRandomDelay         = g_bot_attack_burst_random_delay->value;
    continuousFireMinTime    = g_bot_attack_continuousfire_min_firetime->value;
    continuousFireRandomTime = g_bot_attack_continuousfire_random_firetime->value;

    turnSpeed    = g_bot_turn_speed->value;
    roamRadius   = 2048;
    walkChance   = 0.25f;
    pauseChance  = 0.0025f;
    crouchChance = g_bot_crouch_chance->integer;

    hearingRange     = 1.0f;
    visionFovDegrees = 120;

    preferredWeapon = "";

    weight = 1.0f;
}

BotProfileManager::BotProfileManager() {}

// Added in OPM
//  Initialize the profile manager: load all profiles from
//  main/bots/profiles/*.cfg, then build the default fallback.
void BotProfileManager::Init()
{
    m_profiles.FreeObjectList();
    m_defaultProfile.setDefaults();

    int    numFiles = 0;
    char **fileList = gi.FS_ListFiles("bots/profiles", ".cfg", qfalse, &numFiles);

    if (fileList) {
        for (int i = 0; i < numFiles; i++) {
            str fullPath = str("bots/profiles/") + fileList[i];
            LoadProfileFile(fullPath.c_str());
        }
        gi.FS_FreeFileList(fileList);
    }

    gi.Printf("BotProfileManager: loaded %d profile(s)\n", m_profiles.NumObjects());
}

// Added in OPM
//  Parse a flat key-value profile file. Each line is "key value".
//  Lines starting with '//' or '#' are comments. Blank lines are skipped.
void BotProfileManager::LoadProfileFile(const char *path)
{
    char *buffer = NULL;
    int   len    = gi.FS_ReadFile(path, (void **)&buffer, qtrue);

    if (len <= 0 || !buffer) {
        return;
    }

    BotProfile profile;
    profile.setDefaults();

    char *p = buffer;

    while (p && *p) {
        const char *token = COM_Parse(&p);
        if (!token || !*token) {
            break;
        }

        // Skip comment lines
        if (token[0] == '/' && token[1] == '/') {
            Com_SkipRestOfLine(&p);
            continue;
        }
        if (token[0] == '#') {
            Com_SkipRestOfLine(&p);
            continue;
        }

        str key = token;

        token = COM_Parse(&p);
        if (!token || !*token) {
            break;
        }

        str value = token;

        if (key == "name") {
            profile.name = value;
        } else if (key == "reactionMinDelay") {
            profile.reactionMinDelay = atof(value.c_str());
        } else if (key == "reactionRandomDelay") {
            profile.reactionRandomDelay = atof(value.c_str());
        } else if (key == "aimSpreadMult") {
            profile.aimSpreadMult = atof(value.c_str());
        } else if (key == "aimSettleSpeed") {
            profile.aimSettleSpeed = atof(value.c_str());
        } else if (key == "aimOvershoot") {
            profile.aimOvershoot = atof(value.c_str());
        } else if (key == "aimNoise") {
            profile.aimNoise = atof(value.c_str());
        } else if (key == "aimLerpSpeed") {
            profile.aimLerpSpeed = atof(value.c_str());
        } else if (key == "preferredRangeMin") {
            profile.preferredRangeMin = atof(value.c_str());
        } else if (key == "preferredRangeMax") {
            profile.preferredRangeMax = atof(value.c_str());
        } else if (key == "riskTolerance") {
            profile.riskTolerance = atof(value.c_str());
        } else if (key == "aggression") {
            profile.aggression = atof(value.c_str());
        } else if (key == "burstMinTime") {
            profile.burstMinTime = atof(value.c_str());
        } else if (key == "burstRandomDelay") {
            profile.burstRandomDelay = atof(value.c_str());
        } else if (key == "continuousFireMinTime") {
            profile.continuousFireMinTime = atof(value.c_str());
        } else if (key == "continuousFireRandomTime") {
            profile.continuousFireRandomTime = atof(value.c_str());
        } else if (key == "turnSpeed") {
            profile.turnSpeed = atof(value.c_str());
        } else if (key == "roamRadius") {
            profile.roamRadius = atof(value.c_str());
        } else if (key == "walkChance") {
            profile.walkChance = atof(value.c_str());
        } else if (key == "pauseChance") {
            profile.pauseChance = atof(value.c_str());
        } else if (key == "crouchChance") {
            profile.crouchChance = atoi(value.c_str());
        } else if (key == "hearingRange") {
            profile.hearingRange = atof(value.c_str());
        } else if (key == "visionFovDegrees") {
            profile.visionFovDegrees = atof(value.c_str());
        } else if (key == "preferredWeapon") {
            profile.preferredWeapon = value;
        } else if (key == "weight") {
            profile.weight = atof(value.c_str());
        } else {
            gi.DPrintf("BotProfile: unknown key '%s' in %s\n", key.c_str(), path);
        }
    }

    gi.FS_FreeFile(buffer);

    gi.Printf("BotProfileManager: loaded profile '%s' from %s\n", profile.name.c_str(), path);
    m_profiles.AddObject(profile);
}

// Added in OPM
//  Pick a profile using weighted random selection.
//  If g_bot_profile_override is set, return that specific profile.
//  Falls back to the default profile if nothing matches.
const BotProfile& BotProfileManager::PickProfile() const
{
    if (g_bot_profile_override->string[0]) {
        return FindProfile(g_bot_profile_override->string);
    }

    int numProfiles = m_profiles.NumObjects();
    if (numProfiles == 0) {
        return m_defaultProfile;
    }

    // Calculate total weight
    float totalWeight = 0;
    for (int i = 1; i <= numProfiles; i++) {
        totalWeight += m_profiles.ObjectAt(i).weight;
    }

    if (totalWeight <= 0) {
        return m_profiles.ObjectAt(1);
    }

    // Weighted random pick
    float roll = G_Random(totalWeight);
    float cumulative = 0;

    for (int i = 1; i <= numProfiles; i++) {
        cumulative += m_profiles.ObjectAt(i).weight;
        if (roll < cumulative) {
            return m_profiles.ObjectAt(i);
        }
    }

    return m_profiles.ObjectAt(numProfiles);
}

// Added in OPM
//  Find a profile by name. Returns default if not found.
const BotProfile& BotProfileManager::FindProfile(const char *name) const
{
    for (int i = 1; i <= m_profiles.NumObjects(); i++) {
        if (!Q_stricmp(m_profiles.ObjectAt(i).name.c_str(), name)) {
            return m_profiles.ObjectAt(i);
        }
    }

    gi.DPrintf("BotProfileManager: profile '%s' not found, using default\n", name);
    return m_defaultProfile;
}

const BotProfile& BotProfileManager::GetDefaultProfile() const
{
    return m_defaultProfile;
}

int BotProfileManager::NumProfiles() const
{
    return m_profiles.NumObjects();
}
