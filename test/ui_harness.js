'use strict';
/**
 * Behavioural test harness for ui/js/*.js.
 *
 * Loads the REAL browser scripts — in build_page.py's JS_FILES order, the order
 * they ship in — into a vm context with a stubbed DOM, a virtual clock and a
 * hand-driven fetch(). Tests therefore exercise shipped code, not mirrors of it.
 *
 *   const ui = loadUI();
 *   const call = ui.fetches.find(c => c.url === '/state');
 *   await ui.respond(call, {b: 50});       // resolve that request
 *   await ui.advance(5000);                // run timers due in the next 5 s
 *
 * Mutation testing: set UI_MUTATION to a JSON object (or array of them)
 * {"file":"js/state.js","find":"...","replace":"..."}. The harness throws if a
 * `find` string is absent, so a typo can never masquerade as a "killed" mutant.
 */
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const ROOT = path.resolve(__dirname, '..');
const UI = path.join(ROOT, 'ui');

// Single source of truth for load order: the list build_page.py ships.
function jsFiles() {
  const src = fs.readFileSync(path.join(ROOT, 'build_page.py'), 'utf-8');
  const m = src.match(/JS_FILES\s*=\s*\[([\s\S]*?)\]/);
  if (!m) throw new Error('JS_FILES not found in build_page.py');
  return [...m[1].matchAll(/"([^"]+)"/g)].map(x => x[1]);
}

function mutationsFromEnv() {
  if (!process.env.UI_MUTATION) return [];
  const v = JSON.parse(process.env.UI_MUTATION);
  return Array.isArray(v) ? v : [v];
}

// Initial input values, as the browser would see them from index.html.
function htmlInputValues() {
  const html = fs.readFileSync(path.join(UI, 'index.html'), 'utf-8');
  const out = {};
  for (const m of html.matchAll(/<input[^>]*\bid=(\w+)[^>]*>/g)) {
    const v = m[0].match(/\bvalue=([^\s>]+)/);
    if (v) out[m[1]] = v[1];
  }
  return out;
}

// --------------------------------------------------------------------------
// Virtual clock — setTimeout/setInterval/Date.now are driven by advance().
// --------------------------------------------------------------------------
function makeClock(start) {
  const timers = new Map();
  let nextId = 1;
  const clock = {
    now: start,
    timers,
    setTimeout(fn, ms) { const id = nextId++; timers.set(id, { fn, due: clock.now + (ms | 0), every: 0 }); return id; },
    setInterval(fn, ms) { const id = nextId++; timers.set(id, { fn, due: clock.now + (ms | 0), every: Math.max(1, ms | 0) }); return id; },
    clearTimeout(id) { timers.delete(id); },
    clearInterval(id) { timers.delete(id); },
    intervals() { return [...timers.values()].filter(t => t.every).length; },
  };
  return clock;
}

// Real-time yield: lets every queued microtask (promise chains, fetch .then)
// run before the virtual clock moves on.
async function flush() { for (let i = 0; i < 8; i++) await new Promise(r => setImmediate(r)); }

