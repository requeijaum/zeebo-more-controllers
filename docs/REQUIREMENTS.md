# ZeeHID Probe (ZHP) — Requirements

Scope: a BREW applet for the Zeebo that shows which USB HID devices the console exposes and how it
delivers their input, so that new controllers can be added to `hid_devices.cfg` (or a bridge) on
measured data instead of guesses. This first release is a **read-only instrument**: it never edits a
system file.

Structure follows the V model. Left side = requirements by level; right side = tests by level
(`TEST-PLAN.md`):

| Level | Requirement IDs | Verified by (right side of the V) |
|---|---|---|
| 1 Concept (needs, ConOps) | SN, CON | AT (acceptance on the real console) |
| 2 Logical (functions, quality) | LF, LQ | ST (system tests of the whole app) |
| 3 Technology (platform, tools) | TR | IT (integration tests) |
| 4 Implementation (modules) | IM | CT (component tests on the host) |

Every requirement carries a method (I inspection, A analysis, D demonstration, T test) and an acceptance
criterion. Numeric targets are design proposals; the gate named in `TEST-PLAN.md` is where each stops
being a proposal. Facts the design rests on are listed in section 7 with their source; beliefs that have
not been measured are in section 6.

## 1. Level 1 — Concept

### 1.1 ConOps

Operator: a developer or tester with an unlocked Zeebo, an SD card, a USB hub, the pads under test and a
TV (CRT safe area applies). Mission profile:

1. Build `zhp.mod` and `zhp.mif` on the PC.
2. Copy `mif/` and `mod/` to the SD root; on the console run EMAPPLET, Field Test, Memory Copy; reboot
   (the same flow ZeeUtils uses).
3. Start ZHP from Appmgr (or the Z-Wheel).
4. Plug a pad. A device card appears with VID, PID and the capabilities the console reports.
5. Press every control; the live view shows what the console delivers (button UIDs, position fields).
6. Run the guided capture and the raw probe. The probe says plainly whether raw reports are reachable.
7. Write the session log. Take the SD card to the PC and feed the log to the repo tools.

### 1.2 Stakeholder needs

| ID | Need | Method | Acceptance | Tests |
|---|---|---|---|---|
| SN-1 | See which USB HID devices the console exposes and their identity. | D | AT-002 and AT-003 show the device list for a known pad and for a pad outside the cfg. | AT-002, AT-003 |
| SN-2 | See live input exactly as the console delivers it, to learn the mapping of any pad. | D | AT-005 records buttons, axes and hat behaviour of at least 3 pads. | AT-005 |
| SN-3 | Know whether raw HID report access exists and what it allows. | D | AT-004 records AVAILABLE or NOT AVAILABLE(code) on the console. | AT-004 |
| SN-4 | Keep results as comparable files that the repo tools can read. | D | AT-006 exports logs that the host parser reads. | AT-006 |
| SN-5 | Change nothing on the console beyond the app's own data. | A | The write allowlist is enforced in code and proven by ST-004. | ST-004 |
| SN-6 | Develop and regress without the console, with the same source. | D | AT-001 installs on the console an image built from the same source that the emulator regression ran. | AT-001 |
| SN-7 | Operate with the console's own controller on a TV. | D | AT-007 completes the mission profile with a Z-Pad only. | AT-007 |
| SN-8 | Tell measured values from assumed ones. | D | On the console, the self-test page shows every value with its provenance tag (LQ-007), and a failed interface shows ERR(code). | AT-001 |

### 1.3 Concept constraints and exclusions

