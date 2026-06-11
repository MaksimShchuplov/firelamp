"""
Tests for build_page.py pre-build script.

Run with:  pytest test/ -v
"""
import os
import re
import pathlib
import pytest
import tempfile

_HERE = pathlib.Path(__file__).parent
_BP   = _HERE.parent / "build_page.py"


# ---------------------------------------------------------------------------
# Load minify_css / minify_js directly from build_page.py.
# Import("env") is a SCons builtin injected at runtime; mock it as a no-op.
# The final `build_page(None, None, env)` call raises NameError because `env`
# is never set by the no-op — catch it and use the already-defined functions.
# ---------------------------------------------------------------------------
def _load_build_page():
    ns = {"Import": lambda *a: None, "re": re, "os": os}
    src = _BP.read_text(encoding="utf-8")
    try:
        exec(compile(src, str(_BP), "exec"), ns)  # noqa: S102
    except NameError:
        pass  # `env` is undefined at the last bare call — expected
    return ns


_bp        = _load_build_page()
minify_css = _bp["minify_css"]
minify_js  = _bp["minify_js"]


# ===========================================================================
# CSS minifier
# ===========================================================================

class TestMinifyCss:
    def test_strips_block_comment(self):
        assert minify_css("a { /* comment */ color: red; }") == "a{color:red;}"

    def test_strips_multiline_comment(self):
        result = minify_css("/* multi\nline */\na { color: red; }")
        assert "multi" not in result
        assert "a{color:red;}" == result

    def test_collapses_whitespace(self):
        assert minify_css("a   {   color:   red;   }") == "a{color:red;}"

    def test_removes_colon_space(self):
        result = minify_css("a { color: red; font-size: 12px; }")
        assert "color:red" in result
        assert "font-size:12px" in result

    def test_removes_spaces_around_braces_and_semicolons(self):
        result = minify_css("a { b: 1; } c { d: 2; }")
        assert " {" not in result
        assert "{ " not in result

    def test_empty_input(self):
        assert minify_css("") == ""

    def test_preserves_string_content_value(self):
        # A string value with spaces should not lose its content
        result = minify_css('.x { content: "hello world"; }')
        assert "hello world" in result


# ===========================================================================
# JS minifier
# ===========================================================================

class TestMinifyJs:
    def test_strips_block_comment(self):
        assert minify_js("/* block */ var x = 1;") == "var x = 1;"

    def test_strips_line_comment(self):
        result = minify_js("var x = 1; // comment\nvar y = 2;")
        assert "comment" not in result
        assert "var x = 1;" in result
        assert "var y = 2;" in result

    def test_strips_comment_only_line(self):
        assert minify_js("// entire line\n") == ""

    def test_preserves_double_quoted_string_with_comment_syntax(self):
        s = 'var s = "// not a comment /* also not */";'
        assert minify_js(s) == s.strip()

    def test_preserves_single_quoted_string_with_comment_syntax(self):
        s = "var s = '// not a comment';"
        assert minify_js(s) == s.strip()

    def test_preserves_template_literal_with_comment_syntax(self):
        s = "var s = `/* keep ${x} */`;"
        assert minify_js(s) == s.strip()

    def test_preserves_escaped_quote_in_double_quoted_string(self):
        s = r'var s = "say \"hi\"";'
        assert minify_js(s) == s.strip()

    def test_preserves_escaped_quote_in_single_quoted_string(self):
        s = r"var s = 'it\'s fine';"
        assert minify_js(s) == s.strip()

    def test_preserves_backslash_in_string(self):
        s = r'var p = "C:\\Users\\";'
        assert minify_js(s) == s.strip()

    def test_collapses_multiple_lines(self):
        src = "var x = 1;\nvar y = 2;\nvar z = 3;"
        result = minify_js(src)
        assert "\n" not in result
        assert "var x = 1;" in result

    def test_empty_input(self):
        assert minify_js("") == ""

    def test_block_comment_without_terminator_does_not_crash(self):
        # Unterminated block comment: consume to EOF, no crash
        result = minify_js("/* unterminated")
        assert result == ""


# ===========================================================================
# HTML placeholder replacement (the lambda fix)
# ===========================================================================

