'use strict';
/**
 * Behavioural tests for ui/js/sliders.js and ui/js/globals.js: what the user
 * sees while dragging sliders / tapping theme and Reset, and what reaches the lamp.
 * Run with:  node --test test/ui/sliders-globals.test.js
 */
const { test, describe } = require('node:test');
const assert = require('node:assert/strict');
const { FILLED, boot, val, moveSlider } = require('../ui_helpers.js');

/** Load preset slot 0 so it is highlighted as the active preset. */
async function withActivePreset() {
  const ui = await boot({ presets: FILLED });
  ui.el('pr0').dispatch('click');
  await ui.flush();
  await ui.respond(ui.last('/loadpreset'), ui.state({ b: 40 }));
  assert.ok(ui.el('pr0').classList.contains('act'), 'precondition: preset 0 is active');
  return ui;
}

/** Run a Surprise Me so an AI effect name is on screen. */
async function withAiName(ui) {
  ui.el('surprise').click();
  await ui.respond(ui.last('/surprise'), Object.assign(ui.state(), { name: 'Aurora' }));
  assert.match(ui.el('ainame').textContent, /^Aurora/, 'precondition: AI name shown');
}

/** Collect unhandled promise rejections raised while `fn` runs. */
async function unhandledDuring(fn) {
  const seen = [];
  const h = e => seen.push(e);
  process.on('unhandledRejection', h);
  try { await fn(); await new Promise(r => setImmediate(r)); }
  finally { process.off('unhandledRejection', h); }
  return seen;
}

// ===========================================================================
describe('slider debounce', () => {
  test('dragging Sparking sends one /setsp with the final value, not one per input event', async () => {
    const ui = await boot();
    for (const v of [60, 120, 180, 200]) { await moveSlider(ui, 'ssp', v); await ui.advance(50); }
    await ui.advance(200);
    assert.deepEqual(ui.callsTo('/setsp').map(c => c.url), ['/setsp?v=200']);
  });

  test('the request goes out 120 ms after the last input, not before', async () => {
    const ui = await boot();
    await moveSlider(ui, 'ssp', 90);
    await ui.advance(110);
    assert.equal(ui.callsTo('/setsp').length, 0, 'still debouncing');
    await ui.advance(20);
    assert.equal(ui.callsTo('/setsp').length, 1);
  });
});

// ===========================================================================
describe('a poll in flight never snaps a slider back', () => {
  for (const [id, key, moved, stale] of [['sco', 'co', 99, 46], ['sbl', 'bl', 200, 50], ['ssp', 'sp', 180, 26], ['sc', 'c', 90, 50]]) {
    test(`${id}: a /state response sent before the drag does not overwrite the new value`, async () => {
      const ui = await boot();
      await ui.advance(5000);
      const p = ui.last('/state');
      await moveSlider(ui, id, moved);
      await ui.respond(p, ui.state({ [key]: stale }));
      assert.equal(val(ui, id), String(moved));
    });

    test(`${id}: a poll fired within 1 s of the last input is discarded`, async () => {
      const ui = await boot();
      await moveSlider(ui, id, moved);
      await ui.advance(200);
      await ui.setHidden(true); await ui.setHidden(false);   // tab refocus fires a poll
      await ui.respond(ui.last('/state'), ui.state({ [key]: stale }));
      assert.equal(val(ui, id), String(moved));
    });
  }
});

// ===========================================================================
describe('live labels while dragging', () => {
  test('Sparking updates its number and description as it moves', async () => {
    const ui = await boot();
    await moveSlider(ui, 'ssp', 200);
    assert.equal(String(ui.el('vsp').textContent), '200');
    assert.equal(ui.el('dsp').textContent, 'Hot inferno');
  });

  test('Contrast updates its own label, and leaves Brightness alone', async () => {
    const ui = await boot();
    await moveSlider(ui, 'sc', 0);
    assert.equal(String(ui.el('vc').textContent), '0');
    assert.equal(ui.el('dc').textContent, 'Yellows and whites');
    assert.equal(val(ui, 'sb'), '100', 'brightness slider untouched');
    assert.equal(String(ui.el('vb').textContent), '100');
    assert.equal(ui.el('body').classList.contains('off'), false, 'contrast 0 must not grey the page out');
  });

  test('every slider label tracks its own slider', async () => {
    const ui = await boot();
    const cases = [['sb', 'vb', 'db', 60, 'Medium'], ['sc', 'vc', 'dc', 70, 'Saturated oranges'],
      ['sco', 'vco', 'dco', 120, 'Short sparks'], ['ssp', 'vsp', 'dsp', 100, 'Active fire'],
      ['sbl', 'vbl', 'dbl', 10, 'Frozen glow']];
    for (const [sid, vid, did, v, desc] of cases) {
      await moveSlider(ui, sid, v);
      assert.equal(String(ui.el(vid).textContent), String(v), vid);
      assert.equal(ui.el(did).textContent, desc, did);
    }
  });
});

