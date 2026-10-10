'use strict';
/**
 * Behavioural tests for ui/js/presets.js: the eight preset buttons, the name
 * box, load/save/delete requests, and JSON export/import.
 * Run with:  node --test test/ui/presets.test.js
 */
const { test, describe } = require('node:test');
const assert = require('node:assert/strict');
const { EMPTY_SLOTS, FILLED, boot, moveSlider, sheetButton } = require('../ui_helpers.js');

const CSRF = 'firelamp';
const isShown = (ui, id) => ui.el(id).classList.contains('show');
const isActive = (ui, slot) => ui.el('pr' + slot).classList.contains('act');
const csrf = call => call.opts.headers && call.opts.headers['X-Requested-With'];

/** Slots with `slot` filled under `name`, everything else empty. */
function slotsWith(fills) {
  return EMPTY_SLOTS.map(s => (fills[s.slot]
    ? { slot: s.slot, name: fills[s.slot], b: 40, c: 50, co: 46, sp: 26, bl: 50, th: 0 } : s));
}

/** Tap (click) an empty slot: opens the name box for it. */
async function tapSlot(ui, slot) { ui.el('pr' + slot).dispatch('click'); await ui.flush(); }

/** Hold the mouse on a preset long enough to trigger the long-press action. */
async function longPress(ui, slot) {
  const b = ui.el('pr' + slot);
  b.dispatch('mousedown'); await ui.advance(650);
  b.dispatch('mouseup'); b.dispatch('click');
  await ui.flush();
}

async function typeAndSave(ui, name) {
  ui.el('prename').value = name;
  ui.el('presave').click();
  await ui.flush();
}

async function key(ui, k) {
  const ev = ui.el('prename').dispatch('keydown', { key: k });
  await ui.flush();
  return ev;
}

/** Load preset `slot` via a tap and answer /loadpreset with a state. */
async function loadSlot(ui, slot) {
  ui.el('pr' + slot).dispatch('click');
  await ui.flush();
  await ui.respond(ui.last('/loadpreset'), ui.state({ b: 40 }));
}

/** Pick a file in the hidden import input. */
async function importFile(ui, content) {
  const inp = ui.el('prfile');
  const text = typeof content === 'string' ? content : JSON.stringify(content);
  inp.files = [{ name: 'firelamp-presets.json', text: () => Promise.resolve(text) }];
  inp.value = 'C:\\fakepath\\firelamp-presets.json';
  inp.dispatch('change');
  await ui.flush();
}

const nameParam = call => new URL('http://lamp' + call.url).searchParams.get('name');

// ===========================================================================
describe('preset buttons reflect the slot list', () => {
  test('after page load filled slots show their name and empty slots show "+"', async () => {
    const ui = await boot({ presets: FILLED });
    assert.equal(ui.el('pr0').textContent, 'Cozy');
    assert.ok(ui.el('pr0').classList.contains('filled'));
    assert.equal(ui.el('pr1').textContent, '+');
    assert.equal(ui.el('pr1').classList.contains('filled'), false);
  });

  test('loading a preset highlights exactly that slot', async () => {
    const ui = await boot({ presets: slotsWith({ 0: 'Cozy', 1: 'Blaze' }) });
    await loadSlot(ui, 0);
    assert.equal(isActive(ui, 0), true, 'loaded slot is highlighted');
    assert.equal(isActive(ui, 1), false, 'neighbour is not');
    const call = ui.last('/loadpreset');
    assert.equal(call.url, '/loadpreset?slot=0');
    assert.equal(csrf(call), CSRF);
  });

  test('a failed load (preset deleted elsewhere) does not highlight it and refreshes the list', async () => {
    const ui = await boot({ presets: FILLED });
    const before = ui.callsTo('/getpresets').length;
    ui.el('pr0').dispatch('click');
    await ui.flush();
    await ui.respond(ui.last('/loadpreset'), { error: 'empty' }, 404);
    assert.equal(isActive(ui, 0), false, 'not highlighted as loaded');
    assert.equal(ui.callsTo('/getpresets').length, before + 1, 'slot list re-fetched');
    await ui.respond(ui.last('/getpresets'), EMPTY_SLOTS);
    assert.equal(ui.el('pr0').textContent, '+', 'stale name replaced');
  });
});