def _replace(css: str, js: str, html: str) -> str:
    """Same replacement logic as build_page.py build_page()."""
    out = re.sub(
        r'(<link rel=stylesheet href=css/[^>]+>\n?)+',
        lambda m: "<style>" + css + "</style>",
        html, count=1,
    )
    out = re.sub(
        r'(<script src=js/[^>]+></script>\n?)+',
        lambda m: "<script>" + js + "</script>",
        out, count=1,
    )
    return out


class TestHtmlReplacement:
    def test_single_link_replaced_by_style_tag(self):
        html = '<link rel=stylesheet href=css/base.css>\n<p>hi</p>'
        result = _replace("body{}", "", html)
        assert "<link" not in result
        assert "<style>body{}</style>" in result

    def test_multiple_links_collapsed_into_one_style_tag(self):
        html = (
            '<link rel=stylesheet href=css/a.css>\n'
            '<link rel=stylesheet href=css/b.css>\n'
        )
        result = _replace("combined", "", html)
        assert result.count("<style>") == 1
        assert "<link" not in result

    def test_single_script_replaced_by_inline_script(self):
        html = '<p></p>\n<script src=js/main.js></script>\n'
        result = _replace("", "var x=1;", html)
        assert '<script src=' not in result
        assert '<script>var x=1;</script>' in result

    def test_multiple_scripts_collapsed_into_one(self):
        html = (
            '<script src=js/a.js></script>\n'
            '<script src=js/b.js></script>\n'
        )
        result = _replace("", "combined", html)
        assert result.count("<script>") == 1
        assert '<script src=' not in result

    def test_other_html_content_is_preserved(self):
        html = (
            '<link rel=stylesheet href=css/base.css>\n'
            '<h1>Hello</h1>\n'
            '<script src=js/main.js></script>\n'
        )
        result = _replace("css", "js", html)
        assert "<h1>Hello</h1>" in result

    def test_backslash_digit_in_css_not_treated_as_group_reference(self):
        """
        Regression: CSS like content:'\\1bullet' must survive re.sub verbatim.
        Without the lambda fix, \\1 in the replacement string is interpreted as
        a back-reference to capture group 1 and the link tag is substituted in.
        """
        css = r"a::before{content:'\1bullet'}"
        html = '<link rel=stylesheet href=css/base.css>\n'
        result = _replace(css, "", html)
        assert r"\1" in result, "Backslash-digit was corrupted by group-reference substitution"
        assert "<link" not in result

    def test_backslash_digit_in_js_not_treated_as_group_reference(self):
        js = r"var re=/\d+/g;"
        html = '<script src=js/main.js></script>\n'
        result = _replace("", js, html)
        assert r"\d" in result
        assert "<script src=" not in result

    def test_direct_string_repl_corrupts_backslash_group_ref(self):
        """
        Demonstrate the bug that the lambda fix prevents: without lambda the
        CSS \\1 token is expanded to the content of capture group 1.
        """
        css = r"content:'\1bullet'"
        html = '<link rel=stylesheet href=css/base.css>\n'
        # Direct (buggy) replacement — \1 in repl string → group 1 content
        result_buggy = re.sub(
            r'(<link rel=stylesheet href=css/[^>]+>\n?)+',
            "<style>" + css + "</style>",
            html, count=1,
        )
        # The link element itself was substituted for \1 — CSS is corrupted
        assert "<link" in result_buggy, (
            "Expected \\1 to be replaced by group 1 (the link tag) "
            "in the unfixed code path"
        )


# ===========================================================================
# Source-level guards — protect the fixes from silent regression
# ===========================================================================

class TestBuildPageSource:
    def test_uses_lambda_for_css_re_sub(self):
        src = _BP.read_text(encoding="utf-8")
        assert 'lambda m: "<style>"' in src, (
            "build_page.py must use 'lambda m:' for the CSS re.sub replacement "
            "to prevent CSS backslash sequences from being treated as group references"
        )

    def test_uses_lambda_for_js_re_sub(self):
        src = _BP.read_text(encoding="utf-8")
        assert 'lambda m: "<script>"' in src, (
            "build_page.py must use 'lambda m:' for the JS re.sub replacement "
            "to prevent JS backslash sequences from being treated as group references"
        )

    def test_flpg_sentinel_guard_present(self):
        src = _BP.read_text(encoding="utf-8")
        assert ')FLPG' in src and 'ValueError' in src, (
            "build_page.py must guard against content containing the ')FLPG' raw-string "
            "delimiter and raise ValueError if found"
        )
