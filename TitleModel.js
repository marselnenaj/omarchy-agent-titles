// Pure helpers, shared by the shell and the regression checks.
function agentTitle(client) {
    var app = String(client.class || "");
    var title = String(client.title || "").trim();
    if (!/ghostty|alacritty|kitty|foot|wezterm|org\.omarchy\.agent|org\.omarchy\.claude|org\.omarchy\.codex/i.test(app)) return null;
    var provider = "";
    if (/^[✳✻✽✶✢*]/.test(title) || /claude/i.test(app + " " + title)) provider = "Claude";
    else if (/^[\u2800-\u28ff◐◑◒◓]/.test(title) || /codex/i.test(app + " " + title)) provider = "Codex";
    else if (/org\.omarchy\.agent/i.test(app)) provider = "AI";
    if (!provider) return null;
    title = title.replace(/^[\u2800-\u28ff✳✻✽✶✢◐◑◒◓*\s]+/, "").trim();
    return { provider: provider, title: title || provider };
}

function workspaceVisible(client, monitor) {
    var workspace = Number((client.workspace || {}).id);
    var special = Number((monitor.specialWorkspace || {}).id || 0);
    if (client.pinned && workspace >= 0) return true;
    return workspace === (special || Number((monitor.activeWorkspace || {}).id));
}

function overlaps(a, b) {
    return a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y;
}

function labelFor(client, monitor, clients) {
    var title = agentTitle(client);
    if (!title || !monitor || client.mapped === false || client.hidden || !client.at || !client.size || !workspaceVisible(client, monitor)) return null;
    // Real fullscreen (2) hides every label on this monitor. Maximized (1)
    // still has terminal padding and can safely carry its own title.
    for (var i = 0; i < clients.length; ++i) {
        var other = clients[i];
        if (other.monitor === client.monitor && other.mapped !== false && !other.hidden && workspaceVisible(other, monitor) && Number(other.fullscreen) === 2) return null;
    }
    var rect = { x: client.at[0] + 12, y: client.at[1] - 10, width: client.size[0] - 24, height: 24 };
    if (rect.width < 100) return null;
    // Layer-shell is above application windows. Suppress labels covered by
    // a floating window so they never bleed through a foreground dialog.
    for (var j = 0; j < clients.length; ++j) {
        var cover = clients[j];
        if (cover.address === client.address || cover.monitor !== client.monitor || cover.hidden || cover.mapped === false || !cover.at || !cover.size || !workspaceVisible(cover, monitor)) continue;
        var above = cover.floating && (!client.floating || cover.focusHistoryID < client.focusHistoryID);
        if (above && overlaps(rect, { x: cover.at[0], y: cover.at[1], width: cover.size[0], height: cover.size[1] })) return null;
    }
    return { provider: title.provider, title: title.title, x: rect.x - monitor.x, y: Math.max(0, rect.y - monitor.y), width: rect.width };
}
