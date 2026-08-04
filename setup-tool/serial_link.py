"""Serial bridge between the browser and a Token device.

Token's only serial-side protocol today is the one implemented in
src/account_link.cpp: while the device is sitting on its own Settings ->
Add Account screen, it listens for a single line of the form

    ADD|name|issuer|secret\n

and replies with "OK" or "ERR: <reason>". There is no ping, list, delete,
set_pin, or wipe command over serial -- those stay device-only, driven by
the encoder and on-screen menus. This module only ever does the one thing
the firmware actually supports.
"""
from __future__ import annotations

import time
from dataclasses import dataclass

import serial
import serial.tools.list_ports

BAUD_RATE = 115200

# Espressif's registered USB vendor ID. The T-Embed CC1101 enumerates under
# it when running Token's native-USB-CDC build, so ports carrying it are
# flagged as "likely Token" -- other serial devices (a mouse dongle, an
# Arduino Uno, ...) won't match.
ESPRESSIF_VID = 0x303A


@dataclass
class PortInfo:
    device: str
    description: str
    described: bool
    likely_token: bool


@dataclass
class AddAccountResult:
    ok: bool
    message: str


def list_ports() -> list[PortInfo]:
    ports = []
    for p in serial.tools.list_ports.comports():
        # pyserial reports the literal string "n/a" (not None) when a
        # platform can't determine a port's description -- common on Linux
        # for ports without a USB product string. Treat it the same as no
        # description at all rather than showing the useless literal text.
        described = bool(p.description) and p.description != "n/a"
        ports.append(PortInfo(
            device=p.device,
            description=p.description if described else p.device,
            described=described,
            likely_token=(p.vid == ESPRESSIF_VID),
        ))
    # Likely matches first, then alphabetical within each group.
    ports.sort(key=lambda p: (not p.likely_token, p.device))
    return ports


def _reject_pipes(*fields: str) -> str | None:
    for field in fields:
        if "|" in field or "\n" in field or "\r" in field:
            return "name, issuer, and secret can't contain '|' or newlines"
    return None


def send_add_account(port: str, name: str, issuer: str, secret: str, timeout: float = 5.0) -> AddAccountResult:
    """Sends one ADD line and waits for the device's OK/ERR reply.

    The device only accepts this while its own UI has already called
    AccountLink::begin() (Settings -> Add Account) -- opening the port here
    doesn't trigger that. If the device isn't listening, this will most
    likely time out waiting for a response.
    """
    error = _reject_pipes(name, issuer, secret)
    if error:
        return AddAccountResult(ok=False, message=error)
    if not name.strip():
        return AddAccountResult(ok=False, message="name is required")

    try:
        with serial.Serial(port, BAUD_RATE, timeout=0.2) as ser:
            # Native USB CDC on the ESP32-S3 doesn't reset the chip on open
            # (unlike an external USB-UART bridge's auto-reset circuit), so
            # this doesn't disturb whatever the device is already doing.
            # Drain anything already buffered (e.g. the "waiting for
            # ADD|..." banner AccountLink::begin() printed) before sending.
            ser.reset_input_buffer()

            ser.write(f"ADD|{name}|{issuer}|{secret}\n".encode("utf-8"))
            ser.flush()

            deadline = time.monotonic() + timeout
            line = b""
            while time.monotonic() < deadline:
                chunk = ser.readline()
                if chunk:
                    line = chunk.strip()
                    if line:
                        break

            if not line:
                return AddAccountResult(
                    ok=False,
                    message="No response from Token. Is it on Settings -> Add Account?",
                )

            text = line.decode("utf-8", errors="replace")
            if text == "OK":
                return AddAccountResult(ok=True, message=f"{name} added to Token")
            if text.startswith("ERR:"):
                return AddAccountResult(ok=False, message=text[4:].strip())
            return AddAccountResult(ok=False, message=f"Unexpected reply: {text}")
    except serial.SerialException as exc:
        return AddAccountResult(ok=False, message=f"Serial error: {exc}")
