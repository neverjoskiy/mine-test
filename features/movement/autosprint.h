#pragma once

#include <atomic>

namespace Features {
    namespace AutoSprint {
        inline std::atomic_bool Enabled{ false };
        void Tick();
    }
}
