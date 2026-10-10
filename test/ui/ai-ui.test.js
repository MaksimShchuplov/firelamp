'use strict';
/**
 * Behavioural tests for ui/js/ai.js (Surprise Me + Gemini key form) and
 * ui/js/ui.js (action sheet, slider descriptions).
 * Run with:  node --test test/ui/ai-ui.test.js
 */
const { test, describe } = require('node:test');
const assert = require('node:assert/strict');
const { boot, val } = require('../ui_helpers.js');

const AMBER = '#fbbf24';
const RED = '#ef4444';

const surpriseBtn = ui => ui.el('surprise');
const aiName = ui => ui.el('ainame');
const effect = (ui, name, over) => Object.assign(ui.state(over), { name });

async function surprise(ui) {
  surpriseBtn(ui).click();
  await ui.flush();
  return ui.last('/surprise');
}

/**
 * Answer `call` with headers now and the body later — as a browser does when
 * the response body is still streaming. Aborting the request's signal while the
 * body is unread rejects json() with AbortError, exactly like fetch().
 */
async function respondHeadersOnly(ui, call, status) {
  let finish;
  const body = new Promise((resolve, reject) => {
    finish = resolve;
    const sig = call.opts.signal;
    if (sig) sig.addEventListener('abort',
      () => reject(new DOMException('The operation was aborted.', 'AbortError')), { once: true });
  });
  call.resolve({ status, ok: status >= 200 && status < 300, json: () => body });
  await ui.flush();
  return async payload => { finish(payload); await ui.flush(); };
}

async function saveKey(ui, raw) {
  ui.el('aikey').value = raw;
  ui.el('aiksave').click();
  await ui.flush();
  return ui.last('/setgeminikey');
}

// The harness DOM stub keeps `children` when innerHTML is assigned; a browser
// drops them. Model that for the sheet's button row so stale buttons show up.
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
describe('Surprise Me button (ai.js)', () => {
  test('the button is disabled the moment a request starts, so a double tap cannot send a second one', async () => {
    const ui = await boot();
    await surprise(ui);
    assert.equal(surpriseBtn(ui).disabled, true);
    assert.equal(surpriseBtn(ui).textContent, '✨ Thinking 0s…');
  });

  test('the elapsed counter stops once an effect arrives — the label stays "Surprise Me"', async () => {
    const ui = await boot();
    const req = await surprise(ui);
    await ui.advance(2000);
    assert.equal(surpriseBtn(ui).textContent, '✨ Thinking 2s…');
    await ui.respond(req, effect(ui, 'Aurora'));
    assert.equal(surpriseBtn(ui).textContent, '✨ Surprise Me');
    await ui.advance(5000);
    assert.equal(surpriseBtn(ui).textContent, '✨ Surprise Me', 'no counter ticking after success');
  });

  test('the elapsed counter stops after an error — the label stays "Surprise Me"', async () => {
    const ui = await boot();
    const req = await surprise(ui);
    await ui.advance(2000);
    await ui.respond(req, { error: 'http_error' }, 502);
    assert.equal(surpriseBtn(ui).textContent, '✨ Surprise Me');
    await ui.advance(5000);
    assert.equal(surpriseBtn(ui).textContent, '✨ Surprise Me', 'no counter ticking after an error');
  });

  test('an answer arriving at 27 s (inside the lamp\'s 25 s budget plus slack) is still applied', async () => {
    const ui = await boot();
    const req = await surprise(ui);
    await ui.advance(27000);
    await ui.respond(req, effect(ui, 'Nebula', { b: 61 }));
    assert.equal(aiName(ui).textContent, 'Nebula ✨');
    assert.equal(val(ui, 'sb'), '61');
  });

  test('no answer within 30 s shows "AI timed out" and re-enables the button', async () => {
    const ui = await boot();
    const req = await surprise(ui);
    await ui.advance(29900);
    assert.equal(req.aborted, false, 'still waiting just before 30 s');
    await ui.advance(200);
    assert.equal(req.aborted, true, 'request aborted at 30 s');
    assert.equal(aiName(ui).textContent, '⚠ AI timed out');
    assert.equal(aiName(ui).style.color, RED);
    assert.equal(surpriseBtn(ui).disabled, false);
  });

  test('a response whose headers arrived in time is not reported as a timeout when its body finishes after 30 s', async () => {
    const ui = await boot();
    const req = await surprise(ui);
    await ui.advance(5000);
    const finishBody = await respondHeadersOnly(ui, req, 200);
    await ui.advance(26000);                              // past the 30 s mark
    await finishBody(effect(ui, 'Slowpoke', { b: 33 }));
    assert.equal(aiName(ui).textContent, 'Slowpoke ✨');
    assert.equal(val(ui, 'sb'), '33');
  });
});

// ===========================================================================
describe('Surprise Me error messages (ai.js)', () => {
  const cases = [
    ['no_key', 400, '⚠ Set API key in settings (?)'],
    ['auth_error', 401, '⚠ Invalid API key'],
    ['parse_failed', 502, '⚠ AI response error'],
    ['rate_limit', 429, '⚠ Rate limit — wait a moment'],
  ];
  for (const [code, status, msg] of cases) {
    test(`firmware error "${code}" (HTTP ${status}) shows "${msg}" in red`, async () => {
      const ui = await boot();
      const req = await surprise(ui);
      await ui.respond(req, { error: code }, status);
      assert.equal(aiName(ui).textContent, msg);
      assert.equal(aiName(ui).style.color, RED);
    });
  }

  test('the error message fades after 4 s', async () => {
    const ui = await boot();
    await ui.respond(await surprise(ui), { error: 'auth_error' }, 401);
    await ui.advance(3900);
    assert.equal(aiName(ui).textContent, '⚠ Invalid API key');
    await ui.advance(200);
    assert.equal(aiName(ui).textContent, '');
  });
});

