# ZeeHID Probe (ZHP) — Test plan

Right side of the V. Each level verifies the requirements of the same level in `REQUIREMENTS.md`:

| Level | Test IDs | Where it runs | Verifies |
|---|---|---|---|
| 4 Implementation | CT (component) | host, ASan + UBSan, fakes only | IM |
| 3 Technology | IT (integration) | cross-build, zeebx emulator, host scripts | TR |
| 2 Logical | ST (system) | the complete app in the emulator, scripted | LF, LQ |
| 1 Concept | AT (acceptance) | the real console, operator present | SN, CON |

Rule: a test at one level never replaces one at another. A passing emulator run is evidence about the
emulator (assumption A-6), not about the console.

## 1. Component tests (CT) — host

| ID | Procedure | Pass criterion | Verifies |
|---|---|---|---|
| CT-001 | Push 300 events into the ring of 256. | Last 256 kept in order; dropped = 44; no allocation after init (allocation counter). | IM-001, LQ-010 |
| CT-002 | Add nine devices to the table of eight. | Eight entries, an OVERFLOW marker, no overwrite of entry 0. | IM-002 |
| CT-003 | Feed the classifier four observation sets: buttons only, position only, both, none. | Returns DPAD_BUTTONS, POSITION_AXIS, BOTH, NONE respectively; same result on repeat. | IM-003, LF-008 |
| CT-004 | Format a golden event list, a 200-character value and a flood past 64 KB. | Output equals the golden file; lines at most 160 characters; the file ends with the truncation marker at 64 KB. | IM-004, LF-012, LF-014, LQ-009, HZ-4 |
| CT-005 | Hex dump of lengths 0, 1, 16, 17, 4096. | Equal to the golden text for each length. | IM-005, LF-010 |
| CT-006 | Inject an error code into every field source. | No field prints a number; each prints ERR(code). | IM-006, LQ-007 |
| CT-007 | Run the path table: data folder file, SD log file, `..` escape, absolute system path, empty path, very long path. | Accept only the first two; refusals counted. | IM-009, LQ-005, HZ-1 |
| CT-008 | Save with a fake file layer that fails at temporary write, remove, rename in turn. | The failing step is reported and the temporary file stays. | IM-010, TR-007, LF-013 |
| CT-009 | Drive the adapter with fakes returning each documented error, a dropped-events flag, and a rumble start. | ERR(code) shown; the indicator appears; rumble stops by 1.1 s and a second press is ignored. | IM-007, LF-007, LF-011, TR-009, HZ-2 |
| CT-010 | Probe raw access with a fake that has the interface and one that returns NULL; probe twice. | AVAILABLE and NOT_AVAILABLE(code); QueryInterface called once per device. | IM-008, LF-009, TR-005, TR-009 |

Command (once code exists): `make -C ports/zhp test SAN=1`. Exit 0 required; the sanitizer log must be empty.

## 2. Integration tests (IT) — cross-build and emulator

| ID | Procedure | Pass criterion | Verifies |
|---|---|---|---|
| IT-001 | Cross-build `zhp.mod` and `zhp.mif`; run objdump and nm on the image. | Build succeeds; no VFP mnemonic; no `.data` or `.bss` symbol of ours; the core also builds with the host compiler. | TR-001, TR-002, TR-006, LQ-010 |
| IT-002 | Run `zeebx run zhp.mod` with an emulated pad attached. | Main screen reached; the device list contains the emulated pad with the VID:PID the emulator announces. | TR-004, TR-008 |
| IT-003 | Symbol check on the image; render the list page in the emulator and dump the frame. | No `glTexParameteri` or `glTexEnvi`; text visible in the dump. | TR-003 |
| IT-004 | Attach two emulated devices of different kinds (for example a Z-Pad and a Boomerang). | Both listed with their own VID:PID; selecting each shows its own card. | LF-001, TR-004 |
| IT-005 | Run in the emulator, which has no raw interface. | The raw page shows NOT AVAILABLE(code), not blank. | LF-009, TR-010 |
| IT-006 | Run a session in the emulator, take the log from its file system, run `tools/zhp_log_to_json.py`. | JSON produced; a log with a corrupted line is rejected with its line number. | TR-007, LF-014, IM-013 |
| IT-007 | Run `tools/check_requirements.py` and review `data/SOURCES.md` against the tree. | Exit 0; no copied third-party code found; no vendored `AEEIHIDRawDevice.h`. | LQ-011, TR-005, CON-5 |

## 3. System tests (ST) — whole app in the emulator

