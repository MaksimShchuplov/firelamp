import subprocess
import sys
import os
Import("env")

try:
    sha = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"],
        stderr=subprocess.DEVNULL
    ).decode().strip()
    if not sha:
        raise ValueError("empty SHA")
except Exception as e:
    sha = "unknown"
    print(f"WARNING: could not read git SHA ({e}); FIRMWARE_VERSION will be 'unknown'", file=sys.stderr)

# 0 for local builds; CI sets GITHUB_RUN_NUMBER (monotonically increasing).
# Used to decide update availability: GitHub build_n > BUILD_N → update exists.
build_n = int(os.environ.get('GITHUB_RUN_NUMBER', '0'))

env.Append(CPPDEFINES=[
    ("FIRMWARE_VERSION", f'\\"{sha}\\"'),
    ("BUILD_N", build_n),
])
print(f"Firmware version: {sha}  build_n: {build_n}")
