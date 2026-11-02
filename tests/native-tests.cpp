#include "../native/TitleModel.hpp"
#include "../native/NameStore.hpp"
#include <iostream>

#define CHECK(condition) do { if (!(condition)) { std::cerr << "Check failed at line " << __LINE__ << '\n'; return 1; } } while (false)

int main() {
    using namespace AgentTitles;
    CHECK(detect("com.mitchellh.ghostty", "✳ Example project")->provider == "CLAUDE");
    CHECK(detect("com.mitchellh.ghostty", "⠋ Example task | Demo")->text == "Example task | Demo");
    CHECK(detect("com.mitchellh.ghostty", "Example task | Demo")->provider == "CODEX");
    CHECK(detect("org.omarchy.agent", "Example task", "CODEX")->provider == "CODEX");
    CHECK(!detect("chromium", "✳ Example project"));
    CHECK(!detect("com.mitchellh.ghostty", "~/Example", "CODEX"));
    CHECK(!detect("com.mitchellh.ghostty", "bash", "CODEX"));
    for (const auto marker : {"⠋", "⠙", "⠹", "⠸"})
        CHECK(detect("com.mitchellh.ghostty", std::string(marker) + " Example task | Demo") == detect("com.mitchellh.ghostty", "Example task | Demo"));
    Editor editor;
    editor.begin("Existing name");
    CHECK(editor.selected());
    editor.insert("Äöü – Demo");
    CHECK(editor.text == "Äöü – Demo");
    editor.backspace(); CHECK(editor.text == "Äöü – Dem");
    editor.move(0); editor.deleteForward(); CHECK(editor.text == "öü – Dem");
    editor.move(next(editor.text, 0)); editor.backspace(); CHECK(editor.text == "ü – Dem");
    editor.anchor = 0; editor.cursor = editor.text.size(); editor.backspace(); CHECK(editor.text.empty());
    editor.begin("Example"); editor.left(false); CHECK(editor.cursor == 0 && !editor.selected());
    editor.begin("Example"); editor.right(false); CHECK(editor.cursor == editor.text.size() && !editor.selected());
    editor.begin("Example"); editor.insert(std::string(301, 'x')); CHECK(editor.text == "Example");
    CHECK(sanitize("\t Example\nproject \x1b ") == "Example project");
    CHECK(sanitize(std::string(400, 'x')).size() == 300);

    const auto directory = std::filesystem::temp_directory_path() / ("agent-titles-test-" + std::to_string(getpid()));
    std::filesystem::remove_all(directory);
    NameStore names(directory / "settings.json", "example-session");
    names.data["fontSize"] = 18;
    CHECK(names.set("window-a", "Project A"));
    CHECK(names.get("window-a") == "Project A");
    CHECK(names.set("window-b", "Project B"));
    CHECK(names.get("window-a") == "Project A");
    NameStore loaded(directory / "settings.json", "example-session");
    CHECK(loaded.get("window-b") == "Project B");
    CHECK(loaded.data["fontSize"] == 18);
    struct stat status{}; CHECK(stat(loaded.path.c_str(), &status) == 0); CHECK((status.st_mode & 0777) == 0600);
    CHECK(loaded.set("window-a", "")); CHECK(loaded.get("window-a").empty());
    NameStore otherSession(directory / "settings.json", "another-session");
    CHECK(otherSession.get("window-b").empty());
    CHECK(otherSession.set("window-c", "New project"));
    CHECK(otherSession.data["names"].size() == 1);
    std::filesystem::remove_all(directory);
    std::cout << "Native title, spinner, UTF-8 editing and private settings checks passed.\n";
}
