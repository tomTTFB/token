"""otpauth:// QR code parsing.

Decodes an uploaded image with pyzbar and parses the resulting otpauth://
URI. digits/period/algorithm are parsed and shown to the user for
confirmation, but note: src/account_link.cpp's ADD protocol doesn't accept
them -- every account it creates uses AccountStore's defaults (6 digits,
30s period, SHA1). If a parsed account differs, the caller should warn
that Token will still use the defaults, not the scanned values.
"""
from __future__ import annotations

import io
from dataclasses import dataclass
from urllib.parse import parse_qs, unquote, urlparse

from PIL import Image

from device_defaults import DEFAULT_DIGITS, DEFAULT_PERIOD


@dataclass
class ParsedAccount:
    name: str
    secret: str
    issuer: str
    digits: int
    period: int
    algorithm: str

    @property
    def matches_device_defaults(self) -> bool:
        return self.digits == DEFAULT_DIGITS and self.period == DEFAULT_PERIOD and self.algorithm == "SHA1"


class QrError(ValueError):
    pass


def decode_qr_image(image_bytes: bytes) -> str:
    """Returns the raw string payload of the first QR code found in an image."""
    # Imported lazily: pyzbar needs the system libzbar shared library, and
    # failing here (only when this feature is actually used) beats failing
    # at app startup and taking the whole tool down over a QR-only dependency.
    try:
        from pyzbar.pyzbar import decode as zbar_decode
    except ImportError as exc:
        raise QrError(
            "QR decoding isn't available: the zbar shared library is missing. "
            "Install it (e.g. `sudo apt install libzbar0` on Debian/Ubuntu) and restart the app, "
            "or use Manual Entry instead."
        ) from exc

    try:
        image = Image.open(io.BytesIO(image_bytes))
    except Exception as exc:
        raise QrError(f"Couldn't read that as an image: {exc}") from exc

    results = zbar_decode(image)
    if not results:
        raise QrError("No QR code found in that image")

    return results[0].data.decode("utf-8", errors="replace")


def parse_otpauth(uri: str) -> ParsedAccount:
    parsed = urlparse(uri)
    if parsed.scheme != "otpauth":
        raise QrError("Not an otpauth:// URI")
    if parsed.netloc != "totp":
        raise QrError("Only TOTP accounts are supported (this QR code is for HOTP)")

    label = unquote(parsed.path.lstrip("/"))
    if ":" in label:
        issuer, name = label.split(":", 1)
    else:
        issuer, name = "", label

    params = parse_qs(parsed.query)
    secret = params.get("secret", [""])[0].upper()
    if not secret:
        raise QrError("QR code has no secret parameter")

    try:
        digits = int(params.get("digits", [str(DEFAULT_DIGITS)])[0])
        period = int(params.get("period", [str(DEFAULT_PERIOD)])[0])
    except ValueError as exc:
        raise QrError(f"Invalid digits/period in QR code: {exc}") from exc

    return ParsedAccount(
        name=name.strip() or "Unnamed",
        secret=secret,
        issuer=params.get("issuer", [issuer])[0].strip(),
        digits=digits,
        period=period,
        algorithm=params.get("algorithm", ["SHA1"])[0].upper(),
    )
