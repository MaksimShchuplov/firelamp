'use strict';
/**
 * Behavioural tests for the browser UI: polling lifecycle, sliders, OTA, AI and
 * presets. Runs the real ui/js scripts (see ui_harness.js) against a virtual
 * clock and a hand-driven fetch(), so timing races are reproduced exactly.
 *
 * Every historical bug fixed in these paths has a test here, and
 * test/ui_mutants.js re-introduces each one to prove the suite catches it.
 *
 * Run with:  node test/test_ui_behaviour.js
 */
const { test, describe } = require('node:test');
const assert = require('node:assert/strict');
const { execFileSync } = require('node:child_process');
const path = require('node:path');
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

// ===========================================================================
describe('poll lifecycle (state.js / poll.js)', () => {
  test('polls /state at load and then every 5 s', async () => {
    const ui = await boot();
    assert.equal(ui.callsTo('/state').length, 1);
    await ui.advance(5000);
    assert.equal(ui.callsTo('/state').length, 2);
    await ui.advance(5000);
    assert.equal(ui.callsTo('/state').length, 3);
  });

  test('a hidden tab skips the interval poll; becoming visible polls immediately', async () => {
    const ui = await boot();
    await ui.setHidden(true);
    await ui.advance(15000);
    assert.equal(ui.callsTo('/state').length, 1, 'no polls while hidden');
    await ui.setHidden(false);
    assert.equal(ui.callsTo('/state').length, 2, 'immediate poll on becoming visible');
  });

  test('pausePoll stops polling and discards the response already in flight', async () => {
    const ui = await boot();
    await ui.advance(5000);
    const inflight = ui.last('/state');
    ui.ctx.pausePoll();
    await ui.advance(30000);
    assert.equal(ui.callsTo('/state').length, 2, 'no new polls while paused');
    await ui.respond(inflight, ui.state({ b: 7 }));
    assert.equal(val(ui, 'sb'), '100', 'pre-pause response must not be applied');
  });

  test('visibilitychange while paused does not poll', async () => {
    const ui = await boot();
    ui.ctx.pausePoll();
    await ui.setHidden(true);
    await ui.setHidden(false);
    assert.equal(ui.callsTo('/state').length, 1);
  });

  test('resumePoll is idempotent — never two intervals', async () => {
    const ui = await boot();
    ui.ctx.resumePoll();
    ui.ctx.resumePoll();
    assert.equal(ui.clock.intervals(), 1);
    await ui.advance(5000);
    assert.equal(ui.callsTo('/state').length, 2, 'exactly one poll per 5 s');
  });

  test('three stalled polls raise the offline banner — not two', async () => {
    const ui = await boot();
    await stallPoll(ui); await stallPoll(ui);           // polls at 5 s and 10 s time out
    await ui.advance(4000);                              // second one aborts at 14 s
    assert.equal(offline(ui), false, 'two failures must not show the banner');
    await ui.advance(5000);                              // third aborts at 19 s
    assert.equal(offline(ui), true, 'three consecutive failures show the banner');
  });

  test('a successful poll clears the banner and the failure count', async () => {
    const ui = await boot();
    for (let i = 0; i < 4; i++) await stallPoll(ui);
    assert.equal(offline(ui), true);
    await ui.respond(ui.last('/state'), ui.state());
    assert.equal(offline(ui), false);
    assert.equal(ui.ctx.pullFails, 0);
  });

  test('a response discarded by the slider gate still clears the banner', async () => {
    const ui = await boot();
    for (let i = 0; i < 4; i++) await stallPoll(ui);
    assert.equal(offline(ui), true);
    const p = ui.last('/state');
    await moveSlider(ui, 'sb', 30);                      // gate now discards p
    await ui.respond(p, ui.state({ b: 90 }));
    assert.equal(val(ui, 'sb'), '30', 'state must not be applied');
    assert.equal(offline(ui), false, 'but the lamp answered — banner must clear');
  });

  test('a poll in flight when a slider moves never overwrites it, however late it lands', async () => {
    const ui = await boot();
    await ui.advance(5000);
    const p = ui.last('/state');
    await ui.advance(100);
    await moveSlider(ui, 'sb', 30);
    await ui.advance(1500);                              // past the 1 s window
    await ui.respond(p, ui.state({ b: 100 }));
    assert.equal(val(ui, 'sb'), '30');
  });

  test('a poll sent within 1 s of a slider move is discarded', async () => {
    const ui = await boot();
    await moveSlider(ui, 'sb', 30);
    await ui.advance(200);
    ui.ctx.pull();
    await ui.respond(ui.last('/state'), ui.state({ b: 100 }));
    assert.equal(val(ui, 'sb'), '30');
  });

  test('an ordinary poll applies the lamp state', async () => {
    const ui = await boot();
    await ui.advance(5000);
    await ui.respond(ui.last('/state'), ui.state({ b: 42, co: 99, th: 2, w: 7.25 }));
    assert.equal(val(ui, 'sb'), '42');
    assert.equal(ui.el('vb').textContent, 42);
    assert.equal(val(ui, 'sco'), '99');
    assert.ok(ui.el('tb2').classList.contains('act'));
    assert.equal(ui.el('vw').textContent, '7.3');
  });

  test('an older poll answering after a newer one is ignored', async () => {
    const ui = await boot();
    ui.ctx.pull(); const older = ui.last('/state');
    ui.ctx.pull(); const newer = ui.last('/state');
    await ui.respond(newer, ui.state({ b: 20 }));
    await ui.respond(older, ui.state({ b: 80 }));
    assert.equal(val(ui, 'sb'), '20');
  });
});

