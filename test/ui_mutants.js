'use strict';
/**
 * Mutation check for the behavioural UI suites in test/ui/*.test.js.
 *
 * MUTANTS below re-introduces a bug that was actually fixed in these UI paths;
 * ui_mutants_hunt.json holds plausible regressions found by an adversarial
 * mutation hunt (entries marked equivalent carry a proof and are skipped).
 * Each non-equivalent mutant The behavioural suite must FAIL against every one of them; a mutant
 * that survives means a regression of that bug would ship green.
 *
 * Mutants are applied in memory by ui_harness.js (UI_MUTATION env) — source
 * files are never touched. A find-string that no longer matches the source is
 * reported as STALE, never counted as killed: after a refactor, update the
 * mutant to describe the same bug against the new code.
 *
 * Equivalent mutants are not listed. Example: dropping a pause check inside the
 * poll interval's callback — pollTid is assigned only by resumePoll(), so the
 * interval cannot exist while paused and that check was removed as dead code.
 *
 * Run with:  node test/ui_mutants.js [--only <area|id-substring>] [--suite <glob>] [--verbose]
 *            (exit 1 if any mutant survives or is stale)
 */
const { spawn } = require('node:child_process');
const os = require('node:os');
const fs = require('node:fs');
const path = require('node:path');

const ROOT = path.resolve(__dirname, '..');

