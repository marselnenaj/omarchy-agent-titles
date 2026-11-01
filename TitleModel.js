// Pure helpers, shared by the shell and the regression checks.
function agentTitle(client, rememberedProvider) {
    var app = String(client.class || "");
    var title = String(client.title || "").trim();
    if (!/ghostty|alacritty|kitty|foot|wezterm|org\.omarchy\.agent|org\.omarchy\.claude|org\.omarchy\.codex/i.test(app)) return null;
    var provider = "";
    if (/^[✳✻✽✶✢*]/.test(title) || /claude/i.test(app + " " + title)) provider = "Claude";
    else if (/^[\u2800-\u28ff◐◑◒◓]/.test(title) || /codex/i.test(app + " " + title)) provider = "Codex";
    // Codex drops its spinner while idle, but retains "task | directory".
    else if (/\S\s+\|\s+\S/.test(title)) provider = "Codex";
    else if (/org\.omarchy\.agent/i.test(app)) provider = rememberedProvider || "AI";
    // Keep an already recognized window labelled between activity frames.
    // A shell/path title means the agent has exited, so drop that hint.
    else if (rememberedProvider && title && !/^(?:[~\/]|(?:bash|zsh|fish|sh|ghostty|terminal)$|\S+@\S+:)/i.test(title)) provider = rememberedProvider;
    if (!provider) return null;
    title = title.replace(/^[\u2800-\u28ff✳✻✽✶✢◐◑◒◓*\s]+/, "").trim();
    return { provider: provider, title: title || provider };
}

// Strip transient IPC metadata before publishing state to the UI. Stable
// object identities prevent every IPC response from re-running each label's
// layout, occlusion checks and text measurement.
function presentationClient(client, previous) {
    var detected = agentTitle(client, previous ? previous.agentProvider : "");
    return {
        address: client.address,
        stableId: client.stableId,
        class: client.class,
        title: detected ? detected.title : String(client.title || ""),
        agentProvider: detected ? detected.provider : "",
        monitor: client.monitor,
        workspace: { id: (client.workspace || {}).id },
        at: client.at,
        size: client.size,
        mapped: client.mapped,
        hidden: client.hidden,
        fullscreen: client.fullscreen,
        pinned: client.pinned,
        floating: client.floating,
        // Focus ordering matters only for floating-window occlusion.
        focusHistoryID: client.floating ? client.focusHistoryID : undefined
    };
}

function reconcileClients(observed, previous) {
    var byAddress = {};
    var clients = [];
    var changed = observed.length !== Object.keys(previous).length;
    for (var i = 0; i < observed.length; ++i) {
        var raw = observed[i];
        var old = previous[raw.address];
        var next = presentationClient(raw, old);
        if (old && samePresentation(old, next)) next = old;
        else changed = true;
        byAddress[raw.address] = next;
        clients.push(next);
    }
    return { changed: changed, byAddress: byAddress, clients: clients };
}

function samePresentation(a, b) {
    return a.address === b.address && a.stableId === b.stableId &&
        a.class === b.class && a.title === b.title && a.agentProvider === b.agentProvider &&
        a.monitor === b.monitor && a.workspace.id === b.workspace.id &&
        (a.at || [])[0] === (b.at || [])[0] && (a.at || [])[1] === (b.at || [])[1] &&
        (a.size || [])[0] === (b.size || [])[0] && (a.size || [])[1] === (b.size || [])[1] &&
        a.mapped === b.mapped && a.hidden === b.hidden && a.fullscreen === b.fullscreen &&
        a.pinned === b.pinned && a.floating === b.floating && a.focusHistoryID === b.focusHistoryID;
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

function labelFor(client, monitor, clients, rememberedProvider) {
    var title = agentTitle(client, rememberedProvider);
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
