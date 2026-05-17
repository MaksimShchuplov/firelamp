import subprocess
Import("env")

try:
    sha = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"],
        stderr=subprocess.DEVNULL
    ).decode().strip()
except Exception:
    sha = "unknown"

print(f"Firmware version: {sha}")
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", f'\\"{sha}\\"')])
