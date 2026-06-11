'use strict';
/**
 * Tests for the DD descriptor table (ui/js/globals.js) and the
 * dynDesc bucket-lookup logic (ui/js/ui.js).
 *
 * Pure JS — no DOM required.
 * Run with:  node test/test_ui.js
 */
const { test, describe } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const ROOT = path.resolve(__dirname, '..');

// ---------------------------------------------------------------------------
// Extract the DD table from globals.js without executing any DOM calls.
// The table occupies the first block ending at the standalone "};" line.
// ---------------------------------------------------------------------------
function loadDD() {
  const src = fs.readFileSync(path.join(ROOT, 'ui/js/globals.js'), 'utf-8');
  const lines = src.split('\n');
  const ddLines = [];
  let capturing = false;
  for (const line of lines) {
    if (line.startsWith('var DD={')) capturing = true;
    if (capturing) {
      ddLines.push(line);
      if (line === '};') break;
    }
  }
  if (!ddLines.length) throw new Error('DD table not found in globals.js');
  // eslint-disable-next-line no-new-func
  return new Function(ddLines.join('\n') + '\nreturn DD;')();
}

const DD = loadDD();

// Expected slider id → [min, max]
const SLIDER_RANGES = {
  sb:  [0,   100],
  sc:  [0,   100],
  sco: [20,  150],
  ssp: [0,   255],
  sbl: [0,   255],
};

// ---------------------------------------------------------------------------
// Pure dynDesc bucket lookup — mirrors ui.js logic without the DOM write.
// Returns undefined for unknown sid, first-bucket label when out of range.
// ---------------------------------------------------------------------------
function lookup(sid, val, lang) {
  const r = DD[sid];
  if (!r) return undefined;
  const a = r[lang === 'ru' ? 'ru' : 'en'];
  let t = a[0][2];
  for (let i = 0; i < a.length; i++) {
    if (val >= a[i][0] && val <= a[i][1]) { t = a[i][2]; break; }
  }
  return t;
}

// ===========================================================================
// DD table structure
// ===========================================================================

describe('DD table structure', () => {
  test('all expected sliders are present', () => {
    for (const sid of Object.keys(SLIDER_RANGES)) {
      assert.ok(sid in DD, `DD missing entry for "${sid}"`);
    }
  });

  test('each slider entry has en and ru arrays', () => {
    for (const sid of Object.keys(SLIDER_RANGES)) {
      const e = DD[sid];
      assert.ok(Array.isArray(e.en), `${sid}.en should be an array`);
      assert.ok(Array.isArray(e.ru), `${sid}.ru should be an array`);
    }
  });

  test('en and ru have the same bucket count', () => {
    for (const sid of Object.keys(SLIDER_RANGES)) {
      const { en, ru } = DD[sid];
      assert.equal(en.length, ru.length,
        `${sid}: en has ${en.length} buckets, ru has ${ru.length}`);
    }
  });

  test('first bucket starts at the slider minimum', () => {
    for (const [sid, [lo]] of Object.entries(SLIDER_RANGES)) {
      assert.equal(DD[sid].en[0][0], lo,
        `${sid}: first bucket lo should be ${lo}`);
    }
  });

  test('last bucket ends at the slider maximum', () => {
    for (const [sid, [, hi]] of Object.entries(SLIDER_RANGES)) {
      const buckets = DD[sid].en;
      assert.equal(buckets[buckets.length - 1][1], hi,
        `${sid}: last bucket hi should be ${hi}`);
    }
  });

  test('consecutive buckets are contiguous — no gaps', () => {
    for (const sid of Object.keys(SLIDER_RANGES)) {
      const buckets = DD[sid].en;
      for (let i = 1; i < buckets.length; i++) {
        assert.equal(
          buckets[i][0], buckets[i - 1][1] + 1,
          `${sid}: gap between bucket ${i - 1} (hi=${buckets[i-1][1]}) ` +
          `and bucket ${i} (lo=${buckets[i][0]})`
        );
      }
    }
  });

  test('no bucket has an inverted range', () => {
    for (const sid of Object.keys(SLIDER_RANGES)) {
      for (const lang of ['en', 'ru']) {
        DD[sid][lang].forEach((b, i) => {
          assert.ok(b[0] <= b[1],
            `${sid}.${lang}[${i}]: lo(${b[0]}) > hi(${b[1]})`);
        });
      }
    }
  });

  test('en and ru bucket boundaries match', () => {
    for (const sid of Object.keys(SLIDER_RANGES)) {
      const { en, ru } = DD[sid];
      for (let i = 0; i < en.length; i++) {
        assert.equal(en[i][0], ru[i][0],
          `${sid}[${i}]: lo mismatch en=${en[i][0]} ru=${ru[i][0]}`);
        assert.equal(en[i][1], ru[i][1],
          `${sid}[${i}]: hi mismatch en=${en[i][1]} ru=${ru[i][1]}`);
      }
    }
  });
});

