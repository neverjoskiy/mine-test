#pragma once

#include <atomic>
#include <windows.h>

namespace Features::CombatControl {
    inline std::atomic<unsigned long long> SprintSuppressUntilMs{ 0 };

    inline void SuppressSprintFor(unsigned long long ms) {
        SprintSuppressUntilMs = GetTickCount64() + ms;
    }

    inline bool ShouldSuppressSprint() {
        return GetTickCount64() <= SprintSuppressUntilMs.load();
    }
}
