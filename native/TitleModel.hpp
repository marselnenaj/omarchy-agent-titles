#pragma once

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace AgentTitles {

inline std::string trim(std::string value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    return value.substr(begin, value.find_last_not_of(" \t\r\n") - begin + 1);
}

inline std::string lower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) { return std::tolower(c); });
    return value;
}

inline bool terminal(std::string_view app) {
    for (const auto match : {"ghostty", "alacritty", "kitty", "foot", "wezterm", "org.omarchy.agent", "org.omarchy.claude", "org.omarchy.codex"})
        if (app.find(match) != std::string_view::npos) return true;
    return false;
}

struct Title {
    std::string provider;
    std::string text;
    bool operator==(const Title&) const = default;
};

inline std::optional<Title> detect(std::string app, std::string title, std::string remembered = {}) {
    app = lower(app);
    title = trim(title);
    if (!terminal(app)) return std::nullopt;
    const std::string combined = app + " " + lower(title);
    std::string provider;
    const auto claude = [](const std::string& t) {
        for (const auto marker : {"✳", "✻", "✽", "✶", "✢", "*"}) if (t.starts_with(marker)) return true;
        return false;
    };
    const auto codex = [](const std::string& t) {
        if (t.size() >= 3 && static_cast<unsigned char>(t[0]) == 0xe2 &&
            static_cast<unsigned char>(t[1]) >= 0xa0 && static_cast<unsigned char>(t[1]) <= 0xa3) return true;
        for (const auto marker : {"◐", "◑", "◒", "◓"}) if (t.starts_with(marker)) return true;
        return false;
    };
    if (claude(title) || combined.find("claude") != std::string::npos) provider = "CLAUDE";
    else if (codex(title) || combined.find("codex") != std::string::npos || title.find(" | ") != std::string::npos) provider = "CODEX";
    else if (app.find("org.omarchy.agent") != std::string::npos) provider = remembered.empty() ? "AI" : remembered;
    else if (!remembered.empty() && !title.empty() && !title.starts_with("/") && !title.starts_with("~") &&
             title.find('@') == std::string::npos && lower(title) != "bash" && lower(title) != "zsh" && lower(title) != "fish") provider = remembered;
    if (provider.empty()) return std::nullopt;
    while (!title.empty()) {
        if (claude(title) || codex(title)) title.erase(0, title[0] == '*' ? 1 : 3);
        else if (std::isspace(static_cast<unsigned char>(title[0]))) title.erase(0, 1);
        else break;
    }
    return Title{provider, title.empty() ? provider : title};
}

// UTF-8 byte offsets; never split a multi-byte codepoint while editing.
inline size_t previous(const std::string& text, size_t pos) {
    if (!pos) return 0;
    --pos;
    while (pos && (static_cast<unsigned char>(text[pos]) & 0xc0) == 0x80) --pos;
    return pos;
}
inline size_t next(const std::string& text, size_t pos) {
    if (pos >= text.size()) return text.size();
    ++pos;
    while (pos < text.size() && (static_cast<unsigned char>(text[pos]) & 0xc0) == 0x80) ++pos;
    return pos;
}

inline std::string sanitize(std::string text) {
    for (char& c : text) if (static_cast<unsigned char>(c) < 32 || c == 127) c = ' ';
    text = trim(text);
    size_t end = 0;
    for (size_t count = 0; count < 300 && end < text.size(); ++count) end = next(text, end);
    text.resize(end);
    return text;
}

struct Editor {
    std::string text;
    size_t cursor = 0;
    size_t anchor = 0;
    bool selected() const { return cursor != anchor; }
    void begin(std::string value) { text = std::move(value); cursor = text.size(); anchor = 0; }
    void eraseSelection() {
        const auto begin = std::min(cursor, anchor);
        text.erase(begin, std::max(cursor, anchor) - begin);
        cursor = anchor = begin;
    }
    void insert(std::string_view value) {
        const auto begin = std::min(cursor, anchor);
        auto candidate = text;
        candidate.replace(begin, std::max(cursor, anchor) - begin, value);
        size_t pos = 0, count = 0;
        while (pos < candidate.size() && count <= 300) { pos = next(candidate, pos); ++count; }
        if (count > 300 || candidate.size() > 4096) return;
        text = std::move(candidate);
        cursor = anchor = begin + value.size();
    }
    void move(size_t pos, bool selecting = false) { cursor = pos; if (!selecting) anchor = pos; }
    void left(bool selecting) { move(selected() && !selecting ? std::min(cursor, anchor) : previous(text, cursor), selecting); }
    void right(bool selecting) { move(selected() && !selecting ? std::max(cursor, anchor) : next(text, cursor), selecting); }
    void backspace() {
        if (selected()) { eraseSelection(); return; }
        anchor = previous(text, cursor); eraseSelection();
    }
    void deleteForward() {
        if (selected()) { eraseSelection(); return; }
        anchor = next(text, cursor); eraseSelection();
    }
};

}
