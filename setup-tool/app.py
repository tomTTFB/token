"""Token setup tool: a local Flask app for flashing firmware and adding
accounts over USB serial. See ../plan/setup-tool.md for the original design
doc -- this implementation only covers what the firmware actually exposes
today (see serial_link.py's docstring for the gap).

Binds to 127.0.0.1 only; see README.md's Security Considerations.
"""
from __future__ import annotations

import subprocess
from pathlib import Path

from flask import Flask, jsonify, render_template, request, send_from_directory

import serial_link
from qr import QrError, decode_qr_image, parse_otpauth

REPO_ROOT = Path(__file__).resolve().parent.parent
PIO_ENV = "T_Embed_CC1101"  # must match platformio.ini's default_envs
PIO_BUILD_DIR = REPO_ROOT / ".pio" / "build" / PIO_ENV
VENDORED_FIRMWARE_DIR = Path(__file__).resolve().parent / "firmware"

# (filename, offset, source directory) for the ESP32-S3 default 16MB
# partition table (boards/T_Embed_CC1101.json -> default_16MB.csv).
# bootloader/partitions/firmware.bin come fresh from the last `pio run`;
# boot_app0.bin never changes so it's vendored instead of pulled from
# PlatformIO's package cache (path varies by machine/OS).
FIRMWARE_PARTS = [
    ("bootloader.bin", 0x0, PIO_BUILD_DIR),
    ("partitions.bin", 0x8000, PIO_BUILD_DIR),
    ("boot_app0.bin", 0xE000, VENDORED_FIRMWARE_DIR),
    ("firmware.bin", 0x10000, PIO_BUILD_DIR),
]

app = Flask(__name__)


def firmware_version() -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "rev-parse", "--short", "HEAD"],
            capture_output=True, text=True, timeout=2,
        )
        if result.returncode == 0:
            sha = result.stdout.strip()
            dirty = subprocess.run(
                ["git", "-C", str(REPO_ROOT), "status", "--porcelain"],
                capture_output=True, text=True, timeout=2,
            ).stdout.strip()
            return f"{sha}{'-dirty' if dirty else ''}"
    except (OSError, subprocess.SubprocessError):
        pass
    return "unknown"


def firmware_build_missing() -> list[str]:
    """Filenames from FIRMWARE_PARTS that aren't present on disk yet."""
    missing = []
    for filename, _offset, directory in FIRMWARE_PARTS:
        if not (directory / filename).is_file():
            missing.append(filename)
    return missing


@app.route("/")
def index():
    return render_template(
        "index.html",
        ports=serial_link.list_ports(),
        missing_firmware=firmware_build_missing(),
    )


@app.route("/api/ports")
def api_ports():
    return jsonify([vars(p) for p in serial_link.list_ports()])


@app.route("/flash")
def flash():
    return render_template("flash.html", missing_firmware=firmware_build_missing())


@app.route("/manifest.json")
def manifest():
    return jsonify({
        "name": "Token",
        "version": firmware_version(),
        "new_install_prompt_erase": False,
        "builds": [
            {
                "chipFamily": "ESP32-S3",
                "parts": [
                    {"path": f"/firmware/{filename}", "offset": offset}
                    for filename, offset, _dir in FIRMWARE_PARTS
                ],
            }
        ],
    })


@app.route("/firmware/<filename>")
def firmware_file(filename: str):
    for part_name, _offset, directory in FIRMWARE_PARTS:
        if filename == part_name:
            return send_from_directory(directory, filename)
    return "Unknown firmware file", 404


@app.route("/accounts/add")
def accounts_add():
    return render_template("add_account.html", ports=serial_link.list_ports())


@app.route("/api/parse-qr", methods=["POST"])
def api_parse_qr():
    image = request.files.get("image")
    if image is None:
        return jsonify({"ok": False, "error": "No image uploaded"}), 400

    try:
        uri = decode_qr_image(image.read())
        account = parse_otpauth(uri)
    except QrError as exc:
        return jsonify({"ok": False, "error": str(exc)}), 400

    return jsonify({
        "ok": True,
        "account": {
            "name": account.name,
            "issuer": account.issuer,
            "secret": account.secret,
            "digits": account.digits,
            "period": account.period,
            "algorithm": account.algorithm,
            "matches_device_defaults": account.matches_device_defaults,
        },
    })


@app.route("/api/add-account", methods=["POST"])
def api_add_account():
    data = request.get_json(silent=True) or {}
    port = data.get("port", "")
    name = data.get("name", "")
    issuer = data.get("issuer", "")
    secret = data.get("secret", "")

    if not port:
        return jsonify({"ok": False, "error": "No serial port selected"}), 400

    result = serial_link.send_add_account(port, name, issuer, secret)
    status = 200 if result.ok else 400
    return jsonify({"ok": result.ok, "message": result.message}), status


if __name__ == "__main__":
    # 127.0.0.1 only -- never expose this to the network. See README.md.
    app.run(host="127.0.0.1", port=5000, debug=False)
