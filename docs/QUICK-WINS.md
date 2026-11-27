# Quick wins

Low-effort items that move the project, each with the evidence that it is worth doing, a verification
command and what it needs. Effort is an estimate (a "session" is about half a day; "short" is under an hour).
Ranked by value for the effort. Nothing here has been done yet unless marked.

| ID | Item | Effort | Needs | Value |
|---|---|---|---|---|
| QW-1 | Dump the descriptors of the pads you own on the PC | short | you + pads | High |
| QW-2 | Fix the descriptor extractor: configuration descriptors and vendor-defined reports | short | nothing | Medium |
| QW-3 | Add the hat encoding and report layouts from the Nintendont files to the cross reference | 0.5 session | nothing | Medium |
| QW-4 | Host tool for `hid_devices.cfg`: parse, validate, diff, merge, back up | 1 session | nothing | High |
| QW-5 | Reuse the IHID adapter code of ZSDK as the starting point | saves ~1 session | licence check | High |
| QW-6 | First real datum with no new application: a draft cfg on the console | 0.5 session after QW-4 | console + backup | High, risk low |
| QW-7 | `git init` and a first local commit | short | your go | Low, free |
| QW-8 | Walking skeleton: an applet that only lists devices, run in zeebx | 1 to 2 sessions | nothing | High, not quick |

## QW-1 — Descriptors of your own pads (PC)

Evidence: the cross reference holds no descriptor for the DualShock 4 (`054C:05C4`, `054C:09CC`), the
Logitech Dual Action and RumblePad2 that the cfg lists, or the Mayflash Wii U Pro adapter; the DS4 appears only
through a Nintendont file and the zeemu cfg. (The two Logitech descriptors in the thread are G29 wheels.)
`lsusb` and `usbhid-dump` are installed on this machine.

Action: plug each pad, run `usbhid-dump -m <vid>:<pid>` and save the output under
`data/pc-dumps/<vid>-<pid>.txt` together with `lsusb -v -d <vid>:<pid>`. The extractor can read the same
byte format after QW-2.

Verification: `tools/extract_descriptors.py` lists the new devices with button count, hat and axes.

Related lead: the thread points to `github.com/sonik-br/pico_input_usb_tester`, a Pico-based tool that
prints VID, PID, descriptors and live reports. Useful for devices that cannot be plugged into a PC
(adapters, dongles) but needs a Pico, so it is not part of this item.

## QW-2 — Extractor fixes

Evidence (measured on the data):

- Two logs on forum page 74 (VID:PID shown as `0000:0000` and `0932:0004`, a Hori wireless stick) print
  as "report descriptor" the bytes `09 02 29 00 01 01 00 80 32 09 04 00 00 02 03 ... 09 21 10 01 ... 22 70 00
  07 05 81 03 40 00 0A 07 05 02 03 40 00 0A`. That is a **configuration descriptor**: 1 interface of class 3
  (HID), a HID descriptor announcing a 112-byte report descriptor, an interrupt IN endpoint of 64 bytes at
  10 ms and an interrupt OUT endpoint of 64 bytes. The log says `HID has 0 reports`: the adapter never
  received the real report descriptor. The extractor reports these as 0 buttons.
- Three descriptors (`2DC8:3207` twice, one of `2DC8:301C`) use usage page `0xFFA0`: vendor-defined
  reports of 63 or 64 bytes, not a gamepad layout. They are not parse failures.

Action: recognise a configuration descriptor (first bytes `09 02`) and decode interface, HID and endpoint
descriptors; label vendor-defined pages; show both in `descriptors.md`.

Verification: `python3 tools/extract_descriptors.py` marks the two page-74 rows as CONFIG_ONLY with
endpoint data and the three vendor rows as VENDOR; no row reports a parse failure.

## QW-3 — Hat encoding and layouts from the Nintendont files

