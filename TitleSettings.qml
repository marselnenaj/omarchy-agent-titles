import QtQuick
import Quickshell
import Quickshell.Io
import "TitleSettingsModel.js" as Settings

QtObject {
    id: root
    property var values: ({})
    readonly property string session: Quickshell.env("HYPRLAND_INSTANCE_SIGNATURE")
    readonly property string configPath: (Quickshell.env("XDG_CONFIG_HOME") || Quickshell.env("HOME") + "/.config") + "/omarchy/agent-titles.json"
    property FileView file: FileView {
        path: root.configPath
        watchChanges: true
        atomicWrites: true
        printErrors: false
        onFileChanged: reload()
        onLoaded: {
            try {
                var parsed = JSON.parse(text());
                if (parsed && typeof parsed === "object" && !Array.isArray(parsed)) root.values = parsed;
            } catch (error) { console.warn("agent-titles: invalid settings; keeping last valid values"); }
        }
    }
    function customName(client) {
        return Settings.customName(values, session, client);
    }
    function rename(client, title) {
        // Names belong to a window in this compositor session, never to a
        // different window that happens to reuse its old address.
        values = Settings.renamed(values, session, client, title);
        file.setText(JSON.stringify(values, null, 2) + "\n");
    }
    function number(name, fallback, min, max) {
        return Settings.number(values, name, fallback, min, max);
    }
    function color(name, fallback) {
        return Settings.color(values, name, fallback);
    }
}
