#include "friends.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>

namespace {
    std::mutex gMutex;
    std::set<std::string> gFriends;
    bool gLoaded = false;

    std::filesystem::path FriendsPath() {
        return std::filesystem::path("C:\\Nvidia\\Legit") / "friends.txt";
    }

    std::string Normalize(std::string name) {
        while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front()))) {
            name.erase(name.begin());
        }
        while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) {
            name.pop_back();
        }

        std::string out;
        out.reserve(name.size());
        for (unsigned char ch : name) {
            if (ch == '\r' || ch == '\n' || ch == '\t') {
                continue;
            }
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
        return out;
    }

    void SaveLocked() {
        std::error_code ignored;
        std::filesystem::create_directories(FriendsPath().parent_path(), ignored);

        std::ofstream file(FriendsPath(), std::ios::trunc);
        if (!file.is_open()) {
            return;
        }

        for (const std::string& name : gFriends) {
            file << name << '\n';
        }
    }
}

namespace Features::Friends {
    void EnsureLoaded() {
        std::lock_guard<std::mutex> lock(gMutex);
        if (gLoaded) {
            return;
        }

        std::ifstream file(FriendsPath());
        std::string line;
        while (std::getline(file, line)) {
            std::string normalized = Normalize(line);
            if (!normalized.empty()) {
                gFriends.insert(std::move(normalized));
            }
        }
        gLoaded = true;
    }

    bool IsFriend(const std::string& name) {
        EnsureLoaded();

        const std::string normalized = Normalize(name);
        if (normalized.empty()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(gMutex);
        return gFriends.find(normalized) != gFriends.end();
    }

    bool Add(const std::string& name) {
        EnsureLoaded();

        const std::string normalized = Normalize(name);
        if (normalized.empty()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(gMutex);
        const bool inserted = gFriends.insert(normalized).second;
        if (inserted) {
            SaveLocked();
        }
        return inserted;
    }

    bool Remove(const std::string& name) {
        EnsureLoaded();

        const std::string normalized = Normalize(name);
        std::lock_guard<std::mutex> lock(gMutex);
        const bool removed = gFriends.erase(normalized) > 0;
        if (removed) {
            SaveLocked();
        }
        return removed;
    }

    bool Toggle(const std::string& name) {
        EnsureLoaded();

        const std::string normalized = Normalize(name);
        if (normalized.empty()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(gMutex);
        if (gFriends.erase(normalized) > 0) {
            SaveLocked();
            return false;
        }

        gFriends.insert(normalized);
        SaveLocked();
        return true;
    }

    std::vector<std::string> List() {
        EnsureLoaded();

        std::lock_guard<std::mutex> lock(gMutex);
        return std::vector<std::string>(gFriends.begin(), gFriends.end());
    }
}