// ===========================================================================
describe('sliders (sliders.js)', () => {
  test('the debounced request carries the value at input time, not at send time', async () => {
    const ui = await boot();
    await moveSlider(ui, 'sb', 30);
    ui.el('sb').value = '99';                            // e.g. rewritten by a late poll
    await ui.advance(150);
    assert.deepEqual(ui.callsTo('/setb').map(c => c.url), ['/setb?v=30']);
  });

  test('rapid input coalesces into one request carrying the last value', async () => {
    const ui = await boot();
    for (const v of [10, 20, 30]) { await moveSlider(ui, 'sco', v); await ui.advance(50); }
    await ui.advance(200);
    assert.deepEqual(ui.callsTo('/setco').map(c => c.url), ['/setco?v=30']);
  });

  test('every slider sends to its own endpoint with the CSRF header', async () => {
    const ui = await boot();
    const map = { sb: '/setb', sc: '/setc', sco: '/setco', ssp: '/setsp', sbl: '/setbl' };
    for (const [id, ep] of Object.entries(map)) await moveSlider(ui, id, 40);
    await ui.advance(200);
    for (const ep of Object.values(map)) {
      const c = ui.last(ep);
      assert.ok(c, `${ep} not called`);
      assert.equal(c.url, `${ep}?v=40`);
      assert.equal(c.opts.headers['X-Requested-With'], 'firelamp');
    }
  });

  test('a theme tap applies the /settheme response directly, without a poll', async () => {
    const ui = await boot();
    const polls = ui.callsTo('/state').length;
    ui.el('tb3').click();
    await ui.respond(ui.last('/settheme'), ui.state({ th: 3 }));
    assert.ok(ui.el('tb3').classList.contains('act'));
    assert.equal(ui.callsTo('/state').length, polls, 'must not schedule a gated poll');
  });

  test('a theme tap right after a slider move still applies', async () => {
    const ui = await boot();
    await moveSlider(ui, 'sb', 30);
    ui.el('tb2').click();
    await ui.respond(ui.last('/settheme'), ui.state({ b: 30, th: 2 }));
    assert.ok(ui.el('tb2').classList.contains('act'));
  });

  test('Reset applies its response directly, even right after a slider move', async () => {
    const ui = await boot();
    await moveSlider(ui, 'sc', 90);
    ui.el('rst').click();
    await ui.respond(ui.last('/reset'), ui.state({ c: 50 }));
    assert.equal(val(ui, 'sc'), '50');
  });
});

// ===========================================================================
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