| ID | Statement | Method | Acceptance | Tests |
|---|---|---|---|---|
| CON-1 | Out of scope: writing `hid_devices.cfg` or any file outside the app's data and the SD log folder. | I | No code path opens another path for writing (IM-009). | ST-004 |
| CON-2 | Out of scope: vendor-class USB devices (Xbox 360/One), Bluetooth pads, USB device mode (Bluetooth is unproven either way, see A-9). | I | The UI lists only what IHID reports and never claims support for these. | ST-001 |
| CON-3 | Out of scope: delivering input to other applications. | I | No resident or background component exists. | AT-007 |
| CON-4 | Needs an unlocked console. | I | The install procedure in 1.1 names the requirement and ZHP adds no unlock step. | AT-001 |
| CON-5 | No code copied from GPL/AFL/Qualcomm sources. Data (IDs, offsets, prototypes) is recorded with its origin. | I | `data/SOURCES.md` lists each source; a review finds no copied code. | IT-007 |

## 2. Level 2 — Logical requirements

### 2.1 Functional

| ID | Requirement | Method | Acceptance | SN | Tests |
|---|---|---|---|---|---|
| LF-001 | At start the app lists the devices returned by `IHID_GetConnectedDevices`, asked once per known device-type UID (Joystick `0x0106c3fd`, Keyboard `0x0106c3fc`, Mouse) and once with type 0, and shows which query returned each device. | T | With N=0..4 emulated devices attached before launch, the list has N entries; each entry names the query that returned it. | SN-1 | IT-004, ST-001, AT-002 |
| LF-002 | Connect and disconnect events update the list without restarting the app. | T | After a scripted connect and disconnect the list reflects the change within 1 s (LQ-002). | SN-1 | ST-001, AT-003 |
| LF-003 | Each device card shows handle, VID, PID, device type, the Bluetooth flag (`bBluetoothDevice`) and status as returned by the console. | T | The values equal the ones configured in the emulator for that device. | SN-1 | ST-001, AT-002 |
| LF-004 | The card shows the number of buttons and the axes with minimum and maximum. | T | Values equal the emulator's configured values; a call that fails shows ERR(code), not a number. | SN-1 | ST-001, ST-005 |
| LF-005 | The live view shows button events with UID, pressed or released, timestamp and order. | T | A scripted press sequence appears in the same order with monotonic timestamps. | SN-2 | ST-002, AT-005 |
| LF-006 | The live view shows the position fields (nX, nY, nZ, nRx, nRy, nRz) with observed minimum and maximum. | T | A scripted axis sweep shows the extremes sent. | SN-2 | ST-002, AT-005 |
| LF-007 | When the console reports dropped events the live view shows it. | T | A fake adapter that sets the dropped flag makes the indicator appear. | SN-2 | CT-009, ST-005 |
| LF-008 | Guided capture: the operator presses each control in turn; the app records which button UIDs and which position fields changed and classifies the directional pad as DPAD_BUTTONS, POSITION_AXIS, BOTH or NONE, or PARTIAL when only some of the four directions show up (never guessed). | T | The classifier returns the expected class on the scripted observation sets (CT-003) and on the emulator pad (ST-002). | SN-2 | CT-003, ST-002, AT-005 |
| LF-009 | The raw probe asks the device object for the raw HID interface and shows AVAILABLE or NOT AVAILABLE(code); it never stays silent. | T | A fake with and a fake without the interface produce the two messages. | SN-3 | CT-010, IT-005, ST-003, AT-004 |
| LF-010 | If raw access exists: dump the report descriptor in hex and show the last raw report per report id. | T | With a fake raw device the dump equals the fed bytes. | SN-3 | CT-005, ST-003 |
| LF-011 | Rumble test only on operator command, bounded to 1 s and stopped automatically. | T | A fake records one start and one stop within 1.1 s; a second press during the pulse is ignored. | SN-2 | CT-009, ST-003 |
| LF-012 | A session log (`ZHP-LOG 1`) records header, device cards, events, raw dumps, probe results and every failed call. | T | The golden session produces the golden log (LQ-008). | SN-4 | CT-004, ST-006 |
| LF-013 | The log goes to the app data folder and, if writable, to an SD log folder; the app says which succeeded. | T | With the SD fake read-only, the data folder write succeeds and the SD failure is reported. | SN-4 | CT-008, ST-004, AT-006 |
| LF-014 | Log lines are `KEY=VALUE` tokens that a host script parses without ambiguity. | T | `tools/zhp_log_to_json.py` round-trips the golden logs. | SN-4 | CT-004, IT-006 |
| LF-015 | The whole mission profile is operable with the Z-Pad or the keypad: select, open, capture, probe, rumble, write log, exit; Home (AVK_CLR) leaves. | D | The scripted key session of ST-001 reaches every screen. | SN-7 | ST-001, AT-007 |
| LF-016 | Suspend and resume keep the state; pad connect and disconnect while suspended are applied on resume. | T | After suspend and resume the selected device and the live view are as before. | SN-7 | ST-007 |
| LF-017 | A self-test page shows build id, availability of IHID, IHIDDevice, the raw interface and IFileMgr, and free heap. | T | The page lists the four interfaces with their result codes. | SN-8 | ST-005, AT-001 |