// --------------------------------------------------------------------------
// DOM stub — auto-vivifying elements, enough of the API the scripts touch.
// --------------------------------------------------------------------------
function makeDom(initialValues) {
  const listeners = new Map();          // document-level
  const els = new Map();

  function classList() {
    const s = new Set();
    return {
      add: (...c) => c.forEach(x => s.add(x)),
      remove: (...c) => c.forEach(x => s.delete(x)),
      contains: c => s.has(c),
      toggle(c, force) {
        const on = force === undefined ? !s.has(c) : !!force;
        on ? s.add(c) : s.delete(c);
        return on;
      },
      get size() { return s.size; },
    };
  }

  function makeEl(id, tag) {
    const ls = new Map();
    const el = {
      id: id || '', tagName: (tag || 'div').toUpperCase(),
      value: id && initialValues[id] !== undefined ? String(initialValues[id]) : '',
      textContent: '', innerHTML: '', placeholder: '', disabled: false, hidden: false,
      files: [], dataset: {}, onclick: null, children: [],
      style: { cssText: '', setProperty(k, v) { this[k] = String(v); } },
      classList: classList(),
      addEventListener(type, fn) { if (!ls.has(type)) ls.set(type, []); ls.get(type).push(fn); },
      removeEventListener(type, fn) { const a = ls.get(type) || []; const i = a.indexOf(fn); if (i >= 0) a.splice(i, 1); },
      // Dispatch a synthetic event: listeners first, then the on<type> property.
      dispatch(type, init) {
        const ev = Object.assign({ type, target: el, cancelable: true, defaultPrevented: false,
          preventDefault() { this.defaultPrevented = true; } }, init || {});
        for (const fn of (ls.get(type) || []).slice()) fn.call(el, ev);
        const h = el['on' + type];
        if (typeof h === 'function') h.call(el, ev);
        return ev;
      },
      listenerCount(type) { return (ls.get(type) || []).length; },
      click() { return el.dispatch('click'); },
      focus() { dom.document.activeElement = el; },
      blur() { if (dom.document.activeElement === el) dom.document.activeElement = dom.document.body; },
      select() {},
      appendChild(c) { el.children.push(c); return c; },
      insertAdjacentElement(_, c) { return c; },
    };
    return el;
  }

  const dom = {
    els,
    document: {
      hidden: false,
      activeElement: null,
      body: null,
      documentElement: null,
      getElementById(id) { if (!els.has(id)) els.set(id, makeEl(id)); return els.get(id); },
      createElement(tag) { return makeEl('', tag); },
      addEventListener(type, fn) { if (!listeners.has(type)) listeners.set(type, []); listeners.get(type).push(fn); },
      dispatch(type, init) {
        const ev = Object.assign({ type, preventDefault() {} }, init || {});
        for (const fn of (listeners.get(type) || []).slice()) fn(ev);
      },
    },
  };
  dom.document.body = makeEl('body', 'body');
  dom.document.documentElement = makeEl('html', 'html');
  dom.document.activeElement = dom.document.body;
  return dom;
}

// --------------------------------------------------------------------------
// fetch stub — every call stays pending until the test settles it.
// --------------------------------------------------------------------------
function makeFetch() {
  const calls = [];
  function fetch(url, opts) {
    let resolve, reject;
    const p = new Promise((res, rej) => { resolve = res; reject = rej; });
    const call = { url: String(url), opts: opts || {}, settled: false, aborted: false,
      resolve: v => { if (!call.settled) { call.settled = true; resolve(v); } },
      reject: e => { if (!call.settled) { call.settled = true; reject(e); } } };
    const sig = opts && opts.signal;
    if (sig) {
      const onAbort = () => { call.aborted = true; call.reject(new DOMException('The operation was aborted.', 'AbortError')); };
      if (sig.aborted) onAbort(); else sig.addEventListener('abort', onAbort, { once: true });
    }
    calls.push(call);
    return p;
  }
  return { fetch, calls };
}

