'use strict';
/**
 * Behavioural tests for ui/js/state.js and ui/js/poll.js: how lamp state is
 * rendered (sliders, descriptions, theme colours, wattage, update badge) and
 * how the /state poll interacts with pauses, focus, the offline banner and
 * the slider gate. Complements the poll-lifecycle block in core.test.js.
 * Run with:  node --test test/ui/state.test.js
 */
const { test, describe } = require('node:test');
const assert = require('node:assert/strict');
const { boot, val, offline, moveSlider, stallPoll } = require('../ui_helpers.js');

const bodyOff = ui => ui.ctx.document.body.classList.contains('off');
const rootVar = (ui, name) => ui.ctx.document.documentElement.style[name];
const activeTheme = ui => [0, 1, 2, 3].filter(i => ui.el('tb' + i).classList.contains('act'));

/** Advance to the next interval poll and answer it with the given state. */
async function nextPoll(ui, over) {
  const before = ui.callsTo('/state').length;
  for (let i = 0; i < 10 && ui.callsTo('/state').length === before; i++) await ui.advance(1000);
  assert.ok(ui.callsTo('/state').length > before, 'expected a /state poll');
  await ui.respond(ui.last('/state'), ui.state(over));
}

/** Two consecutive timed-out polls: one more failure would raise the banner. */
async function twoFailures(ui) {
  await stallPoll(ui); await stallPoll(ui);            // polls at 5 s and 10 s
  await ui.advance(4000);                               // second one aborts
  assert.equal(offline(ui), false, 'precondition: banner not yet shown');
}

// ===========================================================================
describe('pausing the poll (state.js)', () => {
  test('a poll in flight when Surprise Me starts does not count as a failure when it times out', async () => {
    const ui = await boot();
    await twoFailures(ui);
    await ui.advance(1000);                               // third poll is now in flight
    const inflight = ui.last('/state');
    assert.equal(inflight.settled, false, 'precondition: a poll is in flight');
    ui.el('surprise').click();
    await ui.advance(5000);                               // in-flight poll aborts during Surprise
    assert.equal(inflight.aborted, true, 'precondition: the poll timed out');
    assert.equal(offline(ui), false, 'a superseded poll must not raise "Lamp not responding"');
  });

  test('a poll in flight when Surprise Me starts is not applied when it answers', async () => {
    const ui = await boot();
    await ui.advance(5000);
    const inflight = ui.last('/state');
    ui.el('surprise').click();
    await ui.respond(inflight, ui.state({ b: 7, c: 90 }));
    assert.equal(val(ui, 'sb'), '100');
    assert.equal(val(ui, 'sc'), '50');
  });

  test('after Surprise Me finishes, returning to the tab refreshes immediately', async () => {
    const ui = await boot();
    ui.el('surprise').click();
    await ui.respond(ui.last('/surprise'), Object.assign(ui.state({ b: 61 }), { name: 'Ember' }));
    await ui.setHidden(true);
    const polls = ui.callsTo('/state').length;
    await ui.setHidden(false);
    assert.equal(ui.callsTo('/state').length, polls + 1, 'visible again → immediate /state');
  });
});

// ===========================================================================
describe('offline banner and direct-apply responses (state.js)', () => {
  test('a theme tap that succeeds clears the banner at once and resets the failure count', async () => {
    const ui = await boot();
    for (let i = 0; i < 4; i++) await stallPoll(ui);
    assert.equal(offline(ui), true, 'precondition: banner shown');
    ui.el('tb1').click();
    await ui.respond(ui.last('/settheme'), ui.state({ th: 1 }));
    assert.equal(offline(ui), false, 'lamp answered — banner must go');
    await stallPoll(ui);                                  // one later hiccup…
    await ui.advance(4000);
    assert.equal(offline(ui), false, '…must not bring the banner straight back');
  });
});