// ===========================================================================
// dynDesc lookup logic
// ===========================================================================

describe('dynDesc lookup logic', () => {
  test('exact lower boundary returns correct label', () => {
    assert.equal(lookup('sb',  0,  'en'), 'Off');
    assert.equal(lookup('sco', 20, 'en'), 'Very tall flames');
    assert.equal(lookup('ssp', 0,  'en'), 'Calm smoldering');
    assert.equal(lookup('sbl', 0,  'en'), 'Frozen glow');
  });

  test('exact upper boundary returns correct label', () => {
    assert.equal(lookup('sb',  100, 'en'), 'Full brightness');
    assert.equal(lookup('sco', 150, 'en'), 'Quick embers');
    assert.equal(lookup('ssp', 255, 'en'), 'Raging maximum');
    assert.equal(lookup('sbl', 255, 'en'), 'Sharp flicker');
  });

  test('mid-bucket values return correct labels', () => {
    assert.equal(lookup('sb',  13,  'en'), 'Very dim');     // [1,25]
    assert.equal(lookup('sb',  38,  'en'), 'Dim');          // [26,50]
    assert.equal(lookup('ssp', 130, 'en'), 'Active fire');  // [91,160]
    assert.equal(lookup('sco', 90,  'en'), 'Medium flames');// [71,105]
  });

  test('adjacent bucket boundary is handled correctly', () => {
    assert.equal(lookup('sb', 25, 'en'), 'Very dim');  // [1,25] hi
    assert.equal(lookup('sb', 26, 'en'), 'Dim');       // [26,50] lo
    assert.equal(lookup('sc', 80, 'en'), 'Saturated oranges'); // [56,80] hi
    assert.equal(lookup('sc', 81, 'en'), 'Deep reds only');    // [81,100] lo
  });

  test('returns Russian label when lang=ru', () => {
    assert.equal(lookup('sb',  0,   'ru'), 'Выключено');
    assert.equal(lookup('sb',  100, 'ru'), 'Максимум яркости');
    assert.equal(lookup('sco', 20,  'ru'), 'Очень высокое пламя');
    assert.equal(lookup('ssp', 255, 'ru'), 'Бушующий максимум');
  });

  test('non-ru lang falls back to English', () => {
    assert.equal(lookup('sb', 0, 'de'),        'Off');
    assert.equal(lookup('sb', 0, 'fr'),        'Off');
    assert.equal(lookup('sb', 0, undefined),   'Off');
    assert.equal(lookup('sb', 0, null),        'Off');
  });

  test('unknown sid returns undefined', () => {
    assert.equal(lookup('sx',  50, 'en'), undefined);
    assert.equal(lookup('',    0,  'en'), undefined);
    assert.equal(lookup('sXX', 0,  'en'), undefined);
  });

  test('value below slider range falls back to first-bucket label', () => {
    // sco minimum is 20; values below it have no matching bucket
    assert.equal(lookup('sco', 0,  'en'), 'Very tall flames');
    assert.equal(lookup('sco', 19, 'en'), 'Very tall flames');
  });

  test('value above slider range falls back to first-bucket label', () => {
    // sb maximum is 100; values above it have no matching bucket
    assert.equal(lookup('sb', 101, 'en'), 'Off');
    assert.equal(lookup('sb', 255, 'en'), 'Off');
  });

  test('all integer values in ssp [0,255] map to some label', () => {
    for (let v = 0; v <= 255; v++) {
      const label = lookup('ssp', v, 'en');
      assert.ok(label, `ssp: no label for value ${v}`);
    }
  });

  test('all integer values in sbl [0,255] map to some label', () => {
    for (let v = 0; v <= 255; v++) {
      const label = lookup('sbl', v, 'en');
      assert.ok(label, `sbl: no label for value ${v}`);
    }
  });
});

// ===========================================================================
// xf() header-merge logic — extracted from globals.js for unit testing.
// The real xf() wraps fetch(); here we test only the header-building step
// so no DOM or network is required.
// ===========================================================================

function mergeXfHeaders(o) {
  return Object.assign({'X-Requested-With': 'firelamp'}, o && o.headers);
}