// ===========================================================================
describe('Surprise Me retry after an error (ai.js)', () => {
  test('a successful retry within 4 s of an error keeps its effect name, shown in amber', async () => {
    const ui = await boot();
    await ui.respond(await surprise(ui), { error: 'http_error' }, 502);
    assert.equal(aiName(ui).style.color, RED);
    await ui.advance(1000);
    await ui.respond(await surprise(ui), effect(ui, 'Comet'));
    assert.equal(aiName(ui).textContent, 'Comet ✨');
    assert.equal(aiName(ui).style.color, AMBER, 'effect name must not inherit the error red');
    await ui.advance(10000);
    assert.equal(aiName(ui).textContent, 'Comet ✨', 'the old error fade must not wipe the new name');
  });

  test('a second error within 4 s of the first stays visible for its own full 4 s', async () => {
    const ui = await boot();
    await ui.respond(await surprise(ui), { error: 'http_error' }, 502);
    await ui.advance(2000);
    await ui.respond(await surprise(ui), { error: 'auth_error' }, 401);
    await ui.advance(3000);                               // 5 s after the first error
    assert.equal(aiName(ui).textContent, '⚠ Invalid API key');
    assert.equal(aiName(ui).style.color, RED);
    await ui.advance(1100);                               // 4 s after the second
    assert.equal(aiName(ui).textContent, '');
  });
});

// ===========================================================================
describe('Gemini key form (ai.js)', () => {
  test('Save with an empty or whitespace-only field does nothing', async () => {
    const ui = await boot();
    await saveKey(ui, '   ');
    assert.equal(ui.callsTo('/setgeminikey').length, 0);
    assert.equal(ui.el('aiksave').disabled, false);
    assert.equal(ui.el('aikeystatus').textContent, '');
  });

  test('the key is POSTed trimmed, form-encoded, with the CSRF header', async () => {
    const ui = await boot();
    const req = await saveKey(ui, '  AIzaKey_1-x \n');
    assert.equal(req.opts.method, 'POST');
    assert.equal(req.opts.headers['X-Requested-With'], 'firelamp');
    assert.equal(req.opts.headers['Content-Type'], 'application/x-www-form-urlencoded');
    assert.equal(req.opts.body, 'key=AIzaKey_1-x');
    assert.equal(ui.el('aiksave').disabled, true, 'disabled while saving');
  });

  test('reserved characters in the key are percent-encoded on the wire', async () => {
    const ui = await boot();
    const req = await saveKey(ui, 'a+b&c%d=e');
    assert.equal(req.opts.body, 'key=a%2Bb%26c%25d%3De');
    assert.equal(new URLSearchParams(req.opts.body).get('key'), 'a+b&c%d=e');
  });

  test('a successful save clears the secret from the field and shows "Key saved ✓"', async () => {
    const ui = await boot();
    await ui.respond(await saveKey(ui, 'AIzaSecret'), { ok: true });
    assert.equal(ui.el('aikey').value, '');
    assert.equal(ui.el('aikeystatus').textContent, 'Key saved ✓');
    assert.equal(ui.el('aiksave').textContent, '✓');
    await ui.advance(1500);
    assert.equal(ui.el('aiksave').textContent, 'Save key');
    assert.equal(ui.el('aiksave').disabled, false);
  });

  test('"Key saved ✓" is translated when the language switches to Russian', async () => {
    const ui = await boot();
    await ui.respond(await saveKey(ui, 'AIzaSecret'), { ok: true });
    ui.el('lru').click();
    assert.equal(ui.el('aikeystatus').textContent, 'Ключ сохранён ✓');
  });

  test('after a network failure the Save button comes back enabled after 1.5 s', async () => {
    const ui = await boot();
    await ui.fail(await saveKey(ui, 'AIzaSecret'));
    assert.equal(ui.el('aikeystatus').textContent, 'Network error');
    assert.equal(ui.el('aiksave').textContent, '✗');
    assert.equal(ui.el('aiksave').disabled, true);
    await ui.advance(1500);
    assert.equal(ui.el('aiksave').textContent, 'Save key');
    assert.equal(ui.el('aiksave').disabled, false);
    assert.equal(ui.el('aikey').value, 'AIzaSecret', 'the unsaved key is kept for a retry');
  });
});

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

// ===========================================================================
describe('Surprise Me busy counter stops (ai.js)', () => {
  // The "Thinking Ns…" ticker is a 1 s interval; every exit path must clear it,
  // or an idle, enabled button keeps counting up forever.
  for (const [name, settle] of [
    ['after a server error', ui => ui.respond(ui.last('/surprise'), { error: 'http_error' }, 502)],
    ['after a network failure', ui => ui.fail(ui.last('/surprise'))],
    ['after success', ui => ui.respond(ui.last('/surprise'), Object.assign(ui.state(), { name: 'Ash' }))],
  ]) {
    test(`the "Thinking" counter stops ${name}`, async () => {
      const ui = await boot();
      ui.el('surprise').click();
      await ui.advance(2000);
      assert.match(ui.el('surprise').textContent, /Thinking 2s/);
      await settle(ui);
      assert.equal(ui.el('surprise').textContent, '✨ Surprise Me');
      await ui.advance(5000);
      assert.equal(ui.el('surprise').textContent, '✨ Surprise Me', 'counter must not resume ticking');
    });
  }
});
