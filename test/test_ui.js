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
