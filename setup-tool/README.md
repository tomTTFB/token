# Token Setup Tool

A local Flask + Bootstrap web app for flashing Token firmware and adding 2FA
accounts over USB. See [`../plan/setup-tool.md`](../plan/setup-tool.md) for
the original design doc; this implementation covers what the firmware
actually exposes today, which is narrower than that doc describes (see
"Known limitations" below).

## Running it

```sh
cd setup-tool
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python app.py
```

Open `http://localhost:5000`.

**Linux:** `pyzbar` needs the system `libzbar0` library (`sudo apt install
libzbar0` on Debian/Ubuntu). You may also need to be in the `dialout` group
to access the serial port without sudo: `sudo usermod -a -G dialout $USER`
(log out and back in for it to take effect).

**Flashing:** the Flash page uses [ESP Web Tools](https://esphome.github.io/esp-web-tools/)
over the Web Serial API, so it needs a Chromium-based browser (Chrome, Edge,
Opera) on desktop. It reads firmware straight from `../.pio/build/T_Embed_CC1101/`,
so run `pio run` in the project root first and re-run it after every firmware
change &mdash; there's no build step in this tool itself.

## Pages

- **Home** &mdash; lists detected serial ports and flags ones that look like Token.
- **Flash** &mdash; flashes bootloader/partitions/firmware over USB straight from
  the browser (Web Serial), no `pio run -t upload` or driver install needed.
- **Add Account** &mdash; scan a QR code or enter a secret manually, then send it
  to Token over serial.

## Known limitations

`src/account_link.cpp` is the entire serial-side protocol Token's firmware
implements today: while the device is on its own Settings &rarr; Add Account
screen, it listens for one line (`ADD|name|issuer|secret\n`) and replies
`OK` or `ERR: <reason>`. That's it &mdash; no `ping`, `list_accounts`,
`delete_account`, `set_pin`, `set_time`, or `wipe` command exists over
serial. `plan/setup-tool.md`'s full JSON protocol was the original design
target, but time sync moved to on-device WiFi/NTP and BLE, and account
management beyond adding stayed device-only (encoder + on-screen menus).

Practical effects:
- This tool can't detect whether a connected port is actually Token (no
  `ping`) &mdash; the "likely Token" badge is just a USB vendor ID match.
- Adding an account only works while you've already navigated to
  Settings &rarr; Add Account on the device; the tool can't trigger that
  screen remotely.
- Every account added this way gets Token's hardcoded defaults (6 digits,
  30s period, SHA1) regardless of what a scanned QR code specifies. The Add
  Account page warns if a scanned account's settings differ.
- Listing, deleting, changing the PIN, and wiping are device-only for now.

## Security considerations

Binds to `127.0.0.1:5000` only (see the bottom of `app.py`) &mdash; not
reachable from other devices on the network. Account secrets pass through
this process's memory during a QR parse or an add-account request but are
never written to disk or logged.