describe('xf() header merging', () => {
  test('CSRF header is present when called with no options', () => {
    const h = mergeXfHeaders(undefined);
    assert.equal(h['X-Requested-With'], 'firelamp');
  });

  test('CSRF header is present when options has no headers key', () => {
    const h = mergeXfHeaders({ method: 'POST' });
    assert.equal(h['X-Requested-With'], 'firelamp');
  });

  test('caller headers are merged and preserved alongside CSRF', () => {
    const h = mergeXfHeaders({ headers: { 'Content-Type': 'application/json' } });
    assert.equal(h['X-Requested-With'], 'firelamp');
    assert.equal(h['Content-Type'], 'application/json');
  });

  test('caller cannot accidentally drop CSRF by passing empty headers', () => {
    const h = mergeXfHeaders({ headers: {} });
    assert.equal(h['X-Requested-With'], 'firelamp');
  });

  test('options object is not mutated', () => {
    const opts = { headers: { 'Content-Type': 'text/plain' } };
    mergeXfHeaders(opts);
    assert.deepEqual(Object.keys(opts.headers), ['Content-Type'],
      'original options.headers should not gain X-Requested-With');
  });
});

// ===========================================================================
// ps() slider clamping — mirrors state.js: Math.max(lo, Math.min(hi, n|0))
// ===========================================================================

function psClamp(lo, hi, n) {
  return Math.max(lo, Math.min(hi, n | 0));
}

describe('ps() slider clamping', () => {
  test('value within range is returned unchanged', () => {
    assert.equal(psClamp(0,   100, 50),  50);
    assert.equal(psClamp(20,  150, 85),  85);
    assert.equal(psClamp(0,   255, 0),    0);
    assert.equal(psClamp(0,   255, 255), 255);
  });

  test('value below lo is clamped to lo', () => {
    assert.equal(psClamp(0,   100, -1),   0);
    assert.equal(psClamp(20,  150, 0),   20);
    assert.equal(psClamp(20,  150, 19),  20);
  });

  test('value above hi is clamped to hi', () => {
    assert.equal(psClamp(0,   100, 101), 100);
    assert.equal(psClamp(0,   255, 300), 255);
    assert.equal(psClamp(20,  150, 151), 150);
  });

  test('float is truncated toward zero via |0', () => {
    assert.equal(psClamp(0, 100, 50.9), 50);
    assert.equal(psClamp(0, 100, 50.1), 50);
    assert.equal(psClamp(0, 100,  0.9),  0);
  });

  test('undefined/null become 0 via |0', () => {
    assert.equal(psClamp(0, 100, undefined), 0);
    assert.equal(psClamp(0, 100, null),      0);
  });

  test('undefined/null clamped to lo when lo > 0', () => {
    assert.equal(psClamp(20, 150, undefined), 20);
    assert.equal(psClamp(20, 150, null),      20);
  });
});

// ===========================================================================
// Preset import — validation and URL building extracted from presets.js
// ===========================================================================

// Mirrors the filter logic in the file-import handler.
function filterImport(arr) {
  if (!Array.isArray(arr)) throw new Error('not_array');
  const out = [];
  arr.forEach(function(pr) {
    if (!pr || typeof pr.slot !== 'number' || pr.slot < 0 || pr.slot > 7 || !pr.name) return;
    out.push(pr);
  });
  return out;
}

// Mirrors URL construction in the file-import forEach loop.
function buildImportUrl(pr) {
  let u = '/savepreset?slot=' + pr.slot + '&name=' + encodeURIComponent(String(pr.name).substring(0, 15));
  ['b', 'c', 'co', 'sp', 'bl', 'th'].forEach(function(k) {
    if (typeof pr[k] === 'number') u += '&' + k + '=' + pr[k];
  });
  return u;
}

