import subprocess
import sys
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

env.Append(CPPDEFINES=[("FIRMWARE_VERSION", f'\\"{sha}\\"')])
print(f"Firmware version: {sha}")