describe('OTA (ota.js)', () => {
  test('starting OTA pauses polling for the whole update', async () => {
    const ui = await boot();
    await checkAndInstall(ui, 'aaa', 'bbb');
    const polls = ui.callsTo('/state').length;
    await ui.advance(60000);
    assert.equal(ui.callsTo('/state').length, polls);
    assert.equal(ui.ctx.pollPaused, true);
  });

  test('a non-OK /update fails immediately and resumes polling', async () => {
    const ui = await boot();
    await checkAndInstall(ui, 'aaa', 'bbb');
    await ui.respond(ui.last('/update'), { error: 'busy' }, 503);
    assert.match(ui.el('vinfo').textContent, /Could not start/);
    assert.equal(ui.el('chk').dataset.mode, 'reload');
    assert.equal(ui.ctx.otaActive, false);
    await ui.advance(15000);
    assert.equal(ui.callsTo('/info').length, 0, 'no reboot polling after a refused update');
    assert.ok(ui.callsTo('/state').length >= 2, 'polling resumed');
  });

  test('rebooted into the new version → success and reload', async () => {
    const ui = await boot();
    await startedOTA(ui);
    await rebootProbe(ui, { uptime_s: 4, version: 'bbb' });
    assert.match(ui.el('vinfo').textContent, /Updated to bbb/);
    await ui.advance(2500);
    assert.equal(ui.reloads.length, 1);
  });

  test('fresh uptime but the OLD version is not success', async () => {
    const ui = await boot();
    await startedOTA(ui);
    for (let i = 0; i < 5; i++) await rebootProbe(ui, { uptime_s: 4 + i * 3, version: 'aaa' });
    assert.doesNotMatch(ui.el('vinfo').textContent, /Updated to/);
    assert.match(ui.el('vinfo').textContent, /Update failed/);
    await ui.advance(3000);
    assert.equal(ui.reloads.length, 0);
  });

  test('old firmware answering with high uptime fails after five confirmations, not never', async () => {
    const ui = await boot();
    await startedOTA(ui);
    for (let i = 0; i < 4; i++) await rebootProbe(ui, { uptime_s: 900, version: 'aaa' });
    assert.doesNotMatch(ui.el('vinfo').textContent, /Update failed/, 'not before the 5th');
    await rebootProbe(ui, { uptime_s: 900, version: 'aaa' });
    assert.match(ui.el('vinfo').textContent, /Update failed/);
  });

  test('a failed OTA re-enables the controls and resumes polling', async () => {
    const ui = await boot();
    await startedOTA(ui);
    for (let i = 0; i < 5; i++) await rebootProbe(ui, { uptime_s: 900, version: 'aaa' });
    for (const id of ['sb', 'sc', 'surprise', 'rst']) assert.equal(ui.el(id).disabled, false, id);
    const polls = ui.callsTo('/state').length;
    await ui.advance(5000);
    assert.equal(ui.callsTo('/state').length, polls + 1);
  });

  test('switching language translates the Install label instead of resetting it', async () => {
    const ui = await boot();
    ui.el('chk').click();
    await ui.respond(ui.last('/checkupdate'), { current: 'aaa', latest: 'bbb', update_available: true });
    assert.equal(ui.el('chk').textContent, 'Install Update ↑');
    ui.el('lru').click();
    assert.equal(ui.el('chk').textContent, 'Установить обновление ↑');
    ui.el('len').click();
    assert.equal(ui.el('chk').textContent, 'Install Update ↑');
  });

  test('switching language translates the Refresh label after a failed update', async () => {
    const ui = await boot();
    await checkAndInstall(ui, 'aaa', 'bbb');
    await ui.respond(ui.last('/update'), { error: 'busy' }, 503);
    assert.equal(ui.el('chk').textContent, 'Refresh page');
    ui.el('lru').click();
    assert.equal(ui.el('chk').textContent, 'Обновить страницу');
  });
});

// ===========================================================================
describe('Surprise Me (ai.js)', () => {
  test('polling is paused while the request is in flight', async () => {
    const ui = await boot();
    ui.el('surprise').click();
    const polls = ui.callsTo('/state').length;
    await ui.advance(20000);
    assert.equal(ui.callsTo('/state').length, polls);
  });

  test('success applies the effect and resumes polling', async () => {
    const ui = await boot();
    ui.el('surprise').click();
    await ui.respond(ui.last('/surprise'), Object.assign(ui.state({ b: 61, th: 1 }), { name: 'Ember' }));
    assert.equal(val(ui, 'sb'), '61');
    assert.match(ui.el('ainame').textContent, /^Ember/);
    assert.equal(ui.el('surprise').disabled, false);
    const polls = ui.callsTo('/state').length;
    await ui.advance(5000);
    assert.equal(ui.callsTo('/state').length, polls + 1);
  });

  test('an error resumes polling and re-enables the button', async () => {
    const ui = await boot();
    ui.el('surprise').click();
    await ui.respond(ui.last('/surprise'), { error: 'http_error' }, 502);
    assert.equal(ui.el('surprise').disabled, false);
    assert.match(ui.el('ainame').textContent, /Gemini server error/);
    const polls = ui.callsTo('/state').length;
    await ui.advance(5000);
    assert.equal(ui.callsTo('/state').length, polls + 1);
  });

  test('a Surprise finishing after OTA started neither resumes polling nor re-enables itself', async () => {
    const ui = await boot();
    ui.el('surprise').click();
    const req = ui.last('/surprise');
    await checkAndInstall(ui, 'aaa', 'bbb');
    await ui.respond(req, Object.assign(ui.state(), { name: 'Late' }));
    assert.equal(ui.el('surprise').disabled, true);
    const polls = ui.callsTo('/state').length;
    await ui.advance(20000);
    assert.equal(ui.callsTo('/state').length, polls, 'OTA owns the poll pause');
  });

  test('the effect name is truncated to 15 codepoints, not UTF-16 units', async () => {
    const ui = await boot();
    ui.el('surprise').click();
    await ui.respond(ui.last('/surprise'), Object.assign(ui.state(), { name: '🔥'.repeat(20) }));
    assert.equal(Array.from(ui.ctx.lastAiName).length, 15);
  });
});

