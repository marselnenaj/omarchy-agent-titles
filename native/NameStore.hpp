#pragma once

#include "TitleModel.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace AgentTitles {

class NameStore {
  public:
    nlohmann::json data = nlohmann::json::object();
    std::filesystem::path path;
    std::string session;

    NameStore(std::filesystem::path explicitPath = {}, std::string explicitSession = {}) {
        if (!explicitPath.empty()) { path = std::move(explicitPath); session = std::move(explicitSession); reload(); return; }
        const char* config = std::getenv("XDG_CONFIG_HOME");
        const char* home = std::getenv("HOME");
        const char* instance = std::getenv("HYPRLAND_INSTANCE_SIGNATURE");
        path = (config && *config ? std::filesystem::path(config) : std::filesystem::path(home ? home : ".") / ".config") / "omarchy/agent-titles.json";
        session = instance ? instance : "";
        reload();
    }
    void reload() {
        std::error_code error;
        if (!std::filesystem::exists(path, error) || std::filesystem::file_size(path, error) > 4 * 1024 * 1024) return;
        std::ifstream file(path);
        auto parsed = nlohmann::json::parse(file, nullptr, false);
        if (parsed.is_object()) data = std::move(parsed);
    }
    std::string get(const std::string& id) const {
        if (!data.contains("names") || !data["names"].is_object()) return {};
        const auto found = data["names"].find(session + ":" + id);
        return found != data["names"].end() && found->is_string() ? sanitize(found->get<std::string>()) : "";
    }
    bool set(const std::string& id, const std::string& title) {
        auto saved = data;
        reload();
        auto next = data;
        if (!next.contains("names") || !next["names"].is_object()) next["names"] = nlohmann::json::object();
        auto& names = next["names"];
        const auto value = sanitize(title);
        if (value.empty()) names.erase(session + ":" + id);
        else names[session + ":" + id] = value;
        for (auto it = names.begin(); it != names.end();) {
            if (!it.key().starts_with(session + ":")) it = names.erase(it);
            else ++it;
        }
        try {
            std::filesystem::create_directories(path.parent_path());
            auto temporary = path.string() + ".XXXXXX";
            const int fd = mkostemp(temporary.data(), O_CLOEXEC);
            if (fd < 0) return false;
            const auto contents = next.dump(2) + "\n";
            size_t offset = 0;
            while (offset < contents.size()) {
                const auto count = write(fd, contents.data() + offset, contents.size() - offset);
                if (count < 0 && errno == EINTR) continue;
                if (count <= 0) { close(fd); unlink(temporary.c_str()); return false; }
                offset += count;
            }
            const bool synced = fsync(fd) == 0;
            close(fd);
            if (!synced || rename(temporary.c_str(), path.c_str()) != 0) { unlink(temporary.c_str()); return false; }
            data = std::move(next);
            return true;
        } catch (...) { data = std::move(saved); return false; }
    }
};

}