// ===========================================================================
describe('rendering lamp state (state.js)', () => {
  test('a poll reporting 0 W shows 0.0 instead of keeping the last wattage', async () => {
    const ui = await boot();
    assert.equal(ui.el('vw').textContent, '12.3');
    await nextPoll(ui, { b: 0, w: 0 });
    assert.equal(ui.el('vw').textContent, '0.0');
  });

  test('switching back to the Fire theme (th 0) highlights Fire and restores its colours', async () => {
    const ui = await boot();
    await nextPoll(ui, { th: 2 });
    assert.deepEqual(activeTheme(ui), [2]);
    assert.equal(rootVar(ui, '--ar'), '160');
    await nextPoll(ui, { th: 0 });
    assert.deepEqual(activeTheme(ui), [0]);
    assert.equal(rootVar(ui, '--ar'), '255');
    assert.equal(rootVar(ui, '--tc1'), '#2a0d04');
  });

  test('every ambient channel of the Ice theme gets its own value', async () => {
    const ui = await boot();
    ui.el('tb3').click();
    await ui.respond(ui.last('/settheme'), ui.state({ th: 3 }));
    const got = ['--ar', '--ag', '--ab', '--ar2', '--ag2', '--ab2'].map(k => rootVar(ui, k));
    assert.deepEqual(got, ['20', '100', '230', '8', '50', '170']);
    assert.deepEqual(['--th2', '--th3', '--th4'].map(k => rootVar(ui, k)), ['#80c8ff', '#1080d0', '#043060']);
  });

  test('the page is styled "off" exactly while brightness is 0', async () => {
    const ui = await boot();                              // boots at b=100
    assert.equal(bodyOff(ui), false, 'lit at load');
    await nextPoll(ui, { b: 100 });
    assert.equal(bodyOff(ui), false, 'a repeated poll must not flip it');
    await moveSlider(ui, 'sb', 0);
    assert.equal(bodyOff(ui), true);
    await moveSlider(ui, 'sb', 0);
    assert.equal(bodyOff(ui), true, 'still off on a repeated 0');
    await moveSlider(ui, 'sb', 40);
    assert.equal(bodyOff(ui), false);
    await moveSlider(ui, 'sb', 60);
    assert.equal(bodyOff(ui), false, 'dragging while lit stays lit');
  });

  test('slider descriptions follow slider moves and polled values', async () => {
    const ui = await boot();
    await moveSlider(ui, 'sb', 0);
    assert.equal(ui.el('db').textContent, 'Off');
    await moveSlider(ui, 'sb', 10);
    assert.equal(ui.el('db').textContent, 'Very dim');
    await ui.advance(2000);                               // leave the slider gate window
    await nextPoll(ui, { b: 10, c: 90, co: 140, sp: 200, bl: 5 });
    assert.equal(ui.el('dc').textContent, 'Deep reds only');
    assert.equal(ui.el('dco').textContent, 'Quick embers');
    assert.equal(ui.el('dsp').textContent, 'Hot inferno');
    assert.equal(ui.el('dbl').textContent, 'Frozen glow');
  });
});

// ===========================================================================
describe('focused sliders (state.js)', () => {
  test('a poll landing >1 s after a keyboard nudge leaves the focused contrast slider alone', async () => {
    const ui = await boot();
    ui.el('sc').focus();
    await moveSlider(ui, 'sc', 70);                       // arrow key on the focused slider
    await ui.advance(1500);                               // pause > 1 s
    await nextPoll(ui, { c: 50, b: 30 });                 // lamp still reports the old contrast
    assert.equal(val(ui, 'sc'), '70', 'focused contrast must not jump back');
    assert.equal(val(ui, 'sb'), '30', 'unfocused brightness still follows the lamp');
  });

  test('a poll landing >1 s after a keyboard nudge leaves the focused brightness slider alone', async () => {
    const ui = await boot();
    ui.el('sb').focus();
    await moveSlider(ui, 'sb', 70);
    await ui.advance(1500);
    await nextPoll(ui, { b: 100, c: 80 });
    assert.equal(val(ui, 'sb'), '70', 'focused brightness must not jump back');
    assert.equal(val(ui, 'sc'), '80', 'unfocused contrast still follows the lamp');
  });
});

// ===========================================================================
describe('slider gate window (state.js)', () => {
  test('a poll sent 2 s after a slider move applies changes made elsewhere', async () => {
    const ui = await boot();
    await ui.advance(3000);
    await moveSlider(ui, 'sb', 30);
    await ui.advance(2000);                               // interval poll fires 2 s after the move
    const p = ui.last('/state');
    assert.equal(ui.callsTo('/state').length, 2, 'precondition: the interval poll was sent');
    await ui.respond(p, ui.state({ b: 60 }));             // another phone set 60
    assert.equal(val(ui, 'sb'), '60');
  });
});

// ===========================================================================
describe('update badge (state.js)', () => {
  test('no badge when the lamp reports no pending update', async () => {
    const ui = await boot();                              // boot state has upd: 0
    await nextPoll(ui, { upd: 0 });
    assert.equal(ui.el('vinfo').textContent, '');
  });

  test('a pending update shows an amber "Update available" badge', async () => {
    const ui = await boot();
    await nextPoll(ui, { upd: 1 });
    assert.equal(ui.el('vinfo').textContent, '● Update available');
    assert.equal(ui.el('vinfo').style.color, '#fbbf24');
  });

  test('the badge does not replace the version line from a manual check', async () => {
    const ui = await boot();
    ui.el('chk').click();
    await ui.respond(ui.last('/checkupdate'), { current: 'aaa', latest: 'bbb', update_available: true });
    assert.equal(ui.el('chk').disabled, false, 'precondition: check finished');
    await nextPoll(ui, { upd: 1 });
    assert.equal(ui.el('vinfo').textContent, 'Current: aaa → GitHub: bbb');
  });

  test('a poll during a check adds no badge, so the version line is not left amber', async () => {
    const ui = await boot();
    ui.el('chk').click();                                 // check in flight, button disabled
    await nextPoll(ui, { upd: 1 });
    assert.equal(ui.el('vinfo').textContent, '', 'no badge while checking');
    await ui.respond(ui.last('/checkupdate'), { current: 'aaa', latest: 'bbb', update_available: true });
    assert.equal(ui.el('vinfo').textContent, 'Current: aaa → GitHub: bbb');
    assert.notEqual(ui.el('vinfo').style.color, '#fbbf24', 'version line in normal colour');
  });
});
