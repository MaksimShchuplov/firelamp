'use strict';
/**
 * Behavioural tests for ui/js/lang.js (language switch, help modal) and
 * ui/js/mqtt.js (MQTT broker form).
 * Run with:  node --test test/ui/lang-mqtt.test.js
 */
const { test, describe } = require('node:test');
const assert = require('node:assert/strict');
const { loadUI, EMPTY_SLOTS, boot, offline, stallPoll } = require('../ui_helpers.js');

const GREEN = '#4ade80';
const RED = '#f87171';

const text = (ui, id) => ui.el(id).textContent;
const modalOpen = ui => ui.el('mod').classList.contains('show');
const pressKey = (ui, key) => ui.dom.document.dispatch('keydown', { key });
const storedLang = ui => ui.ctx.localStorage.getItem('lang');

/** Boot like ui_helpers.boot(), but answer the first /state with `over` applied. */
async function bootWith(opts, over) {
  const ui = loadUI(opts);
  await ui.flush();
  await ui.respond(ui.last('/getpresets'), EMPTY_SLOTS);
  await ui.respond(ui.last('/state'), ui.state(over));
  return ui;
}

/** Open the help modal and answer its /geminikey probe. */
async function openHelp(ui, keySet) {
  ui.el('ibtn').click();
  await ui.flush();
  await ui.respond(ui.last('/geminikey'), { set: keySet });
}

function fillMqtt(ui, f) {
  ui.el('mqip').value = f.ip;
  ui.el('mqpt').value = f.pt;
  ui.el('mqu').value = f.u;
  ui.el('mqp').value = f.p;
  ui.el('mqt').value = f.t;
}

async function saveMqtt(ui) {
  ui.el('mqsave').click();
  await ui.flush();
  return ui.last('/setmqtt');
}

// ===========================================================================
describe('help modal (lang.js)', () => {
  test('opening help moves focus to the close button and Escape closes it', async () => {
    const ui = await boot();
    ui.el('ibtn').click();
    assert.equal(modalOpen(ui), true);
    assert.equal(ui.dom.document.activeElement, ui.el('mcls'), 'focus moves into the dialog');
    pressKey(ui, 'Escape');
    assert.equal(modalOpen(ui), false, 'Escape closes the modal');
  });

  test('a stored Gemini key shows as "Key saved ✓" in green and stays so across a language switch', async () => {
    const ui = await boot();
    await openHelp(ui, true);
    assert.equal(ui.callsTo('/geminikey').length, 1, 'opening help asks the lamp for key status');
    assert.equal(text(ui, 'aikeystatus'), 'Key saved ✓');
    assert.equal(ui.el('aikeystatus').style.color, GREEN);
    ui.el('lru').click();
    assert.equal(text(ui, 'aikeystatus'), 'Ключ сохранён ✓');
    ui.el('len').click();
    assert.equal(text(ui, 'aikeystatus'), 'Key saved ✓');
  });

  test('a missing Gemini key shows as "No key set" in red and is translated on a language switch', async () => {
    const ui = await boot();
    await openHelp(ui, false);
    assert.equal(text(ui, 'aikeystatus'), 'No key set');
    assert.equal(ui.el('aikeystatus').style.color, RED);
    ui.el('lru').click();
    assert.equal(text(ui, 'aikeystatus'), 'Ключ не задан');
  });

  test('a network failure saving the key keeps its specific message across language switches', async () => {
    const ui = await boot();
    ui.el('aikey').value = 'AIzaXYZ';
    ui.el('aiksave').click();
    await ui.flush();
    await ui.fail(ui.last('/setgeminikey'));
    assert.equal(text(ui, 'aikeystatus'), 'Network error');
    ui.el('lru').click();
    assert.equal(text(ui, 'aikeystatus'), 'Ошибка сети');
    ui.el('len').click();
    assert.equal(text(ui, 'aikeystatus'), 'Network error', 'must not degrade to a generic "Error"');
  });

  test('a rejected key save keeps its specific message across language switches', async () => {
    const ui = await boot();
    ui.el('aikey').value = 'AIzaXYZ';
    ui.el('aiksave').click();
    await ui.flush();
    await ui.respond(ui.last('/setgeminikey'), { error: 'invalid_key' }, 400);
    assert.equal(text(ui, 'aikeystatus'), 'Save failed');
    ui.el('lru').click();
    assert.equal(text(ui, 'aikeystatus'), 'Ошибка сохранения');
    ui.el('len').click();
    assert.equal(text(ui, 'aikeystatus'), 'Save failed');
  });
});