// --------------------------------------------------------------------------
// loadUI — the entry point tests use.
// --------------------------------------------------------------------------
function loadUI(options) {
  const opt = Object.assign({ lang: 'en', start: 1_000_000, mutations: [] }, options);
  const mutations = opt.mutations.concat(mutationsFromEnv());

  const clock = makeClock(opt.start);
  const dom = makeDom(htmlInputValues());
  const net = makeFetch();
  const store = new Map(opt.lang ? [['lang', opt.lang]] : []);
  const reloads = [];
  const vibrations = [];
  const alerts = [];

  class FakeDate extends Date {
    constructor(...a) { if (a.length === 0) super(clock.now); else super(...a); }
    static now() { return clock.now; }
  }

  const sandbox = {
    console,
    document: dom.document,
    navigator: { language: opt.lang === 'ru' ? 'ru-RU' : 'en-US', vibrate: ms => { vibrations.push(ms); return true; } },
    localStorage: {
      getItem: k => (store.has(k) ? store.get(k) : null),
      setItem: (k, v) => store.set(k, String(v)),
      removeItem: k => store.delete(k),
    },
    location: { reload: () => reloads.push(clock.now), href: '/' },
    alert: m => alerts.push(m),
    URL: { createObjectURL: () => 'blob:stub', revokeObjectURL() {} },
    Blob: class { constructor(parts) { this.parts = parts; } },
    fetch: net.fetch,
    AbortController, DOMException, Response, Promise, Date: FakeDate,
    setTimeout: clock.setTimeout, clearTimeout: clock.clearTimeout,
    setInterval: clock.setInterval, clearInterval: clock.clearInterval,
  };
  sandbox.window = sandbox;
  const ctx = vm.createContext(sandbox);

  // opt.bundle: run one pre-built script (the minified blob that ships) instead
  // of the individual sources. Mutations target source text, so they do not
  // apply to a bundle.
  if (opt.bundle !== undefined) {
    vm.runInContext(opt.bundle, ctx, { filename: 'bundle.min.js' });
    return makeApi();
  }

  const applied = new Set();
  for (const rel of jsFiles()) {
    let src = fs.readFileSync(path.join(UI, rel), 'utf-8');
    mutations.forEach((m, i) => {
      if (m.file !== rel) return;
      if (!src.includes(m.find)) throw new Error(`UI_MUTATION #${i}: find-string not present in ${rel}: ${JSON.stringify(m.find)}`);
      src = src.split(m.find).join(m.replace);
      applied.add(i);
    });
    vm.runInContext(src, ctx, { filename: rel });
  }
  mutations.forEach((m, i) => {
    if (!applied.has(i)) throw new Error(`UI_MUTATION #${i}: file ${m.file} is not in JS_FILES`);
  });
  return makeApi();

  function makeApi() {
  const ui = {
    ctx,
    clock,
    dom,
    el: id => dom.document.getElementById(id),
    fetches: net.calls,
    reloads, vibrations, alerts,
    /** Calls to `prefix` (exact path match before '?'), oldest first. */
    callsTo(prefix) { return net.calls.filter(c => c.url.split('?')[0] === prefix); },
    pending(prefix) { return ui.callsTo(prefix).filter(c => !c.settled); },
    last(prefix) { const a = ui.callsTo(prefix); return a[a.length - 1]; },
    async respond(call, body, status) {
      if (!call) throw new Error('respond(): no such call');
      const st = status || 200;
      call.resolve(new Response(typeof body === 'string' ? body : JSON.stringify(body),
        { status: st, headers: { 'Content-Type': 'application/json' } }));
      await flush();
    },
    async fail(call) {
      if (!call) throw new Error('fail(): no such call');
      call.reject(new TypeError('Failed to fetch'));
      await flush();
    },
    /** Advance the virtual clock, firing due timers in order and draining promise chains between them. */
    async advance(ms) {
      const target = clock.now + ms;
      for (;;) {
        let id = null, t = null;
        for (const [k, v] of clock.timers) if (v.due <= target && (!t || v.due < t.due)) { id = k; t = v; }
        if (!t) break;
        clock.now = Math.max(clock.now, t.due);
        if (t.every) t.due += t.every; else clock.timers.delete(id);
        t.fn();
        await flush();
      }
      clock.now = target;
      await flush();
    },
    flush,
    setHidden(h) { dom.document.hidden = h; dom.document.dispatch('visibilitychange'); return flush(); },
    /** Full current state object as the lamp would return it. */
    state(over) { return Object.assign({ b: 100, c: 50, co: 46, sp: 26, w: 12.3, bl: 50, th: 0, upd: 0 }, over || {}); },
  };
  return ui;
  }
}

module.exports = { loadUI, jsFiles, flush };