// ===========================================================================
describe('haptic tick at the slider end stops', () => {
  test('Brightness vibrates at 0 and at 100, not in between', async () => {
    const ui = await boot();
    await moveSlider(ui, 'sb', 50);
    assert.deepEqual(ui.vibrations, [], 'no tick mid-range');
    await moveSlider(ui, 'sb', 0);
    assert.deepEqual(ui.vibrations, [15]);
    await moveSlider(ui, 'sb', 100);
    assert.deepEqual(ui.vibrations, [15, 15]);
  });

  test('Cooling vibrates at its real minimum 20 and maximum 150', async () => {
    const ui = await boot();
    await moveSlider(ui, 'sco', 21);
    assert.deepEqual(ui.vibrations, []);
    await moveSlider(ui, 'sco', 20);
    assert.deepEqual(ui.vibrations, [15], 'tick at min 20');
    await moveSlider(ui, 'sco', 150);
    assert.deepEqual(ui.vibrations, [15, 15], 'tick at max 150');
  });

  test('Sparking and Blend vibrate at 0 and 255', async () => {
    const ui = await boot();
    for (const id of ['ssp', 'sbl']) { await moveSlider(ui, id, 0); await moveSlider(ui, id, 255); }
    assert.deepEqual(ui.vibrations, [15, 15, 15, 15]);
  });
});

// ===========================================================================
describe('theme buttons', () => {
  test('each theme button requests its own theme index', async () => {
    const ui = await boot();
    for (const t of [0, 1, 2, 3]) ui.el('tb' + t).click();
    await ui.flush();
    assert.deepEqual(ui.callsTo('/settheme').map(c => c.url),
      ['/settheme?v=0', '/settheme?v=1', '/settheme?v=2', '/settheme?v=3']);
    for (const c of ui.callsTo('/settheme')) assert.equal(c.opts.headers['X-Requested-With'], 'firelamp');
  });

  test('a theme tap gives a short haptic tick', async () => {
    const ui = await boot();
    ui.el('tb1').click();
    assert.deepEqual(ui.vibrations, [10]);
  });
});

// ===========================================================================
describe('changing the look clears the active preset and AI effect name', () => {
  test('moving Blend un-highlights the loaded preset', async () => {
    const ui = await withActivePreset();
    await moveSlider(ui, 'sbl', 120);
    assert.equal(ui.el('pr0').classList.contains('act'), false);
  });

  test('moving Blend clears the AI effect name', async () => {
    const ui = await boot();
    await withAiName(ui);
    await moveSlider(ui, 'sbl', 120);
    assert.equal(ui.el('ainame').textContent, '');
  });

  test('moving any slider un-highlights the loaded preset', async () => {
    for (const id of ['sb', 'sc', 'sco', 'ssp', 'sbl']) {
      const ui = await withActivePreset();
      await moveSlider(ui, id, 77);
      assert.equal(ui.el('pr0').classList.contains('act'), false, id);
    }
  });

  // Preset and AI-name halves are separate tests: a Surprise Me itself clears the
  // active preset, so combining them would make the preset assertion vacuous.
  test('a theme tap un-highlights the loaded preset', async () => {
    const ui = await withActivePreset();
    ui.el('tb2').click();
    assert.equal(ui.el('pr0').classList.contains('act'), false);
    await ui.respond(ui.last('/settheme'), ui.state({ th: 2 }));
    assert.equal(ui.el('pr0').classList.contains('act'), false, 'still cleared after the response');
  });

  test('a theme tap clears the AI effect name', async () => {
    const ui = await boot();
    await withAiName(ui);
    ui.el('tb2').click();
    assert.equal(ui.el('ainame').textContent, '');
  });

  test('Reset un-highlights the loaded preset', async () => {
    const ui = await withActivePreset();
    ui.el('rst').click();
    assert.equal(ui.el('pr0').classList.contains('act'), false);
    await ui.respond(ui.last('/reset'), ui.state());
    assert.equal(ui.el('pr0').classList.contains('act'), false, 'still cleared after the response');
  });

  test('Reset clears the AI effect name', async () => {
    const ui = await boot();
    await withAiName(ui);
    ui.el('rst').click();
    assert.equal(ui.el('ainame').textContent, '');
  });
});

// ===========================================================================
describe('unreachable lamp', () => {
  test('Reset with the lamp unreachable leaves no unhandled promise rejection', async () => {
    const ui = await boot();
    const seen = await unhandledDuring(async () => {
      ui.el('rst').click();
      await ui.fail(ui.last('/reset'));
    });
    assert.deepEqual(seen, []);
    assert.equal(val(ui, 'sb'), '100', 'UI state unchanged');
  });

  test('a theme tap with the lamp unreachable leaves no unhandled promise rejection', async () => {
    const ui = await boot();
    const seen = await unhandledDuring(async () => {
      ui.el('tb1').click();
      await ui.fail(ui.last('/settheme'));
    });
    assert.deepEqual(seen, []);
    assert.ok(ui.el('tb0').classList.contains('act'), 'theme highlight unchanged');
  });
});