// ===========================================================================
describe('language switch (lang.js)', () => {
  test('choosing Russian highlights RU, sets <html lang> and is remembered', async () => {
    const ui = await boot({ lang: 'en' });
    assert.equal(ui.dom.document.documentElement.lang, 'en');
    ui.el('lru').click();
    assert.equal(ui.el('lru').classList.contains('act'), true, 'RU highlighted');
    assert.equal(ui.el('len').classList.contains('act'), false, 'EN not highlighted');
    assert.equal(ui.dom.document.documentElement.lang, 'ru');
    assert.equal(storedLang(ui), 'ru', 'next page load comes back in Russian');
  });

  test('choosing English highlights EN, sets <html lang> and is remembered', async () => {
    const ui = await boot({ lang: 'ru' });
    assert.equal(ui.dom.document.documentElement.lang, 'ru');
    assert.equal(ui.el('lru').classList.contains('act'), true);
    ui.el('len').click();
    assert.equal(ui.el('len').classList.contains('act'), true, 'EN highlighted');
    assert.equal(ui.el('lru').classList.contains('act'), false, 'RU not highlighted');
    assert.equal(ui.dom.document.documentElement.lang, 'en');
    assert.equal(storedLang(ui), 'en', 'next page load comes back in English');
  });

  test('switching language mid-check does not reset the busy button to its idle label', async () => {
    const ui = await boot();
    ui.el('chk').click();
    assert.equal(text(ui, 'chk'), 'Checking...');
    ui.el('lru').click();
    assert.notEqual(text(ui, 'chk'), 'Проверить обновления');
    assert.notEqual(text(ui, 'chk'), 'Check for Update');
    assert.equal(ui.el('chk').disabled, true);
  });

  test('switching language while "Up to date ✓" is shown does not reset it to the idle label', async () => {
    const ui = await boot();
    ui.el('chk').click();
    await ui.respond(ui.last('/checkupdate'), { current: 'aaa', latest: 'aaa', update_available: false });
    assert.equal(text(ui, 'chk'), 'Up to date ✓');
    ui.el('lru').click();
    assert.notEqual(text(ui, 'chk'), 'Проверить обновления');
    assert.notEqual(text(ui, 'chk'), 'Check for Update');
  });

  test('the update badge is translated from Russian to English', async () => {
    const ui = await bootWith({ lang: 'ru' }, { upd: 1 });
    assert.equal(text(ui, 'vinfo'), '● Доступно обновление');
    ui.el('len').click();
    assert.equal(text(ui, 'vinfo'), '● Update available');
    ui.el('lru').click();
    assert.equal(text(ui, 'vinfo'), '● Доступно обновление');
  });

  test('a showing offline banner is translated on a language switch', async () => {
    const ui = await boot();
    for (let i = 0; i < 4; i++) await stallPoll(ui);
    assert.equal(offline(ui), true);
    assert.equal(text(ui, 'offb'), '⚠ Lamp not responding');
    ui.el('lru').click();
    assert.equal(text(ui, 'offb'), '⚠ Лампа не отвечает');
  });

  test('before the lamp has ever gone offline the off-screen banner is empty, even after a switch', async () => {
    // The hidden banner is only translated off-screen, not display:none, so it
    // stays in the accessibility tree.
    const ui = await boot();
    assert.equal(offline(ui), false);
    assert.equal(text(ui, 'offb'), '');
    ui.el('lru').click();
    assert.equal(text(ui, 'offb'), '');
  });

  test('switching language while Surprise Me is thinking keeps a busy label, in the new language next tick', async () => {
    const ui = await boot();
    ui.el('surprise').click();
    await ui.flush();
    assert.equal(text(ui, 'surprise'), '✨ Thinking 0s…');
    ui.el('lru').click();
    assert.notEqual(text(ui, 'surprise'), '✨ Удиви меня', 'must not reset to the idle label');
    await ui.advance(1000);
    assert.equal(text(ui, 'surprise'), '✨ Думаю 1s…');
  });

  test('switching language re-translates the slider value descriptions', async () => {
    const ui = await boot();
    assert.equal(text(ui, 'db'), 'Full brightness');
    ui.el('lru').click();
    assert.equal(text(ui, 'db'), 'Максимум яркости');
    ui.el('len').click();
    assert.equal(text(ui, 'db'), 'Full brightness');
  });
});