### 2.2 Quality (non-functional)

| ID | Requirement | Method | Acceptance | SN | Tests |
|---|---|---|---|---|---|
| LQ-001 | Responsiveness: the live view redraws at 20 fps or better on average, with 66 ms frame time at the 95th percentile. | T | Emulator measure ST-008; console measure AT-005. | SN-7 | ST-008, AT-005 |
| LQ-002 | Latency: an event shows on screen at most 2 frames after the app reads it; a list change within 1 s of the console's event. | T | The timestamp pair in the log shows 2 frames or fewer. | SN-2 | ST-008 |
| LQ-003 | Memory: peak heap at most 4 MB and stack use at most 16 KB of the 32 KB stack. | T | The watermark report of ST-008 stays below both. | SN-6 | ST-008 |
| LQ-004 | Robustness: 500 connect and disconnect cycles and 5 minutes at 100 events per second per device end without crash, hang, or heap growth above 64 KB. | T | After the cycles and the flood the app still responds to the keypad, the heap watermark grew by at most 64 KB, and no fault is logged. | SN-8 | ST-009 |
| LQ-005 | Reversibility: writes only inside the app data folder and the SD log folder; any other path is refused and counted. | T | The path recorder of ST-004 sees no other write path. | SN-5 | CT-007, ST-004 |
| LQ-006 | Readability on a CRT: text stays inside the 8 px safe area; glyph height at least 16 px at 640x480. | I | A screenshot analysis finds no pixel of text within 8 px of the border. | SN-7 | ST-001, AT-007 |
| LQ-007 | Provenance: every displayed or logged value is tagged MEASURED, DERIVED, UNKNOWN or ERR(code); an error is never shown as zero. | T | Fault injection returns errors for every call; none appears as 0 (CT-006, ST-005). | SN-8 | CT-006, ST-005 |
| LQ-008 | Reproducibility: the same scripted session yields a byte-identical log once the timestamp field is masked. | T | ST-006 compares two runs and the golden file. | SN-4 | ST-006 |
| LQ-009 | One log file never exceeds 64 KB; beyond that it ends with a truncation marker. | T | A flood of events produces a 64 KB file with the marker. | SN-4 | CT-004, ST-009 |
| LQ-010 | The portable core builds and passes its unit tests on the host under ASan and UBSan, from the same source as the Zeebo build. | T | CT suite passes; IT-001 builds the Zeebo image. | SN-6 | CT-001, IT-001 |
| LQ-011 | Every requirement traces to at least one test and every test to a requirement. | I | `tools/check_requirements.py` exits 0. | SN-8 | IT-007 |

## 3. Level 3 — Technology requirements

