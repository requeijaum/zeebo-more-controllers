# Implementation status

What exists, which requirement it serves, which test exercises it, and what is not built yet.
Updated with the code; a gap listed here is a plan, a gap missing from here is a surprise.

## Built (portable C99, host tested)

| Module | Files | Requirements | Tests |
|---|---|---|---|
| core: provenance values | `src/core/zhp_core.{h,c}` | IM-006, LQ-007 | CT-006 |
| core: event ring (256, overwrite oldest, dropped counter) | same | IM-001, LF-005, LQ-004 | CT-001 |
| core: device table (8, overflow counted, query bits) | same | IM-002, LF-001 | CT-002 |
| core: d-pad classifier (decision function only) | same | IM-003, LF-008 | CT-003 |
| core: hex dump | same | IM-005, LF-010 | CT-005 |
| core: session log `ZHP-LOG 1`, 160 characters per line, 64 KB cap | same | IM-004, LF-012, LF-014, LQ-009, HZ-4 | CT-004 |
| fs: path allowlist and save by temporary file | `src/fs/zhp_fs.{h,c}` | IM-009, IM-010, LQ-005, TR-007, CON-1, HZ-1 | CT-007, CT-008 |
| hidport: device card, event poll, raw probe, rumble pulse over a table of functions | `src/hidport/zhp_hid.{h,c}` | IM-007, IM-008, LF-003, LF-004, LF-007, LF-009, LF-011, TR-009, HZ-2 | CT-006, CT-009, CT-010 |

Run: `make test` (CT-001..010 under ASan and UBSan, all pass) and `make check-portable`.
The tests were also checked by mutation: seven deliberate bugs in the sources (ring drop count, path
prefix check, rumble stop boundary, error mapping, log truncation reserve, d-pad classes, NULL-interface
code) were each caught.

## Built: the applet and its glue

| File | What it is | Requirements |
|---|---|---|
| `src/brew/zhp_app.c` | BREW glue over IHID/IHIDDevice, list page, card page, event history, raw probe, rumble pulse, timer tick | LF-001..005, LF-009, LF-011, TR-004, TR-005, HZ-2 |
| `src/brew/zhp_libc.c`, `src/brew/shim/string.h` | the six libc functions the core uses (`-nostdinc` module, no libc) | TR-001 |
| `src/brew/zhp.bid` | `AEECLSID_ZHP = 0x0BE07100`, next to the ids already used in this workspace | TR-001 |
| `tools/build_applet.sh` | cross-build: BREW 4.0.2 headers, ARM1136, ZSDK `elf2mod`, writes `build/zhp.mod` | TR-001, TR-002 |
| `tools/gen_mif.py` | writes the 60-byte `.mif` that names the applet class | TR-002 |

Build: `bash tools/build_applet.sh` → `build/zhp.mod` (14.8 KB) and `build/zhp.mif`.

## Measured in zeebx 0.6.0 (emulator, **not** the console)

Run: `~/projects/zeebx-emu-copytex/target/release/zeebx run build/zhp.mod --seconds=3 --ports=pad
--key-names=500:select,900:select --dump-surfaces=DIR`

- `IHID_GetConnectedDevices` with type **0** returns 0 devices; with `AEEUID_HID_Joystick_Device`
  (`0x0106c3fd`) it returns 1: the emulated pad, `VID 0x1EAA / PID 0x0135`, `nDeviceType 0x0106c3fd`.
  With the keyboard and mouse UIDs, 0. So a wildcard query is not how a pad is found: enumerate per type.
- `IHIDDevice_GetNumberOfButtons` = 18; `GetDeviceStatus` = 1.
- The button-event queue empties with **`EFAILED` (1)**, not `AEE_ENOMORE` (47). The screen shows which code
  the console used (`empty queue answers N`), so the console settles it.
- `AEEHIDButtonInfo.nButtonUID` arrives correctly (`0x0106c402` for the Start key, `0x0106c40b` for B1).
  The timestamp field came back as **0** on every event.
- `QueryInterface(0x1088045 /* IHIDRawDevice */)` succeeds and hands back **the same pointer as the
  device** — an emulator stub, not a raw interface. The probe flags this as `AVAILABLE(alias)`; a real
  interface is a different object. This is why the probe reports the alias instead of a bare AVAILABLE.
- Rumble (`IHIDDevice_Rumble`) returns success in the emulator; it proves nothing about a motor.
- `--keys=` feeds the **pad** only (HID button events). `EVT_KEY` needs `--key-names=`; the Zeebo pad's
  `start` button arrives as `AVK_ZB_CONFIRM 0xe064`, not `AVK_SELECT`.

## Not built yet

| Item | Requirements | Unblocked by |
|---|---|---|
| `zhp_fs_ops` over IFileMgr (nothing is written yet: this build opens no file) | TR-007, LF-012 | the log page |
| Guided capture sequencing (prompts, per-direction observation, position-field tracking) | LF-006, LF-008 | the glue; the classifier is only its decision function |
| UI pages and navigation, GL text drawing | LF-015, IM-011, TR-003, LQ-006 | G1 (emulator) |
| App entry point, suspend and resume, Home | LF-016, IM-012 | G1 |
| Self-test page, build id | LF-017 | G1 |
| Writing the session log to the data folder and to the SD card | LF-012, LF-014 | the file layer above |
| `tools/zhp_log_to_json.py` | LF-014, IM-013, IT-006 | the log format is fixed (CT-004), so it can be written now |
| Cross-build, `.mod`/`.mif` packaging, objdump and symbol checks | TR-001, TR-002, TR-006, IT-001, IT-003 | G1 |
| Integration, system and acceptance tests | IT, ST, AT | the items above |

## Decisions and loose ends found while coding

- D-1: the d-pad classifier has a fifth class, PARTIAL, for "some directions but not all". With only four
  classes a half-seen pad would have been forced into NONE or a wrong class. Requirements and test plan
  were updated.
- D-2: log lines are cut at 160 characters and end with `~`; a log that would pass the limit ends with
  `TRUNCATED=1`. The cut can fall inside an escape sequence; the parser must treat a trailing `~` as "cut".
- D-3: `ZHP_RUMBLE_ON` (255) is a placeholder. The console's motor range is not verified; the probe should
  read it from the first measurement rather than trust this value.
- D-4: the rumble stop depends on the app calling `zhp_rumble_tick` every frame. A frozen UI would leave
  the motors on; the glue must also stop them on suspend and exit.
- D-5: `ZHP_HID_NOMORE` is an abstract code. The glue maps the console's "no more events" result to it.
