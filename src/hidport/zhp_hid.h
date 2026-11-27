/* ZeeHID Probe HID port: the HID calls behind a table of functions so the core and the UI run on
 * fakes. The BREW glue (IHID, IHIDDevice, the raw interface) will implement zhp_hid_ops.
 * Requirements: IM-007, IM-008, LF-003, LF-004, LF-007, LF-009, LF-011, LQ-007, TR-004, TR-005,
 * TR-009, HZ-2. Tests: CT-009, CT-010, CT-006. Portable C99. */
#ifndef ZHP_HID_H
#define ZHP_HID_H

#include <stddef.h>
#include <stdint.h>

#include "../core/zhp_core.h"

/* ops return 0 on success. next_button_event also uses ZHP_HID_NOMORE for "no event waiting".
 * Any other value is the console's error code and is shown as ERR(code). */
#define ZHP_HID_NOMORE 1
#define ZHP_E_NULL_IFACE (-1000) /* the call succeeded but returned a NULL interface */

typedef struct {
    int32_t type, pid, vid, bt; /* mirror of AEEHIDDeviceInfo */
} zhp_raw_info;

typedef struct {
    void *ctx;
    int (*get_info)(void *ctx, int32_t handle, zhp_raw_info *out);
    int (*get_num_buttons)(void *ctx, int32_t handle, int32_t *n);
    int (*get_status)(void *ctx, int32_t handle, int32_t *status);
    /* ask the device object for the raw HID interface. *has: 0 = nothing, 1 = a separate interface,
     * 2 = the call returned the device object itself, which is a stub, not a raw interface */
    int (*raw_query)(void *ctx, int32_t handle, int *has);
    int (*rumble)(void *ctx, int32_t handle, int32_t left, int32_t right);
    int (*next_button_event)(void *ctx, int32_t handle, int32_t *uid, int32_t *pressed,
                             uint32_t *ts_ms, int *dropped);
} zhp_hid_ops;

/* ---- LF-003, LF-004, IM-007: a device card. A failed call is ERR(code), never a number. ---- */
typedef struct {
    zhp_val vid, pid, type, bt, status, nbuttons;
} zhp_card;

void zhp_hid_card(const zhp_hid_ops *ops, int32_t handle, zhp_card *out);

/* ---- LF-009, IM-008: raw probe, once per device ---- */
typedef enum { ZHP_RAW_UNPROBED = 0, ZHP_RAW_AVAILABLE = 1, ZHP_RAW_NOT_AVAILABLE = 2 } zhp_raw_state;

typedef struct {
    zhp_raw_state state;
    int32_t code;   /* for NOT_AVAILABLE: the console's code, or ZHP_E_NULL_IFACE */
    uint8_t alias;  /* AVAILABLE, but QueryInterface handed back the device object: a stub */
} zhp_raw_result;

typedef struct {
    const zhp_hid_ops *ops;
    struct {
        int32_t handle;
        zhp_raw_result res;
    } cache[ZHP_MAX_DEVICES];
    uint8_t cached;
    uint8_t dropped_seen; /* LF-007: the console reported dropped events */
} zhp_hid;

void zhp_hid_init(zhp_hid *h, const zhp_hid_ops *ops);
zhp_raw_result zhp_hid_raw_probe(zhp_hid *h, int32_t handle);
/* "AVAILABLE", "AVAILABLE(alias)" or "NOT_AVAILABLE(-3)"; returns the length or -1. */
int zhp_raw_fmt(char *out, size_t cap, zhp_raw_result r);

/* ---- LF-005, LF-007: read one button event into the ring. 1 = pushed, 0 = none, -1 = error. ---- */
int zhp_hid_poll_button(zhp_hid *h, int32_t handle, zhp_ring *ring, int32_t *err_code);

/* ---- LF-011, HZ-2: rumble only on command, at most ZHP_RUMBLE_MS, stopped by tick ---- */
#define ZHP_RUMBLE_MS 1000u
#define ZHP_RUMBLE_ON 255 /* motor speed sent on press; the console's range is not verified */

typedef struct {
    uint8_t active;
    int32_t handle;
    uint32_t start_ms;
    int32_t stop_err; /* code returned by the last stop attempt, 0 if it worked */
} zhp_rumble;

/* 1 = started, 0 = ignored (a pulse is running), -1 = the console refused (code in *err) */
int zhp_rumble_press(zhp_rumble *r, const zhp_hid *h, int32_t handle, uint32_t now_ms, int32_t *err);
/* call every frame: stops the motors once ZHP_RUMBLE_MS have passed */
void zhp_rumble_tick(zhp_rumble *r, const zhp_hid *h, uint32_t now_ms);

#endif