| ID | Requirement | Method | Acceptance | Satisfies | Tests |
|---|---|---|---|---|---|
| TR-001 | Language C for the applet; the core is pure C with no BREW header. Soft-float, ARM1136 target, no VFP instruction in the image. | I | An objdump of the image finds no VFP mnemonic; the core compiles with the host compiler. | LQ-010 | IT-001 |
| TR-002 | Package as `.mod` and `.mif` with its own class id, using the existing elf2mod flow; install by the ZeeUtils flow (SD `mif/` and `mod/`, Field Test, Memory Copy, reboot). | D | AT-001 installs and starts the app by that flow. | SN-6, CON-4 | IT-001, AT-001 |
| TR-003 | Render with OpenGL ES 1.0 only. Use `glTexParameterx` and `glTexEnvx`; never an ES 1.1-only entry point (the SDK wrapper turns them into silent no-ops when the 1.1 interface is NULL, as measured on one unit). Text from a bitmap font atlas no larger than 256x256 drawn with `GL_OES_draw_texture`. | I | A symbol check finds no `glTexParameteri` or `glTexEnvi` in the image; text appears in the emulator. | LF-015, LQ-006 | IT-003 |
| TR-004 | Input through IHID and IHIDDevice (devices opened by handle, ISignal for events) polled from the app's own timer; no global input state is assumed. | I | The adapter source calls only those interfaces (IM-007). | LF-001, LF-005 | IT-002, IT-004 |
| TR-005 | The raw interface is obtained at run time by QueryInterface and may be NULL. Its declaration is written by us from the prototypes; no Qualcomm header is vendored (CON-5). | I | The build has no copy of `AEEIHIDRawDevice.h`; a NULL result degrades to NOT AVAILABLE. | LF-009, CON-5 | CT-010, IT-007 |
| TR-006 | All mutable state lives in one heap-allocated context; no writable global or static data in the image. | I | `nm` of the image lists no `.data` or `.bss` symbol of ours. | LQ-003 | IT-001 |
| TR-007 | Files through IFileMgr. Save by temporary file, remove destination, rename (rename fails if the destination exists). The data folder is `/mod/<id>/udata/`; the SD path is tried at run time and reported. | T | CT-008 covers each failing step; AT-006 shows which path worked. | LF-013, LQ-005 | CT-008, IT-006 |
| TR-008 | Regress in the HLE emulator with a scripted session (keys, screenshots, dumped files); emulated devices limited to the ones it implements. | D | ST-001 runs unattended and exits 0. | SN-6 | IT-002 |
| TR-009 | A host harness drives the adapter through fake IHID, IHIDDevice and raw vtables with injectable results. | I | CT-009 and CT-010 use only the fakes. | LQ-007 | CT-009, CT-010 |
| TR-010 | Every interface lookup may return NULL; each feature degrades with an explicit message and never aborts. | T | A run with all lookups failing reaches the self-test page and shows four ERR lines. | LQ-007 | IT-005, ST-005 |
| TR-011 | Timing from the shell's millisecond counter and from event timestamps; a fixed timer period for the UI tick. | A | The tick period is a named constant used in LQ-001 and LQ-002 measurements. | LQ-001 | ST-008 |
| TR-012 | Console limits respected: 32 KB stack, 32 MB heap shared with the binary, saves at most 64 KB. | T | LQ-003 and LQ-009 pass. | LQ-003, LQ-009 | ST-008, ST-009 |

## 4. Level 4 — Implementation requirements (modules)

Modules: **core** (portable C: ring, device table, classifier, formatter), **hidport** (BREW adapter that
exposes the HID calls as a table of functions), **fs** (path-checked file layer), **ui** (GL drawing and
navigation), **app** (entry point, shell events, suspend and resume), **tools** (host log parser).

