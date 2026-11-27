# zeebo-more-controllers

Make the Zeebo console recognise more USB HID controllers (DualShock 4, 8BitDo, generic pads, and
possibly more), and find out, with measurements, what it can and cannot do.

Status: **requirements baseline, data collection and the portable core with component tests (CT-001..010) are done. The BREW probe applet also builds and boots in zeebx: it lists the emulated pad, shows its card, polls events, and reports the raw-interface alias.** This is emulator evidence only; it has not been tested on a real console, and the raw-interface alias is an emulator stub, not proof of raw HID access. See `docs/IMPLEMENTATION.md` for measured capabilities and gaps, `ROADMAP.md` for the plan, and `docs/QUICK-WINS.md` for what can be done first.

## The problem in one paragraph

The Zeebo reads controllers through its own HID layer. Its firmware has a generic HID class driver that
fetches the report descriptor, and a file, `hid_devices.cfg`, that maps each known VID:PID to button and
axis UIDs (12 buttons, 4 axes, optional INVERT and RUMBLE). ZeeUtils can copy that file from an SD card
into the console. What is **not** known: how the console parses the file (button order, how the hat
switch is treated), whether a pad outside the file is exposed at all, whether raw HID reports are
reachable from an application, and whether the Bluetooth stack in the firmware can host HID pads.
Hard limits already known: no vendor-class devices (Xbox 360/One), no init commands in the file (DualShock 3,
Switch Pro), and commercial games read the HID API themselves, so a homebrew cannot feed them.

## Plan in short

1. **ZeeHID Probe**: a read-only BREW applet that lists the devices the console exposes and shows
   exactly what it delivers per pad (VID:PID, button UIDs, position fields, hat behaviour, raw access).
2. Use its logs to generate and test `hid_devices.cfg` entries for the pads that are plain HID.
3. Decide, on measured data, whether a USB bridge (RP2040 or similar) is needed for the rest.

Details: `docs/REQUIREMENTS.md` (V model, four levels), `docs/TEST-PLAN.md` (component to acceptance,
gates G0..G5 with kill criteria), `ROADMAP.md`.

## Layout

| Path | What it holds |
|---|---|
| `docs/REQUIREMENTS.md` | Needs, logical, technology and implementation requirements, hazards, assumptions, evidence annex. |
| `docs/TEST-PLAN.md` | Component (CT), integration (IT), system (ST) and acceptance (AT) tests; gates. |
| `docs/IMPLEMENTATION.md` | What is built, what it traces to, what is not built yet. |
| `src/`, `tests/`, `Makefile` | Portable C core (`core`, `fs`, `hidport`) and its component tests: `make test`. |
| `docs/QUICK-WINS.md` | Ranked low-effort items with evidence. |
| `docs/USB-HID-READING-GUIDE.md` | Where to read what in the USB and HID specifications (page numbers verified). |
| `ROADMAP.md` | Phases mapped to gates, effort, kill conditions. |
| `data/dreamcast-talk-t17404/` | The full USB4MAPLE forum thread (116 pages, 1155 posts) and the VID:PID list mined from it. |
| `data/usb4maple-firmware/` | RP2040 firmware from the project wiki (22/10/2025; no readable VID:PID table inside). The thread's first post attaches a newer one (15.12.25), not downloaded. |
| `data/nintendont-controllerconfigs/` | 43 Nintendont controller `.ini` files (data only): byte offset and mask per button, per VID:PID. |
| `data/linux-extracts/xpad_table.csv` | 322 Xbox-class VID:PID with protocol type, taken from the Linux `xpad.c` (code not stored). |
| `data/hid_devices_cfg/` | Two reference `hid_devices.cfg` files (SDK simulator, zeemu rootfs). None confirmed on a real console. |
| `data/derived/` | Generated: HID descriptors found in the forum logs, and the per-VID:PID cross reference. |
| `data/specs/` | USB 1.1, USB 2.0, HID 1.11, HID Usage Tables 1.7 (USB-IF; git-ignored, not for redistribution). |
| `data/SOURCES.md` | Origin, version and licence of every source. |
| `tools/` | `extract_descriptors.py`, `build_crossref.py`, `check_requirements.py`. |

## Not in git

`data/specs/`, `data/usb4maple-firmware/` and `data/dreamcast-talk-t17404/posts.json` are third-party material and
are ignored (see `.gitignore` and `data/SOURCES.md`). To rebuild `posts.json`, scrape thread 17404 of
dreamcast-talk.com with a visible Chromium (the site is behind a Cloudflare challenge) and save it in the same
format; the derived files in `data/derived/` are committed so the rest works without it.

## Tools

```
python3 tools/extract_descriptors.py          # posts.json -> data/derived/descriptors.{json,md}
XPAD_C=/path/to/xpad.c python3 tools/build_crossref.py   # -> data/derived/crossref.{json,md}
python3 tools/check_requirements.py           # traceability audit; exit 0 expected
```

The descriptor parser is minimal and written here from the HID 1.11 text; it was checked by hand on
one device (Speedlink TORID: 13 buttons at bit 0, hat of 4 bits at bit 16, four 8-bit axes from bit 24).
The "name" column of its output is the first line of the forum post, not a reliable name: identify
devices by VID:PID.

## Rules

- A fact needs a source in `data/SOURCES.md` or in the evidence annex. A belief that has not been
  measured goes in the assumptions table with the gate that decides it.
- Clean room: read GPL, AFL and vendor code, record facts (IDs, offsets, prototypes) with their origin, never
  copy code. No vendor header is vendored.
- The first application is read-only. Writing `hid_devices.cfg` from the console needs its own hazards
  review after gate G5.
- What the USB4MAPLE adapter supports says nothing about the Zeebo: it runs a different USB stack.
- RetroArch autoconfig files give VID:PID and names only; their button indices belong to the host input
  driver, not to the HID report layout.

## Open questions (each has a gate)

| Question | Gate |
|---|---|
| Does the console expose a pad that is not in `hid_devices.cfg`, under which device-type query? | G3 |
| Is the raw HID interface obtainable by an application? | G4 |
| How does the hat switch arrive? | G5 |
| Can an application write to the SD card? | G2 |
| Does the Bluetooth stack in the firmware host HID pads? | G3 |
| Do the IHID headers read here match the console's? | G1, G2 |
