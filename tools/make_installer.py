#!/usr/bin/env python3
"""Builds the web-installer folder: a merged factory image, an OTA image and the ESP Web Tools manifest.

Usage:  python tools/make_installer.py [--build] [--out installer]
Needs a completed `pio run` (or --build to run it) and esptool (`pip install "esptool~=4.8"`).
"""
import argparse, hashlib, json, os, pathlib, re, shutil, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
ENV = "seengreat_hub75_s3"
BUILD = ROOT / ".pio" / "build" / ENV

def version():
    m = re.search(r'MWC_VERSION=\\"([^"\\]+)\\"', (ROOT / "platformio.ini").read_text())
    return m.group(1) if m else "0.0.0"

def boot_app0():
    home = pathlib.Path(os.environ.get("PLATFORMIO_CORE_DIR", pathlib.Path.home() / ".platformio"))
    for p in home.glob("packages/framework-arduinoespressif32*/tools/partitions/boot_app0.bin"):
        return p
    sys.exit("boot_app0.bin not found in the PlatformIO packages")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", action="store_true", help="run pio run first")
    ap.add_argument("--out", default="installer")
    a = ap.parse_args()
    out = ROOT / a.out
    out.mkdir(exist_ok=True)
    if a.build:
        subprocess.check_call(["pio", "run", "-e", ENV], cwd=ROOT)
    ver = version()
    parts = {"bootloader": BUILD / "bootloader.bin", "partitions": BUILD / "partitions.bin",
             "boot_app0": boot_app0(), "firmware": BUILD / "firmware.bin"}
    for name, p in parts.items():
        if not p.exists():
            sys.exit(f"{name} missing: {p} (run pio run first)")
    for old in out.glob("matrix-clock-*.bin"):
        old.unlink()
    factory = out / f"matrix-clock-{ver}-factory.bin"
    ota = out / f"matrix-clock-{ver}-ota.bin"
    subprocess.check_call([sys.executable, "-m", "esptool", "--chip", "esp32s3", "merge_bin", "-o", str(factory),
                           "--flash_mode", "keep", "--flash_freq", "keep", "--flash_size", "16MB",
                           "0x0", str(parts["bootloader"]), "0x8000", str(parts["partitions"]),
                           "0xe000", str(parts["boot_app0"]), "0x10000", str(parts["firmware"])])
    shutil.copyfile(parts["firmware"], ota)
    ota_bytes = ota.read_bytes()
    manifest = {
        "name": "Matrix Clock",
        "version": ver,
        "new_install_prompt_erase": True,
        "new_install_improv_wait_time": 0,
        "builds": [{"chipFamily": "ESP32-S3", "parts": [{"path": factory.name, "offset": 0}]}],
        # used by the firmware's self-updater
        "ota": ota.name,
        "ota_size": len(ota_bytes),
        "ota_md5": hashlib.md5(ota_bytes).hexdigest(),
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"installer: {factory.name} ({factory.stat().st_size} bytes), {ota.name}, manifest.json (v{ver})")

if __name__ == "__main__":
    main()
