'use strict';
/**
 * Shared drivers for the behavioural UI suites in test/ui/*.test.js.
 * Everything here drives the REAL ui/js scripts through ui_harness.js.
 * Run the suites with:  node --test 'test/ui/*.test.js'
 */
const assert = require('node:assert/strict');
const { loadUI } = require('./ui_harness.js');

const EMPTY_SLOTS = Array.from({ length: 8 }, (_, i) => ({ slot: i, name: '' }));

/** Load the UI and settle the requests every page load makes. */
async function boot(opts) {
  const ui = loadUI(opts);
  await ui.flush();
  await ui.respond(ui.last('/getpresets'), (opts && opts.presets) || EMPTY_SLOTS);
  await ui.respond(ui.last('/state'), ui.state());
  return ui;
}

const val = (ui, id) => String(ui.el(id).value);
const offline = ui => ui.el('offb').classList.contains('show');

async function moveSlider(ui, id, v) {
  ui.el(id).value = String(v);
  ui.el(id).dispatch('input');
  await ui.flush();
}

/** Let the in-flight /state poll time out (4 s abort) without answering it. */
async function stallPoll(ui) { await ui.advance(5000); }


// OTA helpers: drive the real Check → Install sheet → startOTA path.
function sheetButton(ui, label) {
  const kids = ui.el('shbtns').children;
  for (let i = kids.length - 1; i >= 0; i--) if (kids[i].textContent === label) return kids[i];
  throw new Error(`sheet button "${label}" not found`);
}
async function checkAndInstall(ui, current, latest) {
  ui.el('chk').click();
  await ui.respond(ui.last('/checkupdate'), { current, latest, update_available: true });
  ui.el('chk').click();                                  // now the install handler
  sheetButton(ui, 'Install').click();
  await ui.flush();
}
/** Advance to the next /info probe; answer it with `body`, or let it time out if null. */
async function rebootProbe(ui, body) {
  const before = ui.callsTo('/info').length;
  for (let i = 0; i < 10 && ui.callsTo('/info').length === before; i++) await ui.advance(1000);
  const c = ui.last('/info');
  assert.ok(ui.callsTo('/info').length > before, 'expected an /info probe');
  if (body) await ui.respond(c, body); else await ui.advance(2100);
}
async function startedOTA(ui) {
  await checkAndInstall(ui, 'aaa', 'bbb');
  await ui.respond(ui.last('/update'), 'Update starting...');
  await ui.advance(5000);                                // doAfter → pollReboot
  await rebootProbe(ui, null);                           // lamp goes offline
}


const FILLED = EMPTY_SLOTS.map((s, i) => (i === 0 ? { slot: 0, name: 'Cozy', b: 40, c: 50, co: 46, sp: 26, bl: 50, th: 0 } : s));

module.exports = {
  loadUI, EMPTY_SLOTS, FILLED, boot, val, offline, moveSlider, stallPoll,
  sheetButton, checkAndInstall, rebootProbe, startedOTA,
};