| ID | Module | Requirement | Method | Acceptance | Satisfies | Tests |
|---|---|---|---|---|---|---|
| IM-001 | core | Event ring of 256 entries, overwrite oldest, count dropped, no allocation after init. | T | 300 pushes keep the last 256 and report 44 dropped. | LF-005, LQ-004 | CT-001 |
| IM-002 | core | Device table of 8 entries; a ninth device is shown as OVERFLOW, not ignored. | T | Nine adds yield eight entries and an OVERFLOW marker. | LF-001 | CT-002 |
| IM-003 | core | Directional-pad classifier: from the capture observations return DPAD_BUTTONS, POSITION_AXIS, BOTH, NONE or PARTIAL, deterministically. | T | The truth table of CT-003 passes. | LF-008 | CT-003 |
| IM-004 | core | Log formatter writes `ZHP-LOG 1` lines of at most 160 characters, `KEY=VALUE` tokens, values escaped, ending in a truncation marker at 64 KB. | T | Golden output matches; a 200-character value is wrapped or cut with a marker. | LF-012, LF-014, LQ-009 | CT-004 |
| IM-005 | core | Hex dump of 0 to 4096 bytes in a fixed format. | T | Length 0, 1, 16, 17 and 4096 match the golden text. | LF-010 | CT-005 |
| IM-006 | core | Every value carries a provenance tag; a failed call yields ERR(code) and no numeric value. | T | CT-006 injects errors on each field; none prints a number. | LQ-007 | CT-006 |
| IM-007 | hidport | Wrap IHID, IHIDDevice and the raw interface behind function pointers; every result code is captured and mapped to ERR(code). | T | Fakes returning each documented error show ERR(code). | TR-004, LQ-007 | CT-009 |
| IM-008 | hidport | The raw probe returns AVAILABLE or NOT_AVAILABLE(code) once per device and caches it. | T | Two probes call QueryInterface once. | LF-009 | CT-010 |
| IM-009 | fs | Allowlist: open for writing only under the data folder or the configured SD log folder; reject `..` and every other prefix; count refusals. | T | The path table of CT-007 gives the expected accept and refuse for each case. | LQ-005, CON-1 | CT-007 |
| IM-010 | fs | Save by temporary file, remove, rename; a failure leaves the temporary file and reports the step. | T | A fake failing at each of the three steps reports that step and keeps the temporary file. | TR-007 | CT-008 |
| IM-011 | ui | Pages List, Card, Live, Capture, Raw, Log and SelfTest; all text inside the safe area. | I | A render of each page has no text pixel within 8 px of the border. | LF-015, LQ-006 | ST-001 |
| IM-012 | app | Home leaves; suspend frees GL objects; resume rebuilds them and reapplies pending connect events. | T | After suspend and resume the GL objects are rebuilt (the list page draws again) and a connect event received while suspended appears in the list. | LF-016 | ST-007 |
| IM-013 | tools | `tools/zhp_log_to_json.py` converts a log to JSON and rejects a malformed line with its number. | T | Golden logs convert; a corrupted line is reported. | LF-014 | IT-006 |

## 5. Hazards and interlocks

| ID | Hazard | Interlock | Requirement | Test |
|---|---|---|---|---|
| HZ-1 | The app writes outside its own area (for example a system file). | Path allowlist in the file layer; no function that writes the cfg is compiled in. | IM-009, LQ-005 | CT-007, ST-004 |
| HZ-2 | Rumble left on. | 1 s bound and automatic stop. | LF-011 | CT-009 |
| HZ-3 | A fault in a lookup or call stops the app. | Every lookup may be NULL and every call result is checked. | TR-010 | ST-005 |
| HZ-4 | Log fills the save area. | 64 KB cap with truncation marker. | LQ-009 | CT-004 |

Kill criteria for the project are in `TEST-PLAN.md`, section 5.

## 6. Assumptions (each names the gate that decides it)