describe('preset import — validation', () => {
  test('non-array input throws', () => {
    assert.throws(() => filterImport({}),   /not_array/);
    assert.throws(() => filterImport('[]'), /not_array/);
    assert.throws(() => filterImport(null), /not_array/);
  });

  test('null entry is skipped', () => {
    assert.deepEqual(filterImport([null, undefined, false]), []);
  });

  test('entry without slot field is skipped', () => {
    assert.deepEqual(filterImport([{ name: 'Blaze' }]), []);
  });

  test('entry with string slot is skipped', () => {
    assert.deepEqual(filterImport([{ slot: '0', name: 'Blaze' }]), []);
  });

  test('slot -1 and slot 8 are out of bounds and skipped', () => {
    assert.deepEqual(filterImport([{ slot: -1, name: 'X' }]), []);
    assert.deepEqual(filterImport([{ slot: 8,  name: 'X' }]), []);
  });

  test('entry without name is skipped', () => {
    assert.deepEqual(filterImport([{ slot: 0 }]), []);
    assert.deepEqual(filterImport([{ slot: 0, name: '' }]), []);
  });

  test('valid entry with all params passes through', () => {
    const pr = { slot: 3, name: 'Blaze', b: 80, c: 60, co: 45, sp: 100, bl: 50, th: 0 };
    assert.deepEqual(filterImport([pr]), [pr]);
  });

  test('valid entries at boundary slots 0 and 7 are accepted', () => {
    const a = { slot: 0, name: 'A' };
    const b = { slot: 7, name: 'B' };
    assert.deepEqual(filterImport([a, b]), [a, b]);
  });

  test('mixed valid and invalid — only valid entries returned', () => {
    const valid   = { slot: 1, name: 'Good' };
    const invalid = { slot: 9, name: 'Bad' };
    assert.deepEqual(filterImport([invalid, null, valid, {}]), [valid]);
  });
});

describe('preset import — URL building', () => {
  test('URL contains correct slot and encoded name', () => {
    const url = buildImportUrl({ slot: 2, name: 'Blaze' });
    assert.ok(url.includes('slot=2'),        url);
    assert.ok(url.includes('name=Blaze'),    url);
  });

  test('numeric params b, c, co, sp, bl, th are appended', () => {
    const url = buildImportUrl({ slot: 0, name: 'X', b: 80, c: 60, co: 45, sp: 100, bl: 50, th: 2 });
    assert.ok(url.includes('&b=80'),   url);
    assert.ok(url.includes('&c=60'),   url);
    assert.ok(url.includes('&co=45'),  url);
    assert.ok(url.includes('&sp=100'), url);
    assert.ok(url.includes('&bl=50'),  url);
    assert.ok(url.includes('&th=2'),   url);
  });

  test('string param values are not appended', () => {
    const url = buildImportUrl({ slot: 0, name: 'X', b: '80' });
    assert.ok(!url.includes('&b='), url);
  });

  test('unknown keys are not appended', () => {
    const url = buildImportUrl({ slot: 0, name: 'X', foo: 99, bar: 42 });
    assert.ok(!url.includes('&foo='), url);
    assert.ok(!url.includes('&bar='), url);
  });

  test('name is truncated to 15 characters', () => {
    const url  = buildImportUrl({ slot: 0, name: 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' });
    const name = decodeURIComponent(url.match(/name=([^&]*)/)[1]);
    assert.equal(name.length, 15, `expected 15 chars, got "${name}"`);
  });

  test('special characters in name are percent-encoded', () => {
    const url = buildImportUrl({ slot: 0, name: 'Огонь' });
    assert.ok(url.includes('name='), url);
    assert.ok(!url.includes('name=Огонь'), 'Cyrillic should be encoded');
  });
});

// ===========================================================================
// kAmb theme color table — mirrors state.js
// ===========================================================================

// Extract kAmb without executing any DOM calls.
function loadKAmb() {
  const src  = fs.readFileSync(path.join(ROOT, 'ui/js/state.js'), 'utf-8');
  const m    = src.match(/var kAmb=(\[[\s\S]*?\]);/);
  if (!m) throw new Error('kAmb not found in state.js');
  // eslint-disable-next-line no-new-func
  return new Function('return ' + m[1])();
}

const kAmb = loadKAmb();

describe('kAmb theme color table', () => {
  test('exactly 4 themes are defined', () => {
    assert.equal(kAmb.length, 4);
  });

  test('each theme entry has 14 values', () => {
    kAmb.forEach((theme, i) => {
      assert.equal(theme.length, 14, `theme ${i} should have 14 entries`);
    });
  });

  test('first 6 values per theme are non-negative integers (RGB components)', () => {
    kAmb.forEach((theme, i) => {
      for (let j = 0; j < 6; j++) {
        assert.ok(Number.isInteger(theme[j]) && theme[j] >= 0 && theme[j] <= 255,
          `theme ${i}[${j}] = ${theme[j]} is not a valid 0-255 integer`);
      }
    });
  });

  test('last 8 values per theme are CSS hex color strings', () => {
    kAmb.forEach((theme, i) => {
      for (let j = 6; j < 14; j++) {
        assert.match(String(theme[j]), /^#[0-9a-f]{6}$/i,
          `theme ${i}[${j}] = "${theme[j]}" is not a #rrggbb color`);
      }
    });
  });
});
