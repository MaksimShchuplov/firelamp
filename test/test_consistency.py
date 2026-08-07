"""
Cross-file consistency tests: config.h is the single source of truth for
parameter ranges/defaults, but the UI mirrors them in four places
(index.html slider attrs, state.js clamps, sliders.js vibration bounds,
globals.js DD description buckets). These tests catch the drift that
one-file edits inevitably produce — e.g. the historical cooling 45/46,
sparking 36/26 mismatch between index.html and config.h.

Run with:  pytest test/ -v
"""
import pathlib
import re

_ROOT = pathlib.Path(__file__).parent.parent

_CONFIG   = (_ROOT / "src" / "config.h").read_text(encoding="utf-8")
_HTML     = (_ROOT / "ui" / "index.html").read_text(encoding="utf-8")
_STATE    = (_ROOT / "ui" / "js" / "state.js").read_text(encoding="utf-8")
_SLIDERS  = (_ROOT / "ui" / "js" / "sliders.js").read_text(encoding="utf-8")
_GLOBALS  = (_ROOT / "ui" / "js" / "globals.js").read_text(encoding="utf-8")
_PARAMS   = (_ROOT / "src" / "params.cpp").read_text(encoding="utf-8")
_PRESETS  = (_ROOT / "ui" / "js" / "presets.js").read_text(encoding="utf-8")


def _define(name):
    m = re.search(r"#define\s+%s\s+(\d+)" % re.escape(name), _CONFIG)
    assert m, "config.h: missing #define %s" % name
    return int(m.group(1))


# slider id -> config.h prefix
SLIDER_PARAMS = {
    "sb":  "BRIGHT",
    "sc":  "CONTRAST",
    "sco": "COOLING",
    "ssp": "SPARKING",
    "sbl": "BLEND",
}


# ===========================================================================
# index.html slider min/max/value vs config.h *_MIN/*_MAX/*_DEFAULT
# ===========================================================================

def _html_slider(sid):
    m = re.search(r"<input[^>]*id=%s\b[^>]*>" % sid, _HTML)
    assert m, "index.html: no slider with id=%s" % sid
    tag = m.group(0)
    attrs = dict(re.findall(r"(min|max|value)=(\d+)", tag))
    assert set(attrs) == {"min", "max", "value"}, "index.html %s: missing attrs" % sid
    return {k: int(v) for k, v in attrs.items()}


def test_html_slider_ranges_match_config():
    for sid, prefix in SLIDER_PARAMS.items():
        got = _html_slider(sid)
        assert got["min"] == _define(prefix + "_MIN"), "%s min" % sid
        assert got["max"] == _define(prefix + "_MAX"), "%s max" % sid
        assert got["value"] == _define(prefix + "_DEFAULT"), "%s default" % sid


# ===========================================================================
# state.js ps(lo,hi,...) clamps vs config.h
# ===========================================================================

def test_state_js_clamps_match_config():
    fns = {"sb": "pb", "sc": "pc", "sco": "pco", "ssp": "psp", "sbl": "pbl"}
    for sid, fn in fns.items():
        m = re.search(r"function %s\(n\)\{[^}]*?ps\((\d+),(\d+)," % fn, _STATE)
        assert m, "state.js: %s() does not call ps(lo,hi,...)" % fn
        prefix = SLIDER_PARAMS[sid]
        assert int(m.group(1)) == _define(prefix + "_MIN"), "%s lo" % fn
        assert int(m.group(2)) == _define(prefix + "_MAX"), "%s hi" % fn


# ===========================================================================
# sliders.js vib(v,min,max) bounds vs config.h
# ===========================================================================

def test_sliders_js_vib_bounds_match_config():
    for m in re.finditer(r"vib\(\+(s\w+)\.value,(\d+),(\d+)\)", _SLIDERS):
        sid, lo, hi = m.group(1), int(m.group(2)), int(m.group(3))
        prefix = SLIDER_PARAMS[sid]
        assert lo == _define(prefix + "_MIN"), "vib %s lo" % sid
        assert hi == _define(prefix + "_MAX"), "vib %s hi" % sid


# ===========================================================================
# globals.js DD description buckets: full coverage, contiguous, EN == RU
# ===========================================================================

def _dd_buckets():
    out = {}
    for m in re.finditer(r"(s\w+):\{en:(\[\[.*?\]\]),\s*ru:(\[\[.*?\]\])\}",
                         _GLOBALS, re.DOTALL):
        sid = m.group(1)
        out[sid] = {
            lang: [(int(a), int(b))
                   for a, b in re.findall(r"\[(\d+),(\d+),", m.group(i))]
            for i, lang in ((2, "en"), (3, "ru"))
        }
    return out


def test_dd_buckets_cover_config_ranges():
    dd = _dd_buckets()
    assert set(dd) == set(SLIDER_PARAMS), "DD sliders != expected set"
    for sid, langs in dd.items():
        prefix = SLIDER_PARAMS[sid]
        lo, hi = _define(prefix + "_MIN"), _define(prefix + "_MAX")
        assert langs["en"] == langs["ru"], "DD %s: EN/RU boundaries differ" % sid
        b = langs["en"]
        assert b[0][0] == lo, "DD %s: first bucket starts at %d, want %d" % (sid, b[0][0], lo)
        assert b[-1][1] == hi, "DD %s: last bucket ends at %d, want %d" % (sid, b[-1][1], hi)
        for (a1, b1), (a2, _) in zip(b, b[1:]):
            assert a2 == b1 + 1, "DD %s: gap/overlap at %d..%d" % (sid, b1, a2)


# ===========================================================================
# params.cpp registry keys vs presets.js import key list, counts vs config.h
# ===========================================================================

def test_presets_js_keys_match_registry():
    reg = re.findall(r'\{"(\w+)",', _PARAMS)
    assert reg, "params.cpp: PARAMS[] keys not found"
    m = re.search(r"\[((?:'\w+',?)+)\]\.forEach", _PRESETS)
    assert m, "presets.js: import key list not found"
    js = re.findall(r"'(\w+)'", m.group(1))
    assert js == [k for k in reg if k != "b"] or js == reg, \
        "presets.js keys %r != params.cpp registry %r" % (js, reg)


def test_button_counts_match_config():
    themes = {int(n) for n in re.findall(r"id=tb(\d+)", _HTML)}
    assert themes == set(range(_define("THEME_COUNT"))), "theme buttons vs THEME_COUNT"
    presets = {int(n) for n in re.findall(r"id=pr(\d+)", _HTML)}
    assert presets == set(range(_define("PRESET_COUNT"))), "preset buttons vs PRESET_COUNT"
    m = re.search(r"pr\.slot>(\d+)", _PRESETS)
    assert m and int(m.group(1)) == _define("PRESET_COUNT") - 1, \
        "presets.js import slot bound vs PRESET_COUNT"
