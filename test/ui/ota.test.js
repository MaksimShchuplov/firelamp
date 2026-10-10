'use strict';
/**
 * Behavioural tests for ui/js/ota.js: the update check button, the OTA
 * install → reboot-watch flow, and the Reset WiFi sheet.
 * Run with:  node --test test/ui/ota.test.js
 */
const { test, describe } = require('node:test');
const assert = require('node:assert/strict');
const { boot, sheetButton, checkAndInstall } = require('../ui_helpers.js');

// From the /update 200 to the first /info probe: doAfter's 5 s delay + one 3 s interval.
const FIRST_PROBE_MS = 8000;
const PROBE_EVERY_MS = 3000;

/** Check → Install → the lamp accepts /update with 200. */
async function acceptedOTA(ui) {
  await checkAndInstall(ui, 'aaa', 'bbb');
  await ui.respond(ui.last('/update'), 'Update starting...');
}

/** Advance the clock until the next /info probe is sent and return it. */
async function nextProbe(ui) {
  const before = ui.callsTo('/info').length;
  for (let i = 0; i < 100 && ui.callsTo('/info').length === before; i++) await ui.advance(100);
  assert.equal(ui.callsTo('/info').length, before + 1, 'expected exactly one new /info probe');
  return ui.last('/info');
}

const answer = async (ui, body) => ui.respond(await nextProbe(ui), body);
const unreachable = async ui => ui.fail(await nextProbe(ui));
const OLD_RUNNING = { uptime_s: 900, version: 'aaa' };   // old firmware, still up
const NEW_BOOTED = { uptime_s: 4, version: 'bbb' };      // new firmware, fresh boot

/**
 * Resolve a probe with headers only; the body arrives when the returned
 * function is called. Like a real fetch, aborting the request's signal
 * before then makes the body read reject.
 */
function respondSlowBody(call) {
  let deliver;
  const body = new Promise((res, rej) => {
    deliver = res;
    const sig = call.opts.signal;
    if (sig) sig.addEventListener('abort', () => rej(new DOMException('The operation was aborted.', 'AbortError')));
  });
  call.resolve({ ok: true, status: 200, json: () => body });
  return deliver;
}

// ===========================================================================
describe('update check button', () => {
  test('"Up to date" stays readable for 3 s, then the button is back to an enabled "Check for Update"', async () => {
    const ui = await boot();
    const chk = ui.el('chk');
    chk.click();
    assert.equal(chk.disabled, true, 'disabled while checking');
    await ui.respond(ui.last('/checkupdate'), { current: 'aaa', latest: 'aaa', update_available: false });
    assert.equal(chk.textContent, 'Up to date ✓');
    await ui.advance(2900);
    assert.equal(chk.textContent, 'Up to date ✓', 'confirmation still visible just before 3 s');
    await ui.advance(100);
    assert.equal(chk.textContent, 'Check for Update');
    assert.equal(chk.disabled, false, 'can check again');
    chk.click();
    assert.equal(ui.callsTo('/checkupdate').length, 2, 'a second check is sent');
  });

  test('a failed GitHub check leaves the button enabled so the user can retry', async () => {
    const ui = await boot();
    const chk = ui.el('chk');
    chk.click();
    await ui.respond(ui.last('/checkupdate'), { error: 'fetch_failed' });
    assert.equal(chk.textContent, 'Check failed');
    assert.equal(chk.disabled, false);
  });

  test('a dropped /checkupdate request leaves the button enabled so the user can retry', async () => {
    const ui = await boot();
    const chk = ui.el('chk');
    chk.click();
    await ui.fail(ui.last('/checkupdate'));
    assert.equal(chk.textContent, 'Network error');
    assert.equal(chk.disabled, false);
  });
});

