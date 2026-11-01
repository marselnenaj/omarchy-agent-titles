const { readFileSync } = require('node:fs');
const { runInNewContext } = require('node:vm');
const { test } = require('node:test');
const assert = require('node:assert/strict');
const model = {};
runInNewContext(readFileSync(`${__dirname}/../TitleModel.js`, 'utf8'), model);
const client = {
    address: '0xexample', class: 'org.omarchy.agent', title: '⠋ Example task | Demo',
    monitor: 1, workspace: { id: 2 }, at: [20, 40], size: [800, 600],
    mapped: true, fullscreen: 0, floating: false, focusHistoryID: 1,
};

test('1000 unchanged/spinner frames publish no additional presentation updates', () => {
    const initial = model.reconcileClients([client], {});
    assert.equal(initial.changed, true);
    const spinner = ['⠋', '⠙', '⠹', '⠸'];
    for (let i = 0; i < 1000; ++i) {
        const next = model.reconcileClients([{ ...client, title: `${spinner[i % 4]} Example task | Demo`, focusHistoryID: i }], initial.byAddress);
        assert.equal(next.changed, false);
        assert.equal(next.clients[0], initial.clients[0]);
    }
    const idle = model.reconcileClients([{ ...client, title: 'Example task | Demo' }], initial.byAddress);
    assert.equal(idle.changed, false);
});

test('geometry, workspaces and real title changes invalidate presentation', () => {
    const initial = model.reconcileClients([client], {});
    for (const change of [{ at: [100, 50] }, { size: [900, 700] }, { workspace: { id: 3 } }, { title: '⠙ Another task | Demo' }, { fullscreen: 2 }]) {
        assert.equal(model.reconcileClients([{ ...client, ...change }], initial.byAddress).changed, true);
    }
    assert.equal(model.reconcileClients([], initial.byAddress).changed, true);
});

test('changing one window preserves other window objects', () => {
    const other = { ...client, address: '0xother' };
    const initial = model.reconcileClients([client, other], {});
    const next = model.reconcileClients([{ ...client, at: [30, 50] }, other], initial.byAddress);
    assert.equal(next.changed, true);
    assert.notEqual(next.clients[0], initial.clients[0]);
    assert.equal(next.clients[1], initial.clients[1]);
});

test('floating focus order remains available for occlusion decisions', () => {
    const floating = { ...client, floating: true };
    const initial = model.reconcileClients([floating], {});
    assert.equal(model.reconcileClients([{ ...floating, focusHistoryID: 2 }], initial.byAddress).changed, true);
});
