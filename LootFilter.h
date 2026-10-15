#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>

enum LootPriority {
    PRIORITY_NONE = 0,
    PRIORITY_LOW = 1,       // Gold, common drops
    PRIORITY_MEDIUM = 2,    // Refined, gems
    PRIORITY_HIGH = 3,      // DragonBall, Meteor, ExpBall, CleanWater
    PRIORITY_CRITICAL = 4   // TanCPBag, GreenCPBag, all CP Bags
};

struct DetectedGroundItem {
    uintptr_t memoryAddress;
    uint32_t uid;
    uint32_t itemId;
    uint16_t posX;
    uint16_t posY;
    std::string name;
    LootPriority priority;
    double distance;
};

class LootFilter {
public:
    static LootPriority EvaluatePriority(const std::string& name, uint32_t itemId) {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // Explicitly reject bogus memory/code strings like "cpy", "memcpy", "strcpy"
        if (lower == "cpy" || lower.find("cpy") != std::string::npos) {
            return PRIORITY_NONE;
        }

        // 1. TanCPBag (Highest Priority #1)
        if (lower.find("tancp") != std::string::npos ||
            (lower.find("tan") != std::string::npos && lower.find("bag") != std::string::npos) ||
            itemId == 1006170 || itemId == 1006171) {
            return PRIORITY_CRITICAL;
        }

        // 2. GreenCPBag (High Priority #2)
        if (lower.find("greencp") != std::string::npos ||
            (lower.find("green") != std::string::npos && lower.find("bag") != std::string::npos) ||
            itemId == 1006180 || itemId == 1006181) {
            return PRIORITY_CRITICAL;
        }

        // General CP Bag fallback
        if (lower.find("cpbag") != std::string::npos ||
            (lower.find("cp") != std::string::npos && lower.find("bag") != std::string::npos)) {
            return PRIORITY_CRITICAL;
        }

        // 3. ExpBall / ExpBall(Event) (Priority #3)
        if (lower.find("expball") != std::string::npos ||
            lower.find("exp ball") != std::string::npos ||
            itemId == 1088000 || itemId == 1088001 || itemId == 722136) {
            return PRIORITY_HIGH;
        }

        // EXCLUSIVE RULE: All other items (Sycee, Gold, Meteor, Sand Essence, etc.) -> ZERO PRIORITY / IGNORED
        return PRIORITY_NONE;
    }

    static bool ShouldLoot(LootPriority priority, int minPriority = PRIORITY_LOW) {
        return (int)priority >= minPriority;
    }
};