// ===========================================================================
describe('pointer gestures on a preset button', () => {
  test('a slow synthetic click (300 ms after touchend) does not load the preset twice', async () => {
    const ui = await boot({ presets: FILLED });
    const b = ui.el('pr0');
    b.dispatch('touchstart'); await ui.advance(80);
    b.dispatch('touchend'); await ui.advance(300);
    b.dispatch('click'); await ui.flush();
    assert.equal(ui.callsTo('/loadpreset').length, 1);
  });

  test('a touch that turns into a scroll (touchmove) never opens the action sheet', async () => {
    const ui = await boot({ presets: FILLED });
    const b = ui.el('pr0');
    b.dispatch('touchstart'); await ui.advance(100);
    b.dispatch('touchmove'); await ui.advance(1000);
    assert.equal(isShown(ui, 'sheet'), false);
    assert.equal(isShown(ui, 'prein'), false);
  });

  test('a touch-scroll starting on an empty slot never opens the name box', async () => {
    const ui = await boot();
    const b = ui.el('pr2');
    b.dispatch('touchstart'); await ui.advance(100);
    b.dispatch('touchmove'); await ui.advance(1000);
    assert.equal(isShown(ui, 'prein'), false);
  });

  test('pressing the mouse and dragging off the button cancels the long press', async () => {
    const ui = await boot({ presets: FILLED });
    const b = ui.el('pr0');
    b.dispatch('mousedown'); await ui.advance(100);
    b.dispatch('mouseleave'); await ui.advance(1000);
    assert.equal(isShown(ui, 'sheet'), false);
    assert.equal(ui.vibrations.length, 0, 'no long-press haptic either');
  });

  test('after one click, a later mouseup over the button (drag from elsewhere) does not reload', async () => {
    const ui = await boot({ presets: FILLED });
    const b = ui.el('pr0');
    b.dispatch('mousedown'); await ui.advance(80);
    b.dispatch('mouseup'); b.dispatch('click');
    await ui.flush();
    assert.equal(ui.callsTo('/loadpreset').length, 1);
    await ui.advance(1000);
    b.dispatch('mouseup'); await ui.flush();
    assert.equal(ui.callsTo('/loadpreset').length, 1, 'a stray mouseup must not load again');
  });
});

// ===========================================================================
describe('name box prefill', () => {
  test('tapping an empty slot opens the name box with a 1-based default name', async () => {
    const ui = await boot();
    await tapSlot(ui, 1);
    assert.ok(isShown(ui, 'prein'));
    assert.equal(ui.el('prename').value, 'Preset 2');
    assert.equal(ui.el('prename').placeholder, 'Name...');
  });

  test('in Russian the default name is "Пресет N"', async () => {
    const ui = await boot({ lang: 'ru' });
    await tapSlot(ui, 0);
    assert.equal(ui.el('prename').value, 'Пресет 1');
  });

  test('"Save to this slot" on a filled preset keeps its existing name', async () => {
    const ui = await boot({ presets: FILLED });
    await longPress(ui, 0);
    assert.ok(isShown(ui, 'sheet'));
    sheetButton(ui, 'Save to this slot').click();
    await ui.flush();
    assert.ok(isShown(ui, 'prein'));
    assert.equal(ui.el('prename').value, 'Cozy');
  });

  test('the AI effect name is offered until a slider changes the look', async () => {
    const ui = await boot();
    ui.el('surprise').click();
    await ui.flush();
    await ui.respond(ui.last('/surprise'), Object.assign(ui.state(), { name: 'Aurora' }));
    await tapSlot(ui, 1);
    assert.equal(ui.el('prename').value, 'Aurora', 'AI name offered right after Surprise Me');
    ui.el('precancel').click();
    await moveSlider(ui, 'sb', 30);
    await tapSlot(ui, 2);
    assert.equal(ui.el('prename').value, 'Preset 3', 'stale AI name no longer offered');
  });
});

