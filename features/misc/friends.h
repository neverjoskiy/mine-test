#pragma once

#include <string>
#include <vector>

namespace Features::Friends {
    void EnsureLoaded();
    bool IsFriend(const std::string& name);
    bool Add(const std::string& name);
    bool Remove(const std::string& name);
    bool Toggle(const std::string& name);
    std::vector<std::string> List();
}
