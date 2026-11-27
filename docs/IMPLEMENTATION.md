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

## Not built yet

| Item | Requirements | Unblocked by |
|---|---|---|
| BREW glue: `zhp_hid_ops` over IHID, IHIDDevice and the raw interface; `zhp_fs_ops` over IFileMgr | TR-004, TR-005, TR-007 | G1 toolchain (walking skeleton) |
| Enumeration loop that queries each device-type UID and fills the table | LF-001, LF-002 | the glue above |
| Guided capture sequencing (prompts, per-direction observation, position-field tracking) | LF-006, LF-008 | the glue; the classifier is only its decision function |
| UI pages and navigation, GL text drawing | LF-015, IM-011, TR-003, LQ-006 | G1 (emulator) |
| App entry point, suspend and resume, Home | LF-016, IM-012 | G1 |
| Self-test page, build id | LF-017 | G1 |
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