const MUTANTS = [
  { id: 'slider-reads-live-value',
    why: 'debounce sent sb.value at fire time, so a poll landing mid-debounce replaced the user\'s value',
    file: 'js/sliders.js', find: "xf('/setb?v='+v)", replace: "xf('/setb?v='+sb.value)" },
  { id: 'poll-no-timeout',
    why: 'bare fetch: a stalled connection never failed, so the offline banner could never fire',
    file: 'js/state.js', find: "fetch('/state',{signal:ac.signal})", replace: "fetch('/state',{})" },
  { id: 'poll-timeout-exceeds-interval',
    why: 'timeout >= poll interval: the rejection lands after the next tick and the seq guard swallows it',
    file: 'js/state.js', find: 'ac.abort();},4000)', replace: 'ac.abort();},6000)' },
  { id: 'gate-no-lastIn-snapshot',
    why: 'only the 1 s window guarded the slider: a poll in flight during a move landed >1 s later and overwrote it',
    file: 'js/state.js', find: 'lastIn!==li||', replace: '' },
  { id: 'gate-skips-banner-reset',
    why: 'pullFails/hideOffline only ran in applyState, so a gated response never cleared the banner',
    file: 'js/state.js', find: 'clearTimeout(to);pullFails=0;hideOffline();if(seq<pullSeq', replace: 'clearTimeout(to);if(seq<pullSeq' },
  { id: 'gate-removed-entirely',
    why: 'no slider gate at all: any poll response rewrote the value under the user\'s finger',
    file: 'js/state.js', find: '||lastIn!==li||Date.now()-lastIn<1000)return;', replace: ')return;' },
  { id: 'seq-guard-removed',
    why: 'without the sequence check an older poll answering late overwrote newer state',
    file: 'js/state.js', find: 'if(seq<pullSeq||lastIn', replace: 'if(lastIn' },
  { id: 'visibility-ignores-pause',
    why: 'visibilitychange polled during OTA/Surprise and could raise a false offline banner',
    file: 'js/state.js', find: "addEventListener('visibilitychange',function(){if(!document.hidden&&!pollPaused)pull();", replace: "addEventListener('visibilitychange',function(){if(!document.hidden)pull();" },
  { id: 'resume-double-interval',
    why: 'resumePoll without clearInterval stacked intervals — two polls per tick, racing seq numbers',
    file: 'js/state.js', find: 'function resumePoll(){clearInterval(pollTid);', replace: 'function resumePoll(){' },
  { id: 'theme-via-gated-pull',
    why: 'theme tap scheduled pull(), which the slider gate could drop, instead of applying its own response',
    file: 'js/sliders.js', find: "xf('/settheme?v='+t).then(function(r){return r.json();}).then(applyState)", replace: "xf('/settheme?v='+t).then(pull)" },
  { id: 'reset-via-gated-pull',
    why: 'Reset scheduled pull(), which the slider gate could drop',
    file: 'js/sliders.js', find: "xf('/reset').then(function(r){return r.json();}).then(applyState)", replace: "xf('/reset').then(pull)" },
  { id: 'ai-resumes-during-ota',
    why: 'askAI resumed polling and re-enabled itself even when an OTA had started meanwhile',
    file: 'js/ai.js', find: 'if(!otaActive){btn.disabled=false;resumePoll();}', replace: 'btn.disabled=false;resumePoll();' },
  { id: 'ai-never-pauses',
    why: 'Surprise did not pause polling: the 25 s block raised a false offline banner',
    file: 'js/ai.js', find: '  pausePoll();\n  btn.disabled=true;', replace: '  btn.disabled=true;' },
  { id: 'ota-success-on-uptime-only',
    why: 'a lamp booted < 120 s ago answering with the OLD firmware after a failed flash read as success',
    file: 'js/ota.js', find: '||(otaCur&&d.version===otaCur)', replace: '' },
  { id: 'ota-loops-forever',
    why: 'no terminal state: old firmware answering with high uptime looped "Rebooting…" forever',
    file: 'js/ota.js', find: "if(++backOnline>=5)showOtaError(ru?'Обновление не удалось. Обновите страницу.':'Update failed. Refresh the page to try again.');", replace: '' },
  { id: 'ota-refused-update-polls',
    why: 'a 503 from /update went to reboot polling for 30 s instead of failing immediately',
    file: 'js/ota.js', find: "if(r.ok)doAfter();else showOtaError(ru?'Не удалось начать обновление. Попробуйте ещё раз.':'Could not start the update. Try again.');", replace: 'doAfter();' },
  { id: 'ota-error-no-resume',
    why: 'showOtaError left polling paused forever',
    file: 'js/ota.js', find: 'otaActive=false;resumePoll();', replace: 'otaActive=false;' },
  { id: 'lang-resets-install-label',
    why: 'language switch relabelled Install/Refresh back to "Check for Update" while the handler stayed install/reload',
    file: 'js/lang.js', find: "if(!ck.disabled){if(ck.dataset.mode==='install')", replace: "if(!ck.disabled){if(false)" },
  { id: 'lang-freezes-install-label',
    why: 'the first fix skipped labelled modes entirely, freezing them in the old language',
    file: 'js/lang.js',
    find: "if(ck.dataset.mode==='install')ck.textContent=ru?'Установить обновление ↑':'Install Update ↑';else if(ck.dataset.mode==='reload')ck.textContent=ru?'Обновить страницу':'Refresh page';else{",
    replace: "if(ck.dataset.mode){}else{" },
  { id: 'preset-double-fire',
    why: 'touchend + the synthetic click both loaded the preset',
    file: 'js/presets.js', find: 'if(skipClick)return;activate();', replace: 'activate();' },
  { id: 'preset-no-click-handler',
    why: 'only pointer events were bound: keyboard activation did nothing',
    file: 'js/presets.js', find: "b.addEventListener('click',function(){if(skipClick)return;activate();});", replace: '' },
  { id: 'preset-no-touchcancel',
    why: 'with a passive touchstart the browser may end the gesture with touchcancel; the long-press timer kept running',
    file: 'js/presets.js', find: "b.addEventListener('touchcancel',function(){if(pt){clearTimeout(pt);pt=null;}},{passive:true});", replace: '' },
  { id: 'preset-blocks-scroll',
    why: 'preventDefault on touchstart cancelled panning for the whole gesture',
    file: 'js/presets.js', find: 'function onStart(e){pt=setTimeout(', replace: 'function onStart(e){if(e.cancelable)e.preventDefault();pt=setTimeout(' },
  { id: 'keysave-neterr-generic',
    why: 'both key-save failures shared one generic "error" state, so a language switch replaced "Network error" with "Ошибка"',
    file: 'js/ai.js', find: "s.dataset.ks='neterr'", replace: "s.dataset.ks='error'" },
  { id: 'mqtt-hint-erased',
    why: 'with no stored password the field placeholder was set to \'\', wiping the "Password (optional)" hint',
    file: 'js/mqtt.js', find: ':mqpHint;', replace: ":'';" },
  { id: 'offline-banner-stays-exposed',
    why: 'the banner hides by transform, so without aria-hidden a screen reader kept reading "Lamp not responding" after recovery',
    file: 'js/ui.js', find: "b.classList.remove('show');b.setAttribute('aria-hidden','true');", replace: "b.classList.remove('show');" },
];

