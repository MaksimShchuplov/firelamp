'use strict';
/**
 * Mutation check for test_ui_behaviour.js.
 *
 * Each mutant below re-introduces a bug that was actually fixed in these UI
 * paths. The behavioural suite must FAIL against every one of them; a mutant
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
 * Run with:  node test/ui_mutants.js        (exit 1 if any mutant survives or is stale)
 */
const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');

const ROOT = path.resolve(__dirname, '..');
const SUITE = path.join(__dirname, 'test_ui_behaviour.js');

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
];

function sourceHas(m) {
  return fs.readFileSync(path.join(ROOT, 'ui', m.file), 'utf-8').includes(m.find);
}

let survived = 0, stale = 0;
for (const m of MUTANTS) {
  if (!sourceHas(m)) {
    stale++;
    console.log(`STALE     ${m.id} — find-string no longer in ui/${m.file}; update the mutant`);
    continue;
  }
  const r = spawnSync(process.execPath, [SUITE], {
    env: Object.assign({}, process.env, { UI_MUTATION: JSON.stringify(m) }),
    encoding: 'utf-8',
  });
  // A harness/load error also exits non-zero; only assertion failures count as a kill.
  const out = r.stdout + r.stderr;
  const failed = (out.match(/^# fail (\d+)/m) || [0, '0'])[1] | 0;
  const loadErr = /UI_MUTATION #\d+/.test(out);
  if (loadErr) { stale++; console.log(`STALE     ${m.id} — harness rejected the mutation`); continue; }
  if (failed > 0) {
    const names = [...out.matchAll(/^\s*not ok \d+ - (.+)$/gm)].map(x => x[1]).filter(n => !/^(poll lifecycle|sliders|OTA|Surprise|preset|shipped)/.test(n));
    console.log(`killed    ${m.id}  (${failed} failing: ${names.slice(0, 2).join('; ')}${names.length > 2 ? '; …' : ''})`);
  } else {
    survived++;
    console.log(`SURVIVED  ${m.id} — ${m.why}`);
  }
}
console.log(`\n${MUTANTS.length - survived - stale}/${MUTANTS.length} killed, ${survived} survived, ${stale} stale`);
process.exit(survived || stale ? 1 : 0);