// ===========================================================================
describe('MQTT form (mqtt.js)', () => {
  const FORM = { ip: '192.168.1.10', pt: '1883', u: 'hass', p: 'p&ss+w%rd=1', t: 'home/lamp' };

  test('Save posts every field form-encoded to /setmqtt with the CSRF header', async () => {
    const ui = await boot();
    fillMqtt(ui, FORM);
    const call = await saveMqtt(ui);
    assert.ok(call, 'a /setmqtt request was sent');
    assert.equal(call.opts.method, 'POST');
    assert.equal(call.opts.headers['X-Requested-With'], 'firelamp');
    assert.equal(call.opts.headers['Content-Type'], 'application/x-www-form-urlencoded');
    const form = new URLSearchParams(call.opts.body);
    assert.equal(form.get('ip'), FORM.ip);
    assert.equal(form.get('pt'), FORM.pt);
    assert.equal(form.get('u'), FORM.u);
    assert.equal(form.get('p'), FORM.p, 'password with &, +, % and = survives encoding');
    assert.equal(form.get('t'), FORM.t);
    assert.deepEqual([...form.keys()], ['ip', 'pt', 'u', 'p', 't']);
  });

  test('Save locks the button until 1.5 s after a successful save', async () => {
    const ui = await boot();
    fillMqtt(ui, FORM);
    const call = await saveMqtt(ui);
    assert.equal(ui.el('mqsave').disabled, true, 'locked against a double tap');
    await ui.respond(call, { ok: true });
    assert.equal(text(ui, 'mqst'), 'Saved ✓');
    assert.equal(ui.el('mqst').style.color, GREEN);
    await ui.advance(1499);
    assert.equal(ui.el('mqsave').disabled, true);
    await ui.advance(1);
    assert.equal(text(ui, 'mqst'), '');
    assert.equal(ui.el('mqsave').disabled, false);
  });

  test('a rejected save shows "Error" in red and unlocks the button after 1.5 s', async () => {
    const ui = await boot();
    fillMqtt(ui, FORM);
    const call = await saveMqtt(ui);
    await ui.respond(call, { ok: false });
    assert.equal(text(ui, 'mqst'), 'Error');
    assert.equal(ui.el('mqst').style.color, RED);
    await ui.advance(1500);
    assert.equal(text(ui, 'mqst'), '');
    assert.equal(ui.el('mqsave').disabled, false);
  });

  test('a network failure shows "Network Error" and unlocks the button after 1.5 s', async () => {
    const ui = await boot();
    fillMqtt(ui, FORM);
    const call = await saveMqtt(ui);
    await ui.fail(call);
    assert.equal(text(ui, 'mqst'), 'Network Error');
    assert.equal(ui.el('mqsave').disabled, true);
    await ui.advance(1500);
    assert.equal(text(ui, 'mqst'), '');
    assert.equal(ui.el('mqsave').disabled, false);
  });

  test('stored settings populate the form shortly after load, with a saved-password hint', async () => {
    const ui = await boot();
    assert.equal(ui.callsTo('/getmqtt').length, 0);
    await ui.advance(500);
    assert.equal(ui.callsTo('/getmqtt').length, 1);
    await ui.respond(ui.last('/getmqtt'), { ip: '10.0.0.7', pt: 1884, u: 'hass', p_set: true, t: 'den/lamp' });
    assert.equal(ui.el('mqip').value, '10.0.0.7');
    assert.equal(String(ui.el('mqpt').value), '1884');
    assert.equal(ui.el('mqu').value, 'hass');
    assert.equal(ui.el('mqt').value, 'den/lamp');
    assert.equal(ui.el('mqp').placeholder, 'Saved (enter to replace, - to clear)');
  });

  test('with no stored password the password field keeps its "optional" hint', async () => {
    const ui = await boot();
    assert.equal(ui.el('mqp').placeholder, 'Password (optional)', 'precondition: hint from index.html');
    await ui.advance(500);
    await ui.respond(ui.last('/getmqtt'), { ip: '10.0.0.7', pt: 1883, u: '', p_set: false, t: 'firelamp' });
    assert.equal(ui.el('mqp').placeholder, 'Password (optional)');
  });
});

// ===========================================================================
describe('offline banner accessibility (ui.js)', () => {
  // .offb is hidden with a transform, so it stays in the accessibility tree.
  // After the lamp recovers, the stale "not responding" text must not be exposed.
  test('the banner is aria-hidden at load, exposed while offline, hidden again after recovery', async () => {
    const ui = await boot();
    const b = ui.el('offb');
    assert.equal(b.getAttribute('aria-hidden'), 'true', 'hidden at load');
    for (let i = 0; i < 4; i++) await ui.advance(5000);   // three+ stalled polls
    assert.ok(b.classList.contains('show'));
    assert.equal(b.getAttribute('aria-hidden'), 'false', 'exposed while shown');
    await ui.respond(ui.last('/state'), ui.state());
    assert.equal(b.classList.contains('show'), false);
    assert.equal(b.getAttribute('aria-hidden'), 'true', 'stale text must not stay exposed');
  });
});
