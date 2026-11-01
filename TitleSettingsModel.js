function record(value) {
    return value && typeof value === "object" && !Array.isArray(value) ? value : {};
}

function windowKey(session, client) {
    return session + ":" + String(client.stableId || client.address);
}

function customName(values, session, client) {
    var value = record(record(values).names)[windowKey(session, client)];
    return typeof value === "string" ? value : "";
}

function renamed(values, session, client, title) {
    var next = JSON.parse(JSON.stringify(record(values)));
    var names = record(next.names);
    var value = String(title || "").replace(/[\x00-\x1f\x7f]/g, " ").trim().slice(0, 300);
    if (value) names[windowKey(session, client)] = value;
    else delete names[windowKey(session, client)];
    for (var id in names) if (id.indexOf(session + ":") !== 0) delete names[id];
    next.names = names;
    return next;
}

function number(values, name, fallback, min, max) {
    var value = record(values)[name];
    var n = Number(value === undefined ? fallback : value);
    return isFinite(n) ? Math.max(min, Math.min(max, n)) : fallback;
}

function color(values, name, fallback) {
    var value = String(record(values)[name] || "");
    return /^#[0-9a-f]{6}([0-9a-f]{2})?$/i.test(value) ? value : fallback;
}