// ===========================================================================
describe('OTA reboot watch', () => {
  test('/info probes start 8 s after /update is accepted and repeat every 3 s', async () => {
    const ui = await boot();
    await acceptedOTA(ui);
    assert.equal(ui.el('chk').textContent, 'Rebooting...');
    await ui.advance(FIRST_PROBE_MS - 1);
    assert.equal(ui.callsTo('/info').length, 0);
    await ui.advance(1);
    assert.equal(ui.callsTo('/info').length, 1);
    await ui.respond(ui.last('/info'), OLD_RUNNING);
    await ui.advance(PROBE_EVERY_MS - 1);
    assert.equal(ui.callsTo('/info').length, 1, 'no second probe before 3 s');
    await ui.advance(1);
    assert.equal(ui.callsTo('/info').length, 2);
  });

  test('a lamp unreachable for ~50 s of flashing and rebooting still ends in success and reload', async () => {
    const ui = await boot();
    await acceptedOTA(ui);
    for (let i = 0; i < 16; i++) await unreachable(ui);   // 8 s + 15 × 3 s = 53 s offline
    await answer(ui, NEW_BOOTED);
    assert.equal(ui.el('vinfo').textContent, 'Updated to bbb');
    assert.equal(ui.el('chk').textContent, 'Done! ✓');
    await ui.advance(2000);
    assert.equal(ui.reloads.length, 1);
  });

  test('after success no further /info probes are sent and the page reloads exactly once', async () => {
    const ui = await boot();
    await acceptedOTA(ui);
    await unreachable(ui);
    await answer(ui, NEW_BOOTED);
    const probes = ui.callsTo('/info').length;
    await ui.advance(10000);
    assert.equal(ui.callsTo('/info').length, probes, 'probing stopped');
    assert.equal(ui.reloads.length, 1);
  });

  test('a lamp still answering on the old firmware for 15 s after accepting the update is not a failure', async () => {
    const ui = await boot();
    await acceptedOTA(ui);
    for (let i = 0; i < 6; i++) await answer(ui, OLD_RUNNING);   // download still in progress
    assert.equal(ui.el('vinfo').textContent, 'Lamp rebooting...');
    await unreachable(ui);                                         // reboot
    await answer(ui, NEW_BOOTED);
    assert.equal(ui.el('vinfo').textContent, 'Updated to bbb');
    await ui.advance(2000);
    assert.equal(ui.reloads.length, 1);
  });

  test('a slow /info body during the download is not mistaken for the lamp going offline', async () => {
    const ui = await boot();
    await acceptedOTA(ui);
    const deliver = respondSlowBody(await nextProbe(ui));
    await ui.flush();                                               // headers handled
    await ui.advance(2500);                                         // body takes longer than the 2 s probe timeout
    deliver(OLD_RUNNING);
    await ui.flush();
    for (let i = 0; i < 5; i++) await answer(ui, OLD_RUNNING);     // still downloading
    assert.equal(ui.el('vinfo').textContent, 'Lamp rebooting...', 'no premature failure');
    await unreachable(ui);
    await unreachable(ui);
    await answer(ui, NEW_BOOTED);
    assert.equal(ui.el('vinfo').textContent, 'Updated to bbb');
  });

  test('a lamp that never goes offline fails on the 11th probe (~38 s), not later', async () => {
    const ui = await boot();
    await acceptedOTA(ui);
    for (let i = 0; i < 10; i++) await answer(ui, OLD_RUNNING);
    assert.equal(ui.el('vinfo').textContent, 'Lamp rebooting...', 'still waiting after 10 probes');
    assert.equal(ui.el('sb').disabled, true);
    await answer(ui, OLD_RUNNING);
    assert.equal(ui.el('vinfo').textContent, 'Update failed. Refresh the page to try again.');
    assert.equal(ui.el('sb').disabled, false, 'controls re-enabled');
  });

  test('a lamp that never comes back fails on the 21st probe (~68 s) with "Lamp not responding"', async () => {
    const ui = await boot();
    await acceptedOTA(ui);
    for (let i = 0; i < 20; i++) await unreachable(ui);
    assert.equal(ui.el('vinfo').textContent, 'Lamp rebooting...', 'still waiting after 20 failed probes');
    await unreachable(ui);
    assert.equal(ui.el('vinfo').textContent, 'Lamp not responding. Refresh manually.');
    assert.equal(ui.el('chk').textContent, 'Refresh page');
  });

  test('success needs uptime strictly below 120 s: a lamp at exactly 120 s is not a fresh boot', async () => {
    const ui = await boot();
    await acceptedOTA(ui);
    await unreachable(ui);
    await answer(ui, { uptime_s: 120, version: 'bbb' });
    assert.notEqual(ui.el('vinfo').textContent, 'Updated to bbb');
    assert.equal(ui.el('vinfo').textContent, 'Lamp rebooting...');
    await answer(ui, { uptime_s: 119, version: 'bbb' });
    assert.equal(ui.el('vinfo').textContent, 'Updated to bbb');
  });

  test('after a failed OTA, /info probing stops and /state polling takes over', async () => {
    const ui = await boot();
    await acceptedOTA(ui);
    await unreachable(ui);
    for (let i = 0; i < 5; i++) await answer(ui, OLD_RUNNING);
    assert.equal(ui.el('vinfo').textContent, 'Update failed. Refresh the page to try again.');
    const probes = ui.callsTo('/info').length;
    const polls = ui.callsTo('/state').length;
    await ui.advance(15000);
    assert.equal(ui.callsTo('/info').length, probes, 'no more /info probes');
    assert.ok(ui.callsTo('/state').length > polls, '/state polling resumed');
    assert.equal(ui.el('vinfo').textContent, 'Update failed. Refresh the page to try again.', 'message is stable');
  });

  test('the "Refresh page" button offered after a failed OTA is enabled and reloads', async () => {
    const ui = await boot();
    await checkAndInstall(ui, 'aaa', 'bbb');
    assert.equal(ui.el('chk').disabled, true, 'disabled while flashing');
    await ui.respond(ui.last('/update'), { error: 'busy' }, 503);
    assert.equal(ui.el('chk').textContent, 'Refresh page');
    assert.equal(ui.el('chk').disabled, false);
    ui.el('chk').click();
    assert.equal(ui.reloads.length, 1);
  });

  test('a network error on /update (ESP closes the connection) still proceeds to the reboot watch', async () => {
    const ui = await boot();
    await checkAndInstall(ui, 'aaa', 'bbb');
    await ui.fail(ui.last('/update'));
    assert.equal(ui.el('chk').textContent, 'Rebooting...');
    assert.equal(ui.el('vinfo').textContent, 'Lamp rebooting...');
    await unreachable(ui);
    await answer(ui, NEW_BOOTED);
    assert.equal(ui.el('vinfo').textContent, 'Updated to bbb');
    await ui.advance(2000);
    assert.equal(ui.reloads.length, 1);
  });
});

// ===========================================================================
describe('Reset WiFi', () => {
  test('confirming sends /resetwifi with the CSRF header and disables the button', async () => {
    const ui = await boot();
    const rwifi = ui.el('rwifi');
    rwifi.click();
    assert.equal(ui.el('shtit').textContent, 'Reset WiFi');
    assert.equal(ui.callsTo('/resetwifi').length, 0, 'nothing sent before confirming');
    sheetButton(ui, 'Reset and Reboot').click();
    await ui.flush();
    assert.equal(ui.callsTo('/resetwifi').length, 1);
    assert.equal(ui.last('/resetwifi').opts.headers['X-Requested-With'], 'firelamp');
    assert.equal(ui.callsTo('/reset').length, 0, 'fire parameters are not reset');
    assert.equal(rwifi.textContent, 'Rebooting...');
    assert.equal(rwifi.disabled, true, 'a second tap cannot fire another /resetwifi');
  });
});
