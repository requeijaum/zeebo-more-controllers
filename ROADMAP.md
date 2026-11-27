# Roadmap

Phases map onto the gates of `docs/TEST-PLAN.md` (G0..G5). Effort is in work sessions (about half a
day each) and is an estimate, not a measurement. A phase ends only when its exit criterion is met;
a kill condition stops the project or the route, and is written before the work it can stop.

## Phase 0 — Baseline (done)

- Requirements (70 items, V model) and test plan (33 tests) with a mechanical audit that exits 0.
- Data: forum thread, 21 HID descriptors, Nintendont configs, Xbox-class table, specifications.
- Output of the audit: 9 assumptions, each with its deciding gate.

## Phase 1 — Quick wins on the host (no console) · gate: none · ~2 sessions

Items in `docs/QUICK-WINS.md`: descriptor extractor fixes, device-type finding applied to the plan,
`hid_devices.cfg` parser and merger with backup, hat encoding table from the Nintendont files, a git
repository. Exit: each item's verification command passes.

Kill: none.

## Phase 2 — Core and component tests · G0 · ~3 sessions · **done on the host (G0 met)**

Pure-C core (event ring, device table, directional-pad classifier, log formatter, hex dump, path
allowlist) with fakes for IHID, IHIDDevice and the raw interface. Start the adapter from the existing
IHID code in `zsdk-src/src/platform/zeebo/zeebo_input.cpp` instead of writing it again.

Exit: CT-001..010 pass under ASan and UBSan; `tools/check_requirements.py` exits 0. Met: see `docs/IMPLEMENTATION.md`. The BREW adapter itself waits for phase 3.

Kill: a requirement that cannot be given a method and a criterion is deleted.

## Phase 3 — Walking skeleton in the emulator · G1 (emulator half) · done
The applet builds and runs in zeebx: it lists the pad, shows a card, polls events, probes the raw
interface and fires a rumble pulse. See `docs/IMPLEMENTATION.md` for what was measured.

A minimal applet that lists devices and draws text, built with the ZSDK/BREW 4.0.2 toolchain,
packaged as `.mod` and `.mif`, run in zeebx. Then the screens and the log.
Deviation: the text is drawn with `IDISPLAY_DrawText` (built-in font), not OpenGL ES. The skeleton
needs a screen, not a renderer; GL text needs its own font path and is the A-7 fallback in TR-003.

Exit: IT-001..007 and ST-001..009 pass.

Kill: none; a failure here is a bug, not a route decision.

## Phase 4 — First contact with the console · G1 (console half), G2 · ~2 sessions of bench time

Install by the ZeeUtils flow, start the applet, open the self-test page, plug a known Z-Pad.
Decides assumptions A-1 (interfaces behave as documented), A-3 (SD writable), A-7 (text readable), A-8
(header layouts) and compares the emulator against the console (A-6).

Exit: AT-001 and AT-002 pass.

Kill: if the applet does not start after two clean installs while ZeeUtils itself starts, stop and
diagnose the packaging. If the known Z-Pad is not listed, the IHID route is wrong: stop and report.

## Phase 5 — The two single-run answers · G3 and G4 · ~1 session each

- G3: plug a plain HID pad that is not in `hid_devices.cfg`; hot-plug and unplug. Records whether it
  appears and under which device-type query (A-4) and whether the Bluetooth flag ever shows (A-9).
- G4: run the raw probe. Records AVAILABLE or NOT AVAILABLE(code) (A-2).

Exit: AT-003 and AT-004 recorded. Neither result stops the project; each one changes the route and is
written up as a deliverable:

| Outcome | Consequence |
|---|---|
| Pad not exposed without a cfg entry | A permissive entry must be installed first; the probe measures from there. |
| Raw interface not available | Pads that need an init command (DualShock 3, Switch Pro) go to the bridge route. |
| Raw interface available | The probe may send init reports; the init-needing pads stay in scope. |

## Phase 6 — Measurement of real pads · G5 · ~3 sessions

Guided capture and live view on at least three pads (start with the Speedlink TORID, an 8BitDo in
D-input mode and a DualShock 4). Records button UID order, position fields and hat behaviour (A-5);
frame rate; log export and host parsing; full mission with a Z-Pad only on a TV.

Exit: AT-005, AT-006 and AT-007 pass.

Kill: if no pad other than the Z-Pad can be captured, stop and report.

## Phase 7 — Decision and first cfg entries · after G5 · ~2 sessions

Write `hid_devices.cfg` entries for the pads whose behaviour was measured, merge them without losing
the console's own entries, test on the console with the probe still running. Then decide on measured
data: continue with the cfg route only, add a USB bridge for the remainder (Bluetooth, vendor-class,
init-needing pads), or stop.

A hazards review precedes any application that writes the cfg; that application is outside this
baseline.

Exit: a written decision with the measurements that support it.

## What would end the project

- The IHID route does not work at all: the known Z-Pad is not listed (decided at G2).
- No pad other than the Z-Pad can be made to appear, even with a cfg entry (G5).

A negative result is a deliverable: one page stating what was tried, what was measured, which
assumption failed and what would make the route viable.

## Dependencies outside this repository

| Item | Needed for | Owner or place |
|---|---|---|
| An unlocked Zeebo and SD card | Phases 4 to 7 | the operator |
| Pads under test (Z-Pad, 8BitDo, DualShock 4, one plain HID pad) | Phases 5 to 7 | the operator |
| The HID headers (BREW MP SDK copy, reached by `zeebo_demoscene/vendor/inc`) | A-8 | present locally; whether they match the console is what G1 and G2 decide |
| The toolchain and packaging flow of `zsdk-src` and the SA1 port | Phases 3 and 4 | existing |
| zeebx with emulated pads and scripted sessions | Phase 3 | existing |