// ===========================================================================
const FILLED = EMPTY_SLOTS.map((s, i) => (i === 0 ? { slot: 0, name: 'Cozy', b: 40, c: 50, co: 46, sp: 26, bl: 50, th: 0 } : s));

describe('preset buttons (presets.js)', () => {
  test('a tap loads a filled preset exactly once despite the synthetic click', async () => {
    const ui = await boot({ presets: FILLED });
    const b = ui.el('pr0');
    b.dispatch('touchstart'); await ui.advance(80);
    b.dispatch('touchend');   b.dispatch('click');       // browsers synthesise click after touchend
    await ui.flush();
    assert.equal(ui.callsTo('/loadpreset').length, 1);
  });

  test('a mouse click (mousedown, mouseup, click) loads once', async () => {
    const ui = await boot({ presets: FILLED });
    const b = ui.el('pr0');
    b.dispatch('mousedown'); await ui.advance(80);
    b.dispatch('mouseup');   b.dispatch('click');
    await ui.flush();
    assert.equal(ui.callsTo('/loadpreset').length, 1);
  });

  test('keyboard activation (click with no pointer events) loads the preset', async () => {
    const ui = await boot({ presets: FILLED });
    ui.el('pr0').dispatch('click');
    await ui.flush();
    assert.equal(ui.callsTo('/loadpreset').length, 1);
  });

  test('a long press opens the action sheet and its release does not load', async () => {
    const ui = await boot({ presets: FILLED });
    const b = ui.el('pr0');
    b.dispatch('touchstart'); await ui.advance(650);
    assert.equal(ui.el('shtit').textContent, 'Cozy');
    assert.ok(ui.el('sheet').classList.contains('show'));
    b.dispatch('touchend'); b.dispatch('click');
    await ui.flush();
    assert.equal(ui.callsTo('/loadpreset').length, 0);
  });

  test('touchcancel aborts a pending long press', async () => {
    const ui = await boot({ presets: FILLED });
    const b = ui.el('pr0');
    b.dispatch('touchstart'); await ui.advance(100);
    b.dispatch('touchcancel'); await ui.advance(1000);
    assert.equal(ui.el('sheet').classList.contains('show'), false);
  });

  test('touchstart does not preventDefault — a swipe over the presets must scroll', async () => {
    const ui = await boot({ presets: FILLED });
    const ev = ui.el('pr3').dispatch('touchstart');
    assert.equal(ev.defaultPrevented, false);
  });
});

// ===========================================================================
// The UI ships as ONE minified <script>; an ASI hazard or a construct the
// minifier mangles only breaks there. Run a core scenario on that exact blob.
describe('shipped minified bundle', () => {
  test('loads and runs the poll + slider path after build_page.py minification', async (t) => {
    if (process.env.UI_MUTATION) { t.skip('mutations target source text, not the bundle'); return; }
    const py = `
import os, re, sys
ns = {"Import": lambda *a: None, "re": re, "os": os}
src = open(sys.argv[1], encoding="utf-8").read()
try: exec(compile(src, sys.argv[1], "exec"), ns)
except NameError: pass
root = os.path.dirname(sys.argv[1])
js = "".join(open(os.path.join(root, "ui", p), encoding="utf-8").read() for p in ns["JS_FILES"])
sys.stdout.write(ns["minify_js"](js))
`;
    let blob;
    try { blob = execFileSync('python3', ['-c', py, path.resolve(__dirname, '..', 'build_page.py')], { encoding: 'utf-8' }); }
    catch (e) { t.skip('python3 unavailable: ' + e.message); return; }
    const ui = await boot({ bundle: blob });
    await moveSlider(ui, 'sb', 30);
    await ui.advance(150);
    assert.deepEqual(ui.callsTo('/setb').map(c => c.url), ['/setb?v=30']);
    await ui.advance(5000);
    await ui.respond(ui.last('/state'), ui.state({ c: 77 }));
    assert.equal(val(ui, 'sc'), '77');
  });
});
