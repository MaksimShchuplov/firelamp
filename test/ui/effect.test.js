'use strict';
/**
 * Behavioural tests for the effect selector (Flames / Lava lamp / Aurora): sliders.js
 * sends /setfx, state.js pfx() renders the choice and hides the flame-only
 * sliders, and presets carry the effect.
 * Run with:  node --test test/ui/effect.test.js
 */
const { test, describe } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('fs');
const path = require('path');
const { boot, FILLED, startedOTA } = require('../ui_helpers.js');

const lavaOn = ui => ui.ctx.document.body.classList.contains('nofire');   // flame sliders hidden
const act = (ui, id) => ui.el(id).classList.contains('act');

async function importFile(ui, content) {
  const inp = ui.el('prfile');
  const text = JSON.stringify(content);
  inp.files = [{ name: 'firelamp-presets.json', text: () => Promise.resolve(text) }];
  inp.value = 'C:\\fakepath\\firelamp-presets.json';
  inp.dispatch('change');
  await ui.flush();
}

// ===========================================================================
describe('effect selector (sliders.js, state.js)', () => {
  test('the page starts on Flames with the flame sliders shown', async () => {
    const ui = await boot();
    assert.equal(act(ui, 'fx0'), true);
    assert.equal(act(ui, 'fx1'), false);
    assert.equal(lavaOn(ui), false);
  });

  test('tapping Lava lamp sends /setfx?v=1 with the CSRF header and applies the answer', async () => {
    const ui = await boot();
    ui.el('fx1').click();
    const call = ui.last('/setfx');
    assert.equal(call.url, '/setfx?v=1');
    assert.equal(call.opts.headers['X-Requested-With'], 'firelamp');
    await ui.respond(call, ui.state({ fx: 1 }));
    assert.equal(act(ui, 'fx1'), true);
    assert.equal(act(ui, 'fx0'), false);
    assert.equal(lavaOn(ui), true, 'flame-only sliders hidden');
  });

  test('tapping Flames again switches back and shows the flame sliders', async () => {
    const ui = await boot();
    ui.el('fx1').click();
    await ui.respond(ui.last('/setfx'), ui.state({ fx: 1 }));
    ui.el('fx0').click();
    assert.equal(ui.last('/setfx').url, '/setfx?v=0');
    await ui.respond(ui.last('/setfx'), ui.state({ fx: 0 }));
    assert.equal(act(ui, 'fx0'), true);
    assert.equal(lavaOn(ui), false);
  });

  test('a change made elsewhere (another phone, MQTT) arrives with the next poll', async () => {
    const ui = await boot();
    await ui.advance(5000);
    await ui.respond(ui.last('/state'), ui.state({ fx: 1 }));
    assert.equal(lavaOn(ui), true);
    await ui.advance(5000);
    await ui.respond(ui.last('/state'), ui.state({ fx: 0 }));
    assert.equal(lavaOn(ui), false);
  });

  test('tapping Aurora sends /setfx?v=2, highlights only Aurora and hides the flame sliders', async () => {
    const ui = await boot();
    ui.el('fx2').click();
    assert.equal(ui.last('/setfx').url, '/setfx?v=2');
    await ui.respond(ui.last('/setfx'), ui.state({ fx: 2 }));
    assert.deepEqual([0, 1, 2].map(i => act(ui, 'fx' + i)), [false, false, true]);
    assert.equal(lavaOn(ui), true);
  });

  test('switching from Lava lamp straight to Aurora keeps the flame sliders hidden', async () => {
    const ui = await boot();
    ui.ctx.pfx(1);
    ui.ctx.pfx(2);
    assert.equal(lavaOn(ui), true);
    assert.deepEqual([0, 1, 2].map(i => act(ui, 'fx' + i)), [false, false, true]);
  });

  test('an out-of-range effect from the lamp falls back to Flames', async () => {
    const ui = await boot();
    ui.ctx.pfx(7);
    assert.equal(act(ui, 'fx0'), true);
    assert.equal(lavaOn(ui), false);
  });

  test('a failed /setfx leaves the current effect and no unhandled rejection', async () => {
    const ui = await boot();
    ui.el('fx1').click();
    await ui.fail(ui.last('/setfx'));
    assert.equal(lavaOn(ui), false);
  });

  test('choosing an effect un-highlights the loaded preset', async () => {
    const ui = await boot({ presets: FILLED });
    ui.el('pr0').dispatch('click');
    await ui.flush();
    await ui.respond(ui.last('/loadpreset'), ui.state({ b: 40 }));
    assert.equal(act(ui, 'pr0'), true, 'precondition: preset 0 is active');
    ui.el('fx1').click();
    assert.equal(act(ui, 'pr0'), false);
  });

  test('loading a preset applies its effect', async () => {
    const ui = await boot({ presets: FILLED });
    ui.el('pr0').dispatch('click');
    await ui.flush();
    await ui.respond(ui.last('/loadpreset'), ui.state({ fx: 1 }));
    assert.equal(lavaOn(ui), true);
  });

  test('importing a preset file sends the effect along with the other params', async () => {
    const ui = await boot();
    await importFile(ui, [{ slot: 0, name: 'Lava', b: 60, th: 2, fx: 1 }]);
    assert.equal(ui.last('/savepreset').url, '/savepreset?slot=0&name=Lava&b=60&th=2&fx=1');
  });

  test('the effect buttons are locked while an update installs', async () => {
    const ui = await boot();
    await startedOTA(ui);
    assert.equal(ui.el('fx0').disabled, true);
    assert.equal(ui.el('fx1').disabled, true);
    assert.equal(ui.el('fx2').disabled, true);
  });

  test('labels are translated on a language switch', async () => {
    const ui = await boot();
    assert.equal(ui.el('fx1').textContent, 'Lava lamp');
    ui.el('lru').click();
    assert.equal(ui.el('lfx').textContent, 'Эффект');
    assert.equal(ui.el('fx0').textContent, 'Пламя');
    assert.equal(ui.el('fx1').textContent, 'Лава-лампа');
    assert.equal(ui.el('fx2').textContent, 'Сияние');
  });
});

// ===========================================================================
describe('flame-only sliders (index.html, themes.css)', () => {
  const html = fs.readFileSync(path.join(__dirname, '..', '..', 'ui', 'index.html'), 'utf8');
  const css = fs.readFileSync(path.join(__dirname, '..', '..', 'ui', 'css', 'themes.css'), 'utf8');
  const block = (html.match(/<div id=fireopts>([\s\S]*?)\n<\/div>/) || [])[1] || '';

  test('contrast, cooling, sparking and blend sit inside the block hidden in lava mode', () => {
    for (const id of ['sc', 'sco', 'ssp', 'sbl']) assert.match(block, new RegExp('id=' + id + '\\b'), id);
    assert.match(css, /body\.nofire #fireopts\{display:none\}/);
  });

  test('brightness, the effect selector and the themes stay outside it', () => {
    for (const id of ['sb', 'fx0', 'fx1', 'fx2', 'tb0']) assert.doesNotMatch(block, new RegExp('id=' + id + '\\b'), id);
  });
});