// ===========================================================================
describe('saving a preset', () => {
  test('save sends slot and name with the CSRF header, closes the box and highlights the slot', async () => {
    const ui = await boot();
    await tapSlot(ui, 1);
    await typeAndSave(ui, 'Night');
    assert.equal(isShown(ui, 'prein'), false, 'box closes on save');
    const call = ui.last('/savepreset');
    assert.equal(call.url, '/savepreset?slot=1&name=Night');
    assert.equal(csrf(call), CSRF);
    await ui.respond(call, { ok: true });
    await ui.respond(ui.last('/getpresets'), slotsWith({ 1: 'Night' }));
    assert.equal(ui.el('pr1').textContent, 'Night');
    assert.equal(isActive(ui, 1), true);
  });

  test('names are trimmed and clamped to 15 codepoints (emoji count as one)', async () => {
    const ui = await boot();
    await tapSlot(ui, 0);
    await typeAndSave(ui, '  ' + '🔥'.repeat(14) + 'XYZ  ');
    assert.equal(nameParam(ui.last('/savepreset')), '🔥'.repeat(14) + 'X');
  });

  test('surrounding spaces do not count toward the 15-character limit', async () => {
    const ui = await boot();
    await tapSlot(ui, 0);
    await typeAndSave(ui, '   abcdefghijklmno   ');
    assert.equal(nameParam(ui.last('/savepreset')), 'abcdefghijklmno');
  });

  test('names with &, # and + reach the lamp intact', async () => {
    const ui = await boot();
    await tapSlot(ui, 0);
    await typeAndSave(ui, 'R&B #1+x');
    const call = ui.last('/savepreset');
    assert.equal(call.url, '/savepreset?slot=0&name=' + encodeURIComponent('R&B #1+x'));
    assert.equal(nameParam(call), 'R&B #1+x');
  });

  test('an empty or whitespace-only name is refused: no request, box stays open and focused', async () => {
    const ui = await boot();
    await tapSlot(ui, 0);
    await typeAndSave(ui, '');
    assert.equal(ui.callsTo('/savepreset').length, 0);
    assert.ok(isShown(ui, 'prein'));
    assert.equal(ui.ctx.document.activeElement, ui.el('prename'));
    await typeAndSave(ui, '    ');
    assert.equal(ui.callsTo('/savepreset').length, 0, 'whitespace-only is refused too');
    assert.ok(isShown(ui, 'prein'));
  });

  test('the save button does nothing when no slot is being named', async () => {
    const ui = await boot();
    await typeAndSave(ui, 'Ghost');
    assert.equal(ui.callsTo('/savepreset').length, 0);
  });

  test('Escape closes the name box; a following Enter saves nothing', async () => {
    const ui = await boot();
    await tapSlot(ui, 3);
    await key(ui, 'Escape');
    assert.equal(isShown(ui, 'prein'), false);
    await key(ui, 'Enter');
    assert.equal(ui.callsTo('/savepreset').length, 0);
  });

  test('the cancel button closes the name box and a later save click sends nothing', async () => {
    const ui = await boot();
    await tapSlot(ui, 3);
    ui.el('precancel').click();
    assert.equal(isShown(ui, 'prein'), false);
    await typeAndSave(ui, 'Late');
    assert.equal(ui.callsTo('/savepreset').length, 0);
  });

  test('Enter in the name box saves', async () => {
    const ui = await boot();
    await tapSlot(ui, 2);
    ui.el('prename').value = 'Embers';
    await key(ui, 'Enter');
    assert.equal(ui.last('/savepreset').url, '/savepreset?slot=2&name=Embers');
  });

  test('a second save started within 8 s of a completed one still goes out', async () => {
    const ui = await boot();
    await tapSlot(ui, 1);
    await typeAndSave(ui, 'One');
    await ui.respond(ui.last('/savepreset'), { ok: true });
    await ui.respond(ui.last('/getpresets'), slotsWith({ 1: 'One' }));
    await ui.advance(5000);
    await tapSlot(ui, 2);
    ui.el('prename').value = 'Two';
    await ui.advance(4000);                              // past the first save's 8 s timeout
    ui.el('presave').click();
    await ui.flush();
    assert.deepEqual(ui.callsTo('/savepreset').map(c => c.url),
      ['/savepreset?slot=1&name=One', '/savepreset?slot=2&name=Two']);
  });

  test('opening another slot before the save answers still highlights the saved slot', async () => {
    const ui = await boot();
    await tapSlot(ui, 1);
    await typeAndSave(ui, 'One');
    await tapSlot(ui, 2);                                // user opens slot 3's name box meanwhile
    await ui.respond(ui.last('/savepreset'), { ok: true });
    await ui.respond(ui.last('/getpresets'), slotsWith({ 1: 'One' }));
    assert.equal(isActive(ui, 1), true, 'the saved slot is highlighted');
    assert.equal(isActive(ui, 2), false, 'the slot being named is not');
  });

  test('opening then cancelling another slot before the save answers still highlights the saved slot', async () => {
    const ui = await boot();
    await tapSlot(ui, 1);
    await typeAndSave(ui, 'One');
    await tapSlot(ui, 2);
    ui.el('precancel').click();
    await ui.respond(ui.last('/savepreset'), { ok: true });
    await ui.respond(ui.last('/getpresets'), slotsWith({ 1: 'One' }));
    assert.equal(isActive(ui, 1), true);
  });
});

