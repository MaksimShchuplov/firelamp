import subprocess
import os
Import("env")

# ---- Git version ----
try:
    sha = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"],
        stderr=subprocess.DEVNULL
    ).decode().strip()
except Exception:
    sha = "unknown"

env.Append(CPPDEFINES=[("FIRMWARE_VERSION", f'\\"{sha}\\"')])
print(f"Firmware version: {sha}")

# ---- WiFi credentials ----
# Priority: secrets.ini (local dev) → env vars (CI / GitHub Actions)
ssid = pw = None

secrets_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "secrets.ini")
if os.path.exists(secrets_path):
    import configparser
    cfg = configparser.ConfigParser()
    cfg.read(secrets_path)
    if cfg.has_section("wifi"):
        ssid = cfg.get("wifi", "ssid", fallback=None)
        pw   = cfg.get("wifi", "pass", fallback=None)

if not ssid:
    ssid = os.environ.get("WIFI_SSID")
if not pw:
    pw = os.environ.get("WIFI_PASS")

if ssid and pw:
    env.Append(CPPDEFINES=[
        ("WIFI_SSID", f'\\"{ssid}\\"'),
        ("WIFI_PASS", f'\\"{pw}\\"'),
    ])
    print(f"WiFi SSID: {ssid}")
else:
    print("WARNING: WiFi credentials not found. Build will fail at #error guard.")
    print("  Local:  copy secrets.ini.example to secrets.ini and fill in credentials")
    print("  CI:     set WIFI_SSID and WIFI_PASS as GitHub Actions secrets")