| ID | Assumption | Decided at | If false |
|---|---|---|---|
| A-1 | IHID and IHIDDevice on the console behave as the SDK headers document. | G2 | Fix the adapter; if the interfaces are absent the project stops. |
| A-2 | The raw HID interface can be obtained from the device object. | G4 | Raw access is not available to apps: init-needing pads (DS3, Switch Pro) go to the bridge route. |
| A-3 | An app can write to the SD card. | G2 | Logs stay in the data folder; retrieval uses the console's backup to external media (ZeeUtils feature) or reading from the screen. |
| A-4 | A pad whose VID:PID is not in `hid_devices.cfg` still appears as an IHID device, under some device-type query (the header does not document a wildcard, and the SA1 port and ZSDK only ask for Joystick). | G3 | A permissive cfg entry must be installed first (ZeeUtils copy) and the app measures from there. |
| A-5 | For pads whose cfg entry maps them, the directional pad arrives as DPad button UIDs. | G5 | Record the real behaviour; the cfg generator treats the hat as unsupported. |
| A-6 | The emulator's IHID behaviour matches the console for the pads it implements. | G2 vs ST-001 | Emulator results are used for regression only, never as evidence of console behaviour. |
| A-7 | Text drawn with `GL_OES_draw_texture` is readable on the console. | G1 | Fall back to the shell's IDisplay text path. |
| A-8 | The struct layouts and slot order of IHID, IHIDDevice and `AEEHIDDeviceInfo` (nDeviceType, wProductID, wVendorID, bBluetoothDevice) on the console equal the BREW MP SDK headers the SA1 port and ZSDK compile against. | G1 and G2 | Log the values read at run time next to the expected sizes; if they differ, rebuild against whatever layout the console shows. |
| A-9 | The console offers no usable Bluetooth HID host. | G3 | If a pad appears with the Bluetooth flag set, the Bluetooth route reopens and CON-2 is revised. |

## 7. Evidence annex (what the design rests on)

- IHID, IHIDDevice and the UID tables: headers `AEEIHID.h`, `AEEIHIDDevice.h`, `AEEHIDDevice_Joystick.h`. The copy read
  for this baseline is the one in the BREW MP SDK 7.12.5 (`AEEHIDDeviceInfo` = nDeviceType, wProductID,
  wVendorID, bBluetoothDevice). The Zeebo SDK package 1.2.4 carries no HID header; the SA1 port builds against
  these same MP SDK files through `zeebo_demoscene/vendor/inc` (symlinks; `AEEIHIDDevice.h` there is
  byte-identical to the MP SDK copy). Whether the console's layouts equal them is assumption A-8.
  `ports/zeebo/zhid.c` and `zsdk-src/src/platform/zeebo/zeebo_input.cpp` use these interfaces.
- Raw interface prototypes (`SetReport`, `GetReport`, `GetDescriptor`, `RegisterForReports`,
  `GetReportEvent`): `AEEIHIDRawDevice.h` in the BREW MP SDK 7.12.5 only. Not found in the Zeebo SDK
  extracts nor in the 1.1.2 firmware strings (the six "hidraw" hits there are graphics files). Treated as
  unknown (A-2).
- Console GL: one measured unit reports ES 1.0-CM, ES 1.1 interface NULL, texture size 1024, two texture
  units, `GL_OES_draw_texture` announced; the SDK wrapper makes 1.1-only entry points do nothing
  when the 1.1 interface is NULL (`GLES_1x_support.h`).
- Bluetooth: the 1.1.2 firmware carries a Bluetooth stack (radio manager, host controller, service discovery;
  strings such as `BT RM: Paired BDA`, `BT SD: Bad HID Class Descriptor`). No string for a Bluetooth HID
  profile (`AEEBT_HID`, `BTHID`) was found. Whether the radio exists or is usable by apps is unknown (A-9).
- Install flow, ZeeUtils copying `hid_devices.cfg`, backup to external media: the O3 Labs ZeeUtils page.
- Limits (stack, heap, saves, safe area, no static data guidance): Zeebo Developer Guide 0.97 as
  summarised in the ZSDK notes; emulator does not enforce them.
- Emulated pads and input scripting: zeebx documentation and the `zeebx session --keys` interface.
- Clean-room: `data/SOURCES.md`.

## 8. Not built yet

| Item | Needs |
|---|---|
| Everything above | This is a requirements baseline; no code exists. |
| Writing `hid_devices.cfg` from the console | A separate hazards review and its own gate, after G5. |
| Raw `SetReport` for init-needing pads | A-2 decided true at G4. |
| Bridge route | Decision after G4 and G5. |