| ID | Procedure | Pass criterion | Verifies |
|---|---|---|---|
| ST-001 | Scripted key session through every page (list, card, live, capture, raw, log, self-test, exit); screenshots at fixed times; runs with 0 to 4 devices. | Every page reached; list size equals device count; no text within 8 px of the border; exit code 0. | LF-001..004, LF-015, LQ-006, CON-2, IM-011 |
| ST-002 | Scripted press and axis sweep on the emulated pad during live view and capture. | Events appear in sent order with monotonic timestamps; extremes shown; directional class equals the emulator pad's. | LF-005, LF-006, LF-008 |
| ST-003 | Test build with the fake raw device and fake rumble. | Descriptor dump equals the fed bytes; rumble pulse of at most 1.1 s. | LF-009, LF-010, LF-011 |
| ST-004 | Run a full session with the file layer recording every path it is asked to write; also try to force a write to a system path through the test hook. | Only the data folder and SD log folder appear; the forced write is refused and counted. | SN-5, LQ-005, CON-1, LF-013, HZ-1 |
| ST-005 | Fault injection: every adapter call fails in turn; then all lookups fail. | Each failure shows ERR(code), never 0; the self-test page shows four ERR lines; the app does not exit. | LF-004, LF-007, LF-017, LQ-007, TR-010, HZ-3 |
| ST-006 | Run the golden session twice; mask the timestamp field; compare. | Both runs equal each other and the golden log. | LF-012, LQ-008 |
| ST-007 | Suspend and resume during live view, with a connect event while suspended. | Selected device and view restored; the new device listed. | LF-016, IM-012 |
| ST-008 | Live view for 60 s at the fixed tick; record frame times, heap and stack watermarks, event-to-screen delay. | 20 fps or better average; p95 frame time at most 66 ms; heap peak at most 4 MB; stack at most 16 KB; delay at most 2 frames. | LQ-001, LQ-002, LQ-003, TR-011, TR-012 |
| ST-009 | 500 connect and disconnect cycles; then 5 minutes at 100 events per second per device; then flood the log. | No crash or hang; heap growth at most 64 KB; log file 64 KB with the marker. | LQ-004, LQ-009, TR-012 |

## 4. Acceptance tests (AT) — real console

Operator runs these. Every run is recorded as in section 6; a run without its record is invalid.

| ID | Procedure | Pass criterion | Verifies |
|---|---|---|---|
| AT-001 | Install by the ZeeUtils flow and start ZHP from Appmgr; open the self-test page. | App starts; the page lists the four interfaces with result codes and a provenance tag on each value; the build id matches the build. | SN-6, SN-8, CON-4, TR-002, LF-017 |
| AT-002 | Plug a known Z-Pad directly. | One device card with its VID:PID; buttons and axes counts plausible. | SN-1, LF-001, LF-003 |
| AT-003 | Plug a HID pad that is not in `hid_devices.cfg`; hot-plug and unplug it. | The list changes within 1 s, or the card is absent and that fact is logged (decides A-4). | SN-1, LF-002 |
| AT-004 | Run the raw probe on the Z-Pad and on the other pad. | AVAILABLE or NOT AVAILABLE(code) recorded for each (decides A-2). | SN-3, LF-009 |
| AT-005 | Guided capture and live view on at least three pads; note the frame rate. | Button UID order, position fields and directional class recorded per pad; 20 fps or better in live view. | SN-2, LF-005, LF-006, LF-008, LQ-001 |
| AT-006 | Write the log; move the SD card to the PC; run the host parser. | The log parses; the app said which write path worked (decides A-3). | SN-4, LF-013 |
| AT-007 | Complete the mission profile with a Z-Pad only, on a TV. | Every step done without the PC; text readable inside the safe area. | SN-7, LF-015, LQ-006, CON-3 |

## 5. Gates

| Gate | Entry | Exit | Kill criterion |
|---|---|---|---|
| G0 | Requirements baseline. | `check_requirements.py` exits 0; CT-001..010 pass. | A requirement that cannot be given a method or criterion is deleted, not kept. |
| G1 | G0. | IT-001..007 and ST-001..009 pass; AT-001 passes (A-7 decided). | If the app does not start after two clean installs while ZeeUtils itself starts, stop and diagnose the packaging before any other work. |
| G2 | G1. | AT-002 passes (A-1, A-3 decided; A-6 compared). | If the known Z-Pad is not listed, the IHID route is wrong: stop and report. |
| G3 | G2. | AT-003 result recorded. | None; a negative changes the route (permissive cfg entry first) and is a deliverable. |
| G4 | G2. | AT-004 result recorded (A-2). | None; a negative closes the raw route and is a deliverable. |
| G5 | G3 or G4. | AT-005, AT-006, AT-007 pass (A-5 decided). | If no pad other than the Z-Pad can be captured, stop and report. |

Order follows information value: G0 and G1 are cheap and decisive about packaging; G2 decides the
whole interface; G3 and G4 are single-run answers that change the plan; G5 is the long measurement.

## 6. Observation channels and run record

Channels per run: the screen (photo or capture), the session log on the SD card, the emulator log for
ST and IT runs. An AT run without a screen record and a log is invalid.

Record per run (one file, `runs/YYYYMMDD-HHMM-<id>.txt`): console firmware, build id of ZHP, pad model,
VID:PID, direct or through a hub, SD card used, the single thing changed from the previous run, the
result against each pass criterion, and the files kept. One variable per run.

## 7. Traceability and what is not built yet

`tools/check_requirements.py` checks that every requirement has a method, an acceptance criterion and
existing tests, that every test verifies an existing requirement, and that every need and every logical
requirement is covered. Nothing in this plan has been run: no code exists yet.

| Item | Unblocked by |
|---|---|
| Component tests CT-001..010 | the core and fakes being written |
| Integration and system tests | the cross-build and the emulator scripts |
| Acceptance tests | G1 passing and a console with an unlocked firmware |
