import QtQuick
import Quickshell
import Quickshell.Hyprland
import Quickshell.Wayland
import qs.Commons
import "TitleModel.js" as Titles

Item {
    id: root
    TitleSettings { id: preferences }
    readonly property int titleFontSize: preferences.number("fontSize", 14, 10, 24)
    readonly property int labelHeight: titleFontSize + 10
    readonly property int maxWidth: preferences.number("maxWidth", 960, 160, 1600)
    readonly property int offsetY: preferences.number("offsetY", 0, -80, 80)
    // Geometry changes do not all have IPC events. This queries Hyprland's
    // shared model directly, without spawning a polling shell command.
    Timer {
        interval: 200
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: Hyprland.refreshToplevels()
    }

    // Monitor IPC snapshots do not stay current when a workspace changes.
    // Use live properties below and refresh special-workspace state as well.
    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: Hyprland.refreshMonitors()
    }
    Connections {
        target: Hyprland
        function onRawEvent(event) {
            if (event && /^(workspace|activespecial|focusedmon|monitoradded|monitorremoved)/.test(event.name)) Hyprland.refreshMonitors();
        }
    }

    function monitorState(monitor) {
        if (!monitor) return null;
        var snapshot = monitor.lastIpcObject || ({});
        return {
            x: monitor.x, y: monitor.y,
            activeWorkspace: { id: monitor.activeWorkspace ? monitor.activeWorkspace.id : (snapshot.activeWorkspace || {}).id },
            specialWorkspace: snapshot.specialWorkspace || ({ id: 0 })
        };
    }

    readonly property var clients: {
        var result = [];
        var windows = Hyprland.toplevels.values;
        for (var i = 0; i < windows.length; ++i) result.push(windows[i].lastIpcObject || ({}));
        return result;
    }

    function screenFor(monitor) {
        if (!monitor) return null;
        var screens = Quickshell.screens;
        for (var i = 0; i < screens.length; ++i) if (screens[i].name === monitor.name) return screens[i];
        return null;
    }

    Variants {
        id: titleVariants
        model: Hyprland.toplevels.values
        delegate: Scope {
            id: entry
            required property var modelData
            readonly property var monitor: modelData.monitor
            readonly property var screen: root.screenFor(monitor)
            readonly property var client: modelData.lastIpcObject || ({})
            property string rememberedProvider: ""
            onClientChanged: {
                var detected = Titles.agentTitle(client);
                if (detected) rememberedProvider = detected.provider;
            }
            readonly property var label: Titles.labelFor(client, root.monitorState(monitor), root.clients, rememberedProvider)
            readonly property bool focused: modelData.activated
            readonly property string automaticName: label ? label.title : ""
            readonly property string caption: preferences.customName(client) || automaticName
            property bool editing: false
            property bool focusPrimed: false

            function beginRename() {
                editor.text = caption;
                focusPrimed = false;
                editing = true;
            }
            function finishRename(save) {
                if (!editing) return;
                if (save) preferences.rename(client, editor.text);
                editing = false;
                focusPrimed = false;
            }
            onLabelChanged: if (!label && editing) finishRename(true)

            Timer {
                id: primeFocus
                interval: 50
                onTriggered: {
                    if (!entry.editing) return;
                    editor.forceActiveFocus();
                    editor.selectAll();
                    entry.focusPrimed = true;
                }
            }

            HyprlandFocusGrab {
                active: entry.editing && editPanel.backingWindowVisible && entry.focusPrimed
                windows: [editPanel]
                onCleared: entry.finishRename(true)
            }

            TextMetrics {
                id: metrics
                font.family: Style.font.family
                font.pixelSize: root.titleFontSize
                font.bold: true
                text: entry.caption
            }

            PanelWindow {
                id: panel
                screen: entry.screen
                visible: entry.screen !== null && entry.label !== null && !entry.editing
                color: "transparent"
                anchors { top: true; left: true }
                implicitWidth: entry.label ? Math.max(100, Math.min(entry.label.width, metrics.advanceWidth + badge.width + 36, root.maxWidth)) : 100
                implicitHeight: root.labelHeight
                margins.left: entry.label ? Math.max(0, Math.round(entry.label.x + (entry.label.width - width) / 2)) : 0
                margins.top: entry.label ? Math.max(0, Math.round(entry.label.y + 24 - height + root.offsetY)) : 0
                exclusionMode: ExclusionMode.Ignore
                WlrLayershell.namespace: "omarchy-agent-titles"
                WlrLayershell.layer: WlrLayer.Top
                WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
                // Only this small tag is interactive; the rest of the desktop
                // and the terminal content keep their normal input behavior.

                Rectangle {
                    anchors.fill: parent
                    radius: 5
                    color: preferences.color("background", Color.background)
                    border.width: 1
                    border.color: entry.focused || entry.editing || clickArea.containsMouse ? preferences.color("accent", Color.accent) : Color.muted

                    Text {
                        id: badge
                        visible: !entry.editing
                        x: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: entry.label ? entry.label.provider.toUpperCase() : "AI"
                        textFormat: Text.PlainText
                        font.family: Style.font.family
                        font.pixelSize: 10
                        font.bold: true
                        color: entry.focused ? preferences.color("accent", Color.accent) : Color.muted
                    }
                    Text {
                        visible: !entry.editing
                        anchors.left: badge.right
                        anchors.leftMargin: 10
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: entry.caption
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        font: metrics.font
                        color: preferences.color("foreground", Color.foreground)
                    }
                    MouseArea {
                        id: clickArea
                        anchors.fill: parent
                        enabled: !entry.editing
                        hoverEnabled: true
                        cursorShape: Qt.IBeamCursor
                        onClicked: entry.beginRename()
                    }
                }
            }

            // Map a fresh keyboard-capable surface for editing. Changing a
            // mapped, non-focusable title layer in place loses focus on some
            // Hyprland versions and leaves a blank, uneditable rectangle.
            PanelWindow {
                id: editPanel
                screen: entry.screen
                visible: entry.editing && entry.screen !== null && entry.label !== null
                color: "transparent"
                anchors { top: true; left: true }
                implicitWidth: entry.label ? Math.min(entry.label.width, Math.max(380, metrics.advanceWidth + 30), root.maxWidth) : 380
                implicitHeight: root.labelHeight
                margins.left: entry.label ? Math.max(0, Math.round(entry.label.x + (entry.label.width - width) / 2)) : 0
                margins.top: panel.margins.top
                exclusionMode: ExclusionMode.Ignore
                WlrLayershell.namespace: "omarchy-agent-title-editor"
                WlrLayershell.layer: WlrLayer.Overlay
                WlrLayershell.keyboardFocus: entry.editing ? (entry.focusPrimed ? WlrKeyboardFocus.OnDemand : WlrKeyboardFocus.Exclusive) : WlrKeyboardFocus.None
                onBackingWindowVisibleChanged: if (backingWindowVisible && entry.editing) {
                    editor.forceActiveFocus();
                    primeFocus.restart();
                }
                Rectangle {
                    anchors.fill: parent
                    radius: 5
                    color: preferences.color("background", Color.background)
                    border.width: 1
                    border.color: preferences.color("accent", Color.accent)
                    TextInput {
                        id: editor
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        verticalAlignment: TextInput.AlignVCenter
                        font: metrics.font
                        color: preferences.color("foreground", Color.foreground)
                        selectionColor: Color.accent
                        selectedTextColor: Color.background
                        clip: true
                        maximumLength: 300
                        selectByMouse: true
                        focus: true
                        onAccepted: entry.finishRename(true)
                        Keys.onEscapePressed: entry.finishRename(false)
                        Keys.onTabPressed: entry.finishRename(true)
                    }
                }
            }
        }
    }
}