// ===========================================================================
describe('deleting a preset', () => {
  test('Delete sends /deletepreset?slot=N and un-highlights the deleted active slot', async () => {
    const ui = await boot({ presets: FILLED });
    await loadSlot(ui, 0);
    assert.equal(isActive(ui, 0), true, 'precondition: slot 0 is active');
    await longPress(ui, 0);
    sheetButton(ui, 'Delete preset').click();
    await ui.flush();
    const call = ui.last('/deletepreset');
    assert.equal(call.url, '/deletepreset?slot=0');
    assert.equal(csrf(call), CSRF);
    await ui.respond(call, { ok: true });
    await ui.respond(ui.last('/getpresets'), EMPTY_SLOTS);
    assert.equal(ui.el('pr0').textContent, '+');
    assert.equal(isActive(ui, 0), false, 'empty "+" button must not stay highlighted');
  });
});

// ===========================================================================
describe('export', () => {
  function spyDownload(ui) {
    const rec = { blobs: [], revoked: [], anchors: [], clicked: [] };
    ui.ctx.URL = {
      createObjectURL: b => { rec.blobs.push(b); return 'blob:presets'; },
      revokeObjectURL: u => rec.revoked.push({ u, at: ui.clock.now }),
    };
    const orig = ui.ctx.document.createElement;
    ui.ctx.document.createElement = tag => {
      const e = orig(tag);
      if (String(tag).toLowerCase() === 'a') {
        rec.anchors.push(e);
        e.addEventListener('click', () => rec.clicked.push({ href: e.href, download: e.download }));
      }
      return e;
    };
    return rec;
  }

  test('Export downloads the slot list from /getpresets as firelamp-presets.json', async () => {
    const ui = await boot({ presets: FILLED });
    const rec = spyDownload(ui);
    const before = ui.callsTo('/getpresets').length;
    ui.el('prexp').click();
    await ui.flush();
    assert.equal(ui.callsTo('/getpresets').length, before + 1, 'fresh /getpresets fetch');
    assert.equal(ui.callsTo('/state').length, 1, 'no state fetch');
    await ui.respond(ui.last('/getpresets'), FILLED);
    assert.equal(rec.blobs.length, 1);
    assert.deepEqual(JSON.parse(rec.blobs[0].parts.join('')), FILLED);
    assert.deepEqual(rec.clicked, [{ href: 'blob:presets', download: 'firelamp-presets.json' }]);
  });

  test('the blob URL is revoked only after the download has had 2 s to start', async () => {
    const ui = await boot({ presets: FILLED });
    const rec = spyDownload(ui);
    ui.el('prexp').click();
    await ui.flush();
    await ui.respond(ui.last('/getpresets'), FILLED);
    const clickedAt = ui.clock.now;
    assert.equal(rec.revoked.length, 0, 'not revoked at click time');
    await ui.advance(1900);
    assert.equal(rec.revoked.length, 0, 'still alive at 1.9 s');
    await ui.advance(200);
    assert.equal(rec.revoked.length, 1);
    assert.equal(rec.revoked[0].u, 'blob:presets');
    assert.ok(rec.revoked[0].at - clickedAt >= 2000);
  });
});

