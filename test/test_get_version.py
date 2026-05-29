"""
Tests for get_version.py pre-build script.

Run with:  pytest test/ -v
"""
import subprocess
import sys
import os
import pathlib
from unittest.mock import patch, MagicMock

_HERE = pathlib.Path(__file__).parent
_GV   = _HERE.parent / "get_version.py"


def _run(*, sha_bytes=None, sha_raises=None, run_number_env=None):
    """
    Execute get_version.py in an isolated namespace.

    sha_bytes      – bytes returned by check_output; default b'deadbee'
    sha_raises     – if set, check_output raises this exception
    run_number_env – value for GITHUB_RUN_NUMBER, or None to unset it
    Returns dict of keyword args passed to env.Append.
    """
    recorded = {}
    mock_env = MagicMock()
    mock_env.Append.side_effect = lambda **kw: recorded.update(kw)

    def _check_output(cmd, **kw):
        if sha_raises is not None:
            raise sha_raises
        return sha_bytes if sha_bytes is not None else b'deadbee'

    env_patch = dict(os.environ)
    if run_number_env is not None:
        env_patch['GITHUB_RUN_NUMBER'] = str(run_number_env)
    else:
        env_patch.pop('GITHUB_RUN_NUMBER', None)

    ns = {
        'Import': lambda *a: None,
        'subprocess': subprocess,
        'sys': sys,
        'os': os,
        'env': mock_env,
    }

    with patch.object(subprocess, 'check_output', _check_output), \
         patch.dict(os.environ, env_patch, clear=True):
        src = _GV.read_text(encoding='utf-8')
        exec(compile(src, str(_GV), 'exec'), ns)  # noqa: S102

    return recorded, mock_env


def _defines(recorded):
    """Return CPPDEFINES list as a dict for easy lookup."""
    return dict(recorded['CPPDEFINES'])


class TestFirmwareVersion:
    def test_git_sha_injected_on_success(self):
        recorded, _ = _run(sha_bytes=b'abc1234\n')
        assert _defines(recorded)['FIRMWARE_VERSION'] == r'\"abc1234\"'

    def test_sha_stripped_of_whitespace(self):
        recorded, _ = _run(sha_bytes=b'  deadbee  \n')
        assert _defines(recorded)['FIRMWARE_VERSION'] == r'\"deadbee\"'

    def test_fallback_to_unknown_on_subprocess_error(self):
        recorded, _ = _run(sha_raises=subprocess.CalledProcessError(128, 'git'))
        assert _defines(recorded)['FIRMWARE_VERSION'] == r'\"unknown\"'

    def test_fallback_to_unknown_on_oserror(self):
        recorded, _ = _run(sha_raises=OSError("git not found"))
        assert _defines(recorded)['FIRMWARE_VERSION'] == r'\"unknown\"'

    def test_fallback_to_unknown_on_empty_sha(self):
        # Empty output triggers ValueError("empty SHA") internally
        recorded, _ = _run(sha_bytes=b'')
        assert _defines(recorded)['FIRMWARE_VERSION'] == r'\"unknown\"'

    def test_firmware_version_format_wraps_sha_in_escaped_quotes(self):
        # PlatformIO needs the C macro to expand to a string literal:
        # FIRMWARE_VERSION expands to "sha" — so the value must be \"sha\"
        recorded, _ = _run(sha_bytes=b'abc1234')
        fv = _defines(recorded)['FIRMWARE_VERSION']
        assert fv.startswith(r'\"')
        assert fv.endswith(r'\"')
        assert 'abc1234' in fv


class TestBuildNumber:
    def test_build_n_from_github_run_number(self):
        recorded, _ = _run(run_number_env=42)
        assert _defines(recorded)['BUILD_N'] == 42

    def test_build_n_is_int_not_string(self):
        recorded, _ = _run(run_number_env=7)
        assert isinstance(_defines(recorded)['BUILD_N'], int)

    def test_build_n_defaults_to_zero_when_env_absent(self):
        recorded, _ = _run()
        assert _defines(recorded)['BUILD_N'] == 0

    def test_build_n_defaults_to_zero_when_env_is_empty_string(self):
        # Some CI wrappers set GITHUB_RUN_NUMBER='' rather than leaving it unset.
        # os.environ.get('GITHUB_RUN_NUMBER', '0') returns '' not '0' in that case,
        # so int('') would raise ValueError without the `or '0'` guard.
        env_patch = dict(os.environ)
        env_patch['GITHUB_RUN_NUMBER'] = ''
        recorded = {}
        mock_env = MagicMock()
        mock_env.Append.side_effect = lambda **kw: recorded.update(kw)
        def _chk(cmd, **kw): return b'abc1234'
        ns = {'Import': lambda *a: None, 'subprocess': subprocess, 'sys': sys, 'os': os, 'env': mock_env}
        with patch.object(subprocess, 'check_output', _chk), \
             patch.dict(os.environ, env_patch, clear=True):
            src = _GV.read_text(encoding='utf-8')
            exec(compile(src, str(_GV), 'exec'), ns)  # noqa: S102
        assert _defines(recorded)['BUILD_N'] == 0

    def test_build_n_large_value(self):
        recorded, _ = _run(run_number_env=9999)
        assert _defines(recorded)['BUILD_N'] == 9999


class TestEnvAppendCall:
    def test_env_append_called_exactly_once(self):
        _, mock_env = _run()
        mock_env.Append.assert_called_once()

    def test_cppdefines_contains_both_keys(self):
        recorded, _ = _run()
        keys = [k for k, _ in recorded['CPPDEFINES']]
        assert 'FIRMWARE_VERSION' in keys
        assert 'BUILD_N' in keys

    def test_cppdefines_is_a_list_of_tuples(self):
        recorded, _ = _run()
        for item in recorded['CPPDEFINES']:
            assert isinstance(item, tuple), f"Expected tuple, got {type(item)}"
            assert len(item) == 2