// Optional second list: plausible regressions found by an adversarial mutation hunt.
const HUNT_FILE = path.join(__dirname, 'ui_mutants_hunt.json');
const HUNT = (fs.existsSync(HUNT_FILE) ? require(HUNT_FILE) : []).map(m => Object.assign({ hunted: true }, m));

function sourceHas(m) {
  return fs.readFileSync(path.join(ROOT, 'ui', m.file), 'utf-8').includes(m.find);
}

let SUITE = 'test/ui/*.test.js';

function runSuite(m) {
  return new Promise(resolve => {
    const env = Object.assign({}, process.env);
    if (m) env.UI_MUTATION = JSON.stringify(m); else delete env.UI_MUTATION;
    const child = spawn(process.execPath, ['--test', SUITE], { cwd: ROOT, env });
    let out = '';
    child.stdout.on('data', d => { out += d; });
    child.stderr.on('data', d => { out += d; });
    child.on('close', () => resolve(out));
  });
}

async function main() {
  const args = process.argv.slice(2);
  const only = args.includes('--only') ? args[args.indexOf('--only') + 1] : null;
  const verbose = args.includes('--verbose');
  if (args.includes('--suite')) SUITE = args[args.indexOf('--suite') + 1];

  // Baseline: the suite must be green on unmutated code, or every "kill" below
  // would just be the pre-existing failure.
  const base = await runSuite(null);
  const baseFail = ((base.match(/^# fail (\d+)/m) || [0, '1'])[1]) | 0;
  if (baseFail > 0 || !/^# pass \d+/m.test(base)) {
    console.log(`BASELINE RED — ${SUITE} fails on unmutated code; fix that first:\n` +
      [...base.matchAll(/^\s*not ok \d+ - (.+)$/gm)].map(x => '  ' + x[1]).join('\n'));
    process.exit(2);
  }
  const all = MUTANTS.concat(HUNT).filter(m => !only || m.id.includes(only) || (m.area || '') === only);
  const equivalent = all.filter(m => m.equivalent);
  const live = all.filter(m => !m.equivalent);

  const results = new Array(live.length);
  let next = 0;
  const workers = Math.max(2, Math.min(8, os.cpus().length - 1));
  await Promise.all(Array.from({ length: workers }, async () => {
    for (;;) {
      const i = next++;
      if (i >= live.length) return;
      const m = live[i];
      if (!sourceHas(m)) { results[i] = { m, status: 'STALE', note: `find-string no longer in ui/${m.file}` }; continue; }
      const out = await runSuite(m);
      if (/UI_MUTATION #\d+/.test(out)) { results[i] = { m, status: 'STALE', note: 'harness rejected the mutation' }; continue; }
      const failed = ((out.match(/^# fail (\d+)/m) || [0, '0'])[1]) | 0;
      const names = [...out.matchAll(/^\s*not ok \d+ - (.+)$/gm)].map(x => x[1])
        .filter(n => !/\.test\.js$/.test(n) && !/^(poll lifecycle|sliders|OTA|Surprise|preset|shipped)/.test(n));
      results[i] = failed > 0 ? { m, status: 'killed', note: names.slice(0, 2).join('; ') } : { m, status: 'SURVIVED', note: m.why };
    }
  }));

  let survived = 0, stale = 0;
  for (const r of results) {
    if (r.status === 'SURVIVED') survived++;
    if (r.status === 'STALE') stale++;
    if (r.status !== 'killed' || verbose)
      console.log(`${r.status.padEnd(9)} ${r.m.id}${r.status === 'killed' ? '  (' + r.note + ')' : ' — ' + String(r.note).slice(0, 160)}`);
  }
  const hist = results.filter(r => !r.m.hunted), hunt = results.filter(r => r.m.hunted);
  const k = rs => rs.filter(r => r.status === 'killed').length;
  console.log(`\nhistorical ${k(hist)}/${hist.length} killed · hunted ${k(hunt)}/${hunt.length} killed · ` +
              `${survived} survived · ${stale} stale · ${equivalent.length} documented-equivalent skipped`);
  process.exit(survived || stale ? 1 : 0);
}
main();