Evidence: `controller_ps4v2.ini` (`054C:09CC`) gives `Up=5,00`, `RightUp=5,01`, `Right=5,02`, `DownRight=5,03`,
`Down=5,04`, `DownLeft=5,05`, `Left=5,06`, `UpLeft=5,07` (byte 5; the same byte holds the face-button
masks 0x10 to 0x80, so reading the hat as its low nibble is an inference), the stick bytes and the
rumble output report (`05 01 00 00 FF FF`). 40 of the 42 VID:PID in the files define `Up=`. The forum
descriptors already agree: all 13 that have a hat declare 4 bits with logical range 0 to 7 (measured). The
files add the direction order and the report layout of devices for which the forum has no descriptor.

Action: parse the button, hat, stick and rumble keys into `crossref.json` per VID:PID.

Verification: `crossref.json` has a `layout` block for the 42 devices; the DS4 hat table matches the
nibble above; the direction order is the same for every Nintendont device that defines it, or the exceptions are listed.

## QW-4 — `hid_devices.cfg` tool

Evidence: the format is fully visible in two files (SDK simulator, 173 lines; zeemu rootfs, 94 lines):
`VERSION`, then blocks of `VID:PID`, 12 `BUTTON`, 4 `AXIS`, optional `INVERT` and `RUMBLE`. ZeeUtils copies the
file over the one on the console, so a merge that drops the Zeebo's own entries (`1A5C:3033`, `15A2:0003`,
`1EAA:0135`) would remove its controller. The hazard is named in `docs/REQUIREMENTS.md`.

Action: `tools/cfg_tool.py` with `parse`, `validate` (duplicate VID:PID, unknown keys, button range),
`diff`, `merge` (never removes an existing entry; refuses to overwrite one without a flag) and an
automatic timestamped backup.

Verification: both reference files round-trip byte for byte (apart from line endings, which the SDK file
writes as CRLF); merging the zeemu file into the SDK file keeps every Zeebo entry.

## QW-5 — ZSDK IHID code as the adapter's start

Evidence: `zsdk-src/src/platform/zeebo/zeebo_input.cpp` (360 lines) already calls
`IHID_GetConnectedDevices`, `IHID_GetNextConnectEvent`, `IHID_GetDeviceInfo`, `IHIDDevice_GetPositionState`
and `IHIDDevice_Rumble`. It asks for the Joystick device type only, so a pad that is not typed as a
joystick would be invisible, which is why `LF-001` now asks per device-type UID.

Open point: `zsdk-src` has no licence file, and its README names a licence only for Lua. Read it
for structure and write our own adapter, or get the owner's permission before reusing code (clean-room
rule of the project).

Verification: the adapter written from it passes CT-009 with the fakes.

## QW-6 — The first real datum without the probe

Evidence: ZeeUtils copies `hid_devices.cfg` from the SD card, and the zeemu cfg already has DS4 entries.
Installing a cfg that adds one plain-HID pad, then checking whether any application that reads IHID (the Z-Wheel or a game) reacts to that pad,
answers "is a pad outside the original file usable at all" (assumption A-4) before any application exists.

Conditions: a backup of the original file first; use the merge of QW-4 so the Zeebo entries stay; one
pad per run; record the result as in `docs/TEST-PLAN.md` section 6. Reversible by restoring the backup.
Not part of the read-only probe: it writes a system file, so it is a separate experiment that you run.

Verification: the run record states which application reacted and how, with the pad model and the cfg used.

## QW-7 — Local repository

`git init` and a first local commit with `data/specs/` ignored (the `.gitignore` is already there). Nothing
is published. Needs your go.

## QW-8 — Walking skeleton (not quick)

An applet that only lists devices and draws text, built with the existing toolchain and run in zeebx.
It decides the packaging path (gate G1, emulator half) with the least code. The toolchain and examples
exist in `zsdk-src` and the SA1 port.

## Leads found while checking, with no action yet

- ZSDK wraps `IBT` and `IBTSPPClient` for the Zeebo (`zeebo_net_extra.cpp`), so Bluetooth is reachable from a
  BREW application in principle. The SDK headers here contain no Bluetooth HID interface, and the firmware has
  no HID-profile string. Assumption A-9 stays open until G3.
- The Zeebo SDK package 1.2.4 carries no HID header. The IHID headers used by the SA1 port and ZSDK come
  from the BREW MP SDK copy (`zeebo_demoscene/vendor/inc` is a set of symlinks).
