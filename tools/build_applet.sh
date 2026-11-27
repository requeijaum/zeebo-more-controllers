#!/usr/bin/env bash
# Build the ZeeHID Probe applet: build/zhp.mod + build/zhp.mif
# (BREW 4.0.2 SP19 headers, ARM1136, elf2mod from the ZSDK). Writes only under build/.
# The HID headers come from the vendor tree that the SA1 port compiles against; override the
# variables to point elsewhere.
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
SDKX="${SDKX:-$HOME/projects/zeebo-emulator/research/docs/sdk-extract}"
ZDEMO="${ZDEMO_VENDOR:-$HOME/projects/zeebo_demoscene/vendor}"
ZSDK="${ZSDK:-$HOME/projects/zsdk-src}"
B4="$SDKX/Zeebo SDK + BREW SDK 4.0.2 + BREW MP SDK/BREW-Simulator-brew_sdk/brew_sdk/sdk"
LDSCRIPT="$ZSDK/tools/elf2mod/elf2mod.x"
ELF2MOD="$ZSDK/tools/elf2mod/elf2mod.exe"
CRT="$ZSDK/third_party/brew_crt"
OUT="$HERE/build"
mkdir -p "$OUT/hdr"
for f in AEEIHID.h AEEIHIDDevice.h AEEHIDDevice_Joystick.h AEEHIDDevice_Keyboard.h AEEHIDDevice_Mouse.h AEEHID.bid; do
  [ -e "$ZDEMO/inc/$f" ] || { echo "missing $ZDEMO/inc/$f" >&2; exit 1; }
  ln -sf "$(readlink -f "$ZDEMO/inc/$f")" "$OUT/hdr/$f"
done
CFLAGS=(-c -Os -fno-builtin -ffreestanding -nostdinc -fno-common -march=armv6 -mcpu=arm1136j-s
        -mfloat-abi=soft -mapcs-frame -mno-thumb-interwork -DDYNAMIC_APP -DBREW_STATIC_APP -DAEE_SIMULATOR=0
        "-D__declspec(x)=" ${ZHP_DEFS:-}
        -I"$HERE/src/brew/shim" -I"$HERE/src/brew" -I"$OUT/hdr" -I"$B4/inc" -I"$ZDEMO/inc"
        -I"$(arm-none-eabi-gcc -print-file-name=include)")
OBJS=()
build() { local o="$OUT/$(basename "${1%.c}").o"; echo "CC $(basename "$1")"
  arm-none-eabi-gcc "${CFLAGS[@]}" "${@:2}" -o "$o" "$1"; OBJS+=("$o"); }
build "$CRT/AEEModGen.c"
build "$B4/src/AEEAppGen.c"
for c in src/core/zhp_core.c src/fs/zhp_fs.c src/hidport/zhp_hid.c src/brew/zhp_libc.c src/brew/zhp_app.c; do
  build "$HERE/$c" -Wall -Wextra
done
arm-none-eabi-ld --emit-relocs -T "$LDSCRIPT" -o "$OUT/zhp.elf" "${OBJS[@]}" \
  -L"$(dirname "$(arm-none-eabi-gcc -print-libgcc-file-name)")" -lgcc
WINEDEBUG=-all wine "$ELF2MOD" -output "$OUT/zhp.mod" "$OUT/zhp.elf"
python3 "$HERE/tools/gen_mif.py" "$OUT/zhp.mif" 0x0BE07100
ls -l "$OUT/zhp.mod"
