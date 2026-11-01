const { readFileSync } = require('node:fs');
const { runInNewContext } = require('node:vm');
const { test } = require('node:test');
const assert = require('node:assert/strict');
const settings = {};
runInNewContext(readFileSync(`${__dirname}/../TitleSettingsModel.js`, 'utf8'), settings);
const client = { address: '0xexample', stableId: 'example-window' };

test('renaming preserves appearance and other windows without mutating the input', () => {
    const original = { fontSize: 18, names: { 'session:other': 'Other project' } };
    const renamed = settings.renamed(original, 'session', client, '  Example project  ');
    assert.equal(settings.customName(renamed, 'session', client), 'Example project');
    assert.equal(renamed.fontSize, 18);
    assert.equal(renamed.names['session:other'], 'Other project');
    assert.equal(Object.keys(original.names).length, 1);
});

test('empty names restore automatic titles and stale sessions are discarded', () => {
    const original = { names: { 'old-session:example-window': 'Old project', 'session:example-window': 'Custom' } };
    const renamed = settings.renamed(original, 'session', client, '   ');
    assert.equal(Object.keys(renamed.names).length, 0);
    assert.equal(settings.customName(renamed, 'session', client), '');
    assert.equal(settings.customName(original, 'new-session', client), '');
});

test('names reject control characters, have a size limit and retain Unicode', () => {
    const result = settings.renamed({}, 'session', client, 'Demo\n\x1bProjekt – ÄÖÜ');
    assert.equal(settings.customName(result, 'session', client), 'Demo  Projekt – ÄÖÜ');
    const long = settings.renamed({}, 'session', client, 'x'.repeat(400));
    assert.equal(settings.customName(long, 'session', client).length, 300);
});

test('malformed settings use safe defaults', () => {
    assert.equal(settings.customName({ names: [] }, 'session', client), '');
    assert.equal(settings.customName({ names: { 'session:example-window': {} } }, 'session', client), '');
    assert.equal(settings.customName(settings.renamed({ names: 'invalid' }, 'session', client, 'Valid'), 'session', client), 'Valid');
    assert.equal(settings.number({ fontSize: 'invalid' }, 'fontSize', 14, 10, 24), 14);
    assert.equal(settings.number({ fontSize: 9999 }, 'fontSize', 14, 10, 24), 24);
    assert.equal(settings.number({ fontSize: -9999 }, 'fontSize', 14, 10, 24), 10);
    assert.equal(settings.number({}, 'fontSize', 14, 10, 24), 14);
    assert.equal(settings.color({ foreground: 'url(https://example.invalid)' }, 'foreground', '#eeeeee'), '#eeeeee');
    assert.equal(settings.color({ foreground: '#abcdef' }, 'foreground', '#eeeeee'), '#abcdef');
});
