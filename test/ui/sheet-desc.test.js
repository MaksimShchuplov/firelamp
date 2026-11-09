'use strict';
/**
 * Behavioural tests for the action sheet and slider descriptions (ui/js/ui.js).
 * Run with:  node --test test/ui/sheet-desc.test.js
 */
const { test, describe } = require('node:test');
const assert = require('node:assert/strict');
const { boot, val } = require('../ui_helpers.js');

function realInnerHTML(el) {
  let html = el.innerHTML;
  Object.defineProperty(el, 'innerHTML', {
    get: () => html,
    set(v) { html = String(v); el.children.length = 0; },
  });
}
// Open the Reset WiFi action sheet through its real button.
function openWifiSheet(ui) { ui.el('rwifi').click(); }
const sheetOpen = ui => ui.el('sheet').classList.contains('show');
const dimShown = ui => ui.el('shdim').classList.contains('show');
const sheetLabels = ui => ui.el('shbtns').children.map(c => c.textContent);
function sheetBtn(ui, label) {
  const b = ui.el('shbtns').children.find(c => c.textContent === label);
  assert.ok(b, `sheet button "${label}" present`);
  return b;
}


// ===========================================================================
describe('action sheet (ui.js)', () => {
  test('choosing an action closes the sheet and its dim overlay, then runs the action', async () => {
    const ui = await boot();
    openWifiSheet(ui);
    assert.ok(sheetOpen(ui) && dimShown(ui), 'precondition: sheet open');
    sheetBtn(ui, 'Reset and Reboot').click();
    await ui.flush();
    assert.equal(sheetOpen(ui), false);
    assert.equal(dimShown(ui), false);
    assert.equal(ui.callsTo('/resetwifi').length, 1);
  });

  test('Cancel closes the sheet and the dim overlay without running the action', async () => {
    const ui = await boot();
    openWifiSheet(ui);
    sheetBtn(ui, 'Cancel').click();
    await ui.flush();
    assert.equal(sheetOpen(ui), false);
    assert.equal(dimShown(ui), false);
    assert.equal(ui.callsTo('/resetwifi').length, 0);
  });

  test('tapping the dim overlay outside the sheet dismisses it', async () => {
    const ui = await boot();
    openWifiSheet(ui);
    ui.el('shdim').click();
    await ui.flush();
    assert.equal(sheetOpen(ui), false);
    assert.equal(dimShown(ui), false);
    assert.equal(ui.callsTo('/resetwifi').length, 0);
  });

  test('reopening a sheet shows only its own buttons, not those of earlier sheets', async () => {
    const ui = await boot();
    realInnerHTML(ui.el('shbtns'));
    openWifiSheet(ui);
    sheetBtn(ui, 'Cancel').click();
    openWifiSheet(ui);
    assert.deepEqual(sheetLabels(ui), ['Reset and Reboot', 'Cancel']);
  });
});

// ===========================================================================
describe('slider descriptions (ui.js)', () => {
  test('values on a bucket\'s upper edge get that bucket\'s description', async () => {
    const ui = await boot();
    await ui.advance(5000);
    await ui.respond(ui.last('/state'), ui.state({ b: 25, c: 55, sp: 40, bl: 255 }));
    assert.equal(ui.el('db').textContent, 'Very dim');
    assert.equal(ui.el('dc').textContent, 'Warm balanced');
    assert.equal(ui.el('dsp').textContent, 'Calm smoldering');
    assert.equal(ui.el('dbl').textContent, 'Sharp flicker');
    await ui.advance(5000);
    await ui.respond(ui.last('/state'), ui.state({ b: 100 }));
    assert.equal(ui.el('db').textContent, 'Full brightness');
  });

  test('the sparking description follows the sparking value', async () => {
    const ui = await boot();
    await ui.advance(5000);
    await ui.respond(ui.last('/state'), ui.state({ sp: 100 }));
    assert.equal(ui.el('dsp').textContent, 'Active fire');
    await ui.advance(5000);
    await ui.respond(ui.last('/state'), ui.state({ sp: 230 }));
    assert.equal(ui.el('dsp').textContent, 'Raging maximum');
  });

  test('Russian users see slider descriptions in Russian', async () => {
    const ui = await boot({ lang: 'ru' });
    assert.equal(ui.el('db').textContent, 'Максимум яркости');
    assert.equal(ui.el('dsp').textContent, 'Тихое тление');
  });

  test('switching language re-translates every slider description, blend included', async () => {
    const ui = await boot();
    assert.equal(ui.el('dbl').textContent, 'Slow motion');
    ui.el('lru').click();
    assert.equal(ui.el('db').textContent, 'Максимум яркости');
    assert.equal(ui.el('dc').textContent, 'Тёплые сбалансированные');
    assert.equal(ui.el('dco').textContent, 'Высокое пламя');
    assert.equal(ui.el('dsp').textContent, 'Тихое тление');
    assert.equal(ui.el('dbl').textContent, 'Замедленное движение');
  });
});