// ===========================================================================
describe('import', () => {
  test('the Import button opens the file picker', async () => {
    const ui = await boot();
    let opened = 0;
    ui.el('prfile').addEventListener('click', () => opened++);
    ui.el('primp').click();
    assert.equal(opened, 1);
  });

  test('valid entries are saved one at a time with all six params; out-of-range slots are skipped', async () => {
    const ui = await boot();
    await importFile(ui, [
      { slot: 0, name: 'Cozy', b: 40, c: 55, co: 46, sp: 26, bl: 50, th: 2 },
      { slot: 8, name: 'TooHigh', b: 10 },
      { slot: -1, name: 'Negative', b: 10 },
      { slot: 3, name: 'Ice' },
    ]);
    assert.deepEqual(ui.pending('/savepreset').map(c => c.url),
      ['/savepreset?slot=0&name=Cozy&b=40&c=55&co=46&sp=26&bl=50&th=2'],
      'only the first save is in flight');
    assert.equal(csrf(ui.last('/savepreset')), CSRF);
    const presetsBefore = ui.callsTo('/getpresets').length;
    await ui.respond(ui.last('/savepreset'), { ok: true });
    assert.deepEqual(ui.pending('/savepreset').map(c => c.url), ['/savepreset?slot=3&name=Ice'],
      'the next save starts only after the previous one answered');
    await ui.respond(ui.last('/savepreset'), { ok: true });
    assert.deepEqual(ui.callsTo('/savepreset').map(c => c.url), [
      '/savepreset?slot=0&name=Cozy&b=40&c=55&co=46&sp=26&bl=50&th=2',
      '/savepreset?slot=3&name=Ice',
    ], 'slots 8 and -1 were never sent');
    assert.equal(ui.callsTo('/getpresets').length, presetsBefore + 1, 'slot list refreshed after import');
  });

  test('one failed save does not abort the remaining imports', async () => {
    const ui = await boot();
    await importFile(ui, [{ slot: 0, name: 'A' }, { slot: 1, name: 'B' }]);
    await ui.fail(ui.last('/savepreset'));
    assert.deepEqual(ui.callsTo('/savepreset').map(c => c.url),
      ['/savepreset?slot=0&name=A', '/savepreset?slot=1&name=B']);
  });

  test('the file input is cleared so the same file can be imported again', async () => {
    const ui = await boot();
    await importFile(ui, [{ slot: 0, name: 'A' }]);
    assert.equal(ui.el('prfile').value, '');
  });

  test('a non-JSON file shows the "Invalid presets file" alert and sends nothing', async () => {
    const ui = await boot();
    await importFile(ui, 'not json');
    assert.deepEqual(ui.alerts, ['Invalid presets file']);
    assert.equal(ui.callsTo('/savepreset').length, 0);
  });
});
