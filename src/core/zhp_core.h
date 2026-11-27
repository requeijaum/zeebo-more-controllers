/* ZeeHID Probe core: portable C99, no allocation, no I/O, no BREW headers.
 * Requirements: IM-001 (ring), IM-002 (device table), IM-003 (d-pad classifier), IM-004 (log),
 * IM-005 (hex dump), IM-006 (provenance). Tests: tests/test_main.c (CT-001..006). */
#ifndef ZHP_CORE_H
#define ZHP_CORE_H

#include <stddef.h>
#include <stdint.h>

/* ---- IM-006: provenance. A failed call is ERR(code) and never a number. ---- */
typedef enum { ZHP_P_MEASURED = 0, ZHP_P_DERIVED, ZHP_P_UNKNOWN, ZHP_P_ERR } zhp_prov;

typedef struct {
    int32_t v;      /* the value; for ZHP_P_ERR the error code */
    zhp_prov prov;
} zhp_val;

zhp_val zhp_val_measured(int32_t v);
zhp_val zhp_val_derived(int32_t v);
zhp_val zhp_val_unknown(void);
zhp_val zhp_val_err(int32_t code);

/* "123/M", "0x045E/M" (hex_digits > 0), "123/D", "UNKNOWN", "ERR(-5)".
 * Returns the length, or -1 if cap is too small (out is then empty). */
int zhp_val_fmt(char *out, size_t cap, zhp_val v, int hex_digits);

/* ---- IM-001: event ring, overwrite oldest, count dropped, no allocation ---- */
typedef enum { ZHP_EV_BTN_DOWN = 1, ZHP_EV_BTN_UP = 2, ZHP_EV_POS = 3 } zhp_ev_kind;

typedef struct {
    uint32_t ts_ms;
    int32_t dev;
    int32_t uid;
    int32_t value;
    uint8_t kind;
} zhp_event;

typedef struct {
    zhp_event *buf;
    uint32_t cap, head, count, dropped;
} zhp_ring;

void zhp_ring_init(zhp_ring *r, zhp_event *storage, uint32_t cap);
void zhp_ring_push(zhp_ring *r, const zhp_event *ev);
int zhp_ring_pop(zhp_ring *r, zhp_event *out); /* oldest first; 1 = got one */

/* ---- IM-002: device table of 8; the ninth is counted, not ignored ---- */
#define ZHP_MAX_DEVICES 8

typedef struct {
    int32_t handle;
    uint8_t query_mask; /* which device-type queries returned it (bit n = query n) */
    zhp_val vid, pid, type, bt, status;
} zhp_dev;

typedef struct {
    zhp_dev dev[ZHP_MAX_DEVICES];
    uint8_t used[ZHP_MAX_DEVICES];
    uint32_t overflow; /* devices refused because the table was full */
} zhp_devtab;

void zhp_devtab_init(zhp_devtab *t);
int zhp_devtab_add(zhp_devtab *t, int32_t handle, uint8_t query_bit); /* slot, or -1 if full */
int zhp_devtab_find(const zhp_devtab *t, int32_t handle);             /* slot or -1 */
int zhp_devtab_remove(zhp_devtab *t, int32_t handle);                 /* 1 if removed */
uint32_t zhp_devtab_count(const zhp_devtab *t);

/* ---- IM-003: directional-pad classifier ---- */
typedef enum { ZHP_DIR_UP = 0, ZHP_DIR_DOWN, ZHP_DIR_LEFT, ZHP_DIR_RIGHT, ZHP_DIR_COUNT } zhp_dir;

typedef struct {
    uint8_t btn_event; /* a d-pad button UID event was seen while this direction was pressed */
    uint8_t pos_event; /* a position field moved while this direction was pressed */
} zhp_dir_obs;

typedef enum {
    ZHP_DPAD_NONE = 0,
    ZHP_DPAD_BUTTONS,  /* all four directions as buttons only */
    ZHP_DPAD_POSITION, /* all four directions as position only */
    ZHP_DPAD_BOTH,     /* all four both ways */
    ZHP_DPAD_PARTIAL   /* some directions but not all: reported, never guessed */
} zhp_dpad_class;

zhp_dpad_class zhp_dpad_classify(const zhp_dir_obs obs[ZHP_DIR_COUNT]);
const char *zhp_dpad_name(zhp_dpad_class c);

/* ---- IM-005: hex dump, 16 bytes per line: "0000: 05 01 ...\n" ---- */
/* Returns characters written (without the NUL), or -1 if cap is too small or len > 65536. */
int zhp_hexdump(const uint8_t *data, size_t len, char *out, size_t cap);

/* ---- IM-004: session log "ZHP-LOG 1", KEY=VALUE tokens, 160 characters per line, 64 KB cap ---- */
#define ZHP_LOG_MAX_LINE 160
#define ZHP_LOG_TRUNC_MARK "TRUNCATED=1\n"

typedef struct {
    char *buf;
    size_t cap;     /* size of buf */
    size_t max;     /* file size limit (65536 for a save file) */
    size_t len;
    uint32_t lines;
    uint8_t truncated;
} zhp_log;

typedef struct {
    const char *key; /* [A-Z0-9_.]+ */
    const char *val; /* escaped on output */
} zhp_kv;

/* cap must be >= max. */
void zhp_log_init(zhp_log *l, char *buf, size_t cap, size_t max);
int zhp_log_header(zhp_log *l, const char *build_id);
/* 1 = written, 0 = dropped because the log is truncated, -1 = bad argument. */
int zhp_log_line(zhp_log *l, const char *tag, const zhp_kv *kv, size_t n);

#endif
