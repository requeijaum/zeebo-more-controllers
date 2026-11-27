/* Component tests CT-001..CT-010 (docs/TEST-PLAN.md). Host only, fakes only. */
#include <string.h>

#include "../src/core/zhp_core.h"
#include "../src/fs/zhp_fs.h"
#include "../src/hidport/zhp_hid.h"
#include "ztest.h"

/* ---------------------------------------------------------------- CT-001 */
static void CT_001_ring_overflow(void)
{
    zhp_event store[256];
    zhp_ring r;
    zhp_event e, out;
    uint32_t i, expect = 44, n = 0;
    zhp_ring_init(&r, store, 256);
    memset(&e, 0, sizeof(e));
    for (i = 0; i < 300; i++) {
        e.ts_ms = i;
        zhp_ring_push(&r, &e);
    }
    CHECK(r.count == 256);
    CHECK(r.dropped == 44);
    while (zhp_ring_pop(&r, &out)) {
        CHECK(out.ts_ms == expect); /* oldest kept is #44, order preserved */
        expect++;
        n++;
    }
    CHECK(n == 256);
    CHECK(expect == 300);
    CHECK(zhp_ring_pop(&r, &out) == 0);
}

/* ---------------------------------------------------------------- CT-002 */
static void CT_002_devtab_overflow(void)
{
    zhp_devtab t;
    int i, slot;
    zhp_devtab_init(&t);
    for (i = 0; i < 8; i++) {
        CHECK(zhp_devtab_add(&t, 100 + i, 1) == i);
    }
    CHECK(zhp_devtab_count(&t) == 8);
    CHECK(t.dev[0].handle == 100);
    slot = zhp_devtab_add(&t, 200, 1);
    CHECK(slot == -1);
    CHECK(t.overflow == 1);
    CHECK(zhp_devtab_count(&t) == 8);
    CHECK(t.dev[0].handle == 100); /* entry 0 not overwritten */
    /* the same handle again merges its query bit, it is not a new device */
    CHECK(zhp_devtab_add(&t, 103, 2) == 3);
    CHECK(t.dev[3].query_mask == ((1u << 1) | (1u << 2)));
    CHECK(zhp_devtab_remove(&t, 105) == 1);
    CHECK(zhp_devtab_add(&t, 201, 0) == 5); /* the freed slot is reused */
}

/* ---------------------------------------------------------------- CT-003 */
static zhp_dpad_class classify(int nb, int np)
{
    zhp_dir_obs o[ZHP_DIR_COUNT];
    int i;
    for (i = 0; i < ZHP_DIR_COUNT; i++) {
        o[i].btn_event = (uint8_t)(i < nb);
        o[i].pos_event = (uint8_t)(i < np);
    }
    return zhp_dpad_classify(o);
}

static void CT_003_dpad_truth_table(void)
{
    CHECK(classify(4, 0) == ZHP_DPAD_BUTTONS);
    CHECK(classify(0, 4) == ZHP_DPAD_POSITION);
    CHECK(classify(4, 4) == ZHP_DPAD_BOTH);
    CHECK(classify(0, 0) == ZHP_DPAD_NONE);
    CHECK(classify(2, 0) == ZHP_DPAD_PARTIAL);
    CHECK(classify(4, 2) == ZHP_DPAD_PARTIAL);
    CHECK(classify(0, 3) == ZHP_DPAD_PARTIAL);
    CHECK(classify(4, 0) == classify(4, 0)); /* deterministic */
    CHECK(strcmp(zhp_dpad_name(ZHP_DPAD_BOTH), "BOTH") == 0);
}

/* ---------------------------------------------------------------- CT-004 */
static void CT_004_log_format(void)
{
    static char buf[70000];
    zhp_log l;
    zhp_kv kv[3];
    char big[300];
    const char *golden = "ZHP-LOG 1 BUILD=t1\nDEV H=3 VID=0x1EAA/M NAME=Pad\\x20One\n";
    size_t i;
    int rc;
    char *nl;

    zhp_log_init(&l, buf, sizeof(buf), 65536);
    CHECK(zhp_log_header(&l, "t1") == 1);
    kv[0].key = "H"; kv[0].val = "3";
    kv[1].key = "VID"; kv[1].val = "0x1EAA/M";
    kv[2].key = "NAME"; kv[2].val = "Pad One";
    CHECK(zhp_log_line(&l, "DEV", kv, 3) == 1);
    buf[l.len] = '\0';
    CHECK(strcmp(buf, golden) == 0);

    /* a 200-character value: the line is cut at 160 characters and ends with '~' */
    memset(big, 'A', 200);
    big[200] = '\0';
    kv[0].key = "V"; kv[0].val = big;
    CHECK(zhp_log_line(&l, "LONG", kv, 1) == 1);
    nl = buf + l.len - 1;
    CHECK(*nl == '\n');
    {
        char *start = nl;
        while (start > buf && start[-1] != '\n') {
            start--;
        }
        CHECK((size_t)(nl - start) == ZHP_LOG_MAX_LINE);
        CHECK(nl[-1] == '~');
    }

    /* escaping: control bytes and backslash never reach the file raw */
    kv[0].key = "RAW"; kv[0].val = "a\tb\\c\nd";
    CHECK(zhp_log_line(&l, "ESC", kv, 1) == 1);
    buf[l.len] = '\0';
    CHECK(strstr(buf, "RAW=a\\x09b\\x5Cc\\x0Ad\n") != NULL);

    /* flood past 64 KB: the file never exceeds the limit and ends with the marker */
    kv[0].key = "K"; kv[0].val = "0123456789";
    for (i = 0; i < 10000; i++) {
        rc = zhp_log_line(&l, "FLOOD", kv, 1);
        if (rc == 0) {
            break;
        }
    }
    CHECK(rc == 0);
    CHECK(l.truncated == 1);
    CHECK(l.len <= 65536);
    CHECK(l.len >= sizeof(ZHP_LOG_TRUNC_MARK) - 1);
    CHECK(memcmp(buf + l.len - (sizeof(ZHP_LOG_TRUNC_MARK) - 1), ZHP_LOG_TRUNC_MARK,
                 sizeof(ZHP_LOG_TRUNC_MARK) - 1) == 0);
    CHECK(zhp_log_line(&l, "AFTER", kv, 1) == 0); /* later lines are dropped */
    CHECK(l.len <= 65536);
}

/* ---------------------------------------------------------------- CT-005 */
static void CT_005_hexdump(void)
{
    uint8_t d[4096];
    static char out[16000];
    size_t i;
    int n;
    for (i = 0; i < sizeof(d); i++) {
        d[i] = (uint8_t)(i * 7u + 1u);
    }
    n = zhp_hexdump(d, 0, out, sizeof(out));
    CHECK(n == 0 && out[0] == '\0');
    n = zhp_hexdump(d, 1, out, sizeof(out));
    CHECK(n == 9 && strcmp(out, "0000: 01\n") == 0);
    n = zhp_hexdump(d, 16, out, sizeof(out));
    CHECK(strcmp(out, "0000: 01 08 0F 16 1D 24 2B 32 39 40 47 4E 55 5C 63 6A\n") == 0);
    n = zhp_hexdump(d, 17, out, sizeof(out));
    CHECK(strcmp(out, "0000: 01 08 0F 16 1D 24 2B 32 39 40 47 4E 55 5C 63 6A\n0010: 71\n") == 0);
    n = zhp_hexdump(d, 4096, out, sizeof(out));
    CHECK(n == 256 * 54);
    CHECK(strncmp(out + 255 * 54, "0FF0:", 5) == 0);
    CHECK(zhp_hexdump(d, 4096, out, 100) == -1); /* too small: refused, not cut */
    CHECK(out[0] == '\0');
}

/* ---------------------------------------------------------------- fakes for CT-006, 009, 010 */
typedef struct {
    int rc_info, rc_btn, rc_status, rc_raw, rc_rumble, rc_event;
    int raw_has;
    int raw_calls;
    int rumble_calls, last_left, last_right;
    int32_t ev_uid, ev_pressed;
    uint32_t ev_ts;
    int ev_dropped;
    zhp_raw_info info;
} fake_t;

static int f_info(void *c, int32_t h, zhp_raw_info *o)
{
    fake_t *f = c;
    (void)h;
    *o = f->info;
    return f->rc_info;
}
static int f_btn(void *c, int32_t h, int32_t *n) { fake_t *f = c; (void)h; *n = 12; return f->rc_btn; }
static int f_status(void *c, int32_t h, int32_t *s) { fake_t *f = c; (void)h; *s = 1; return f->rc_status; }
static int f_raw(void *c, int32_t h, int *has) { fake_t *f = c; (void)h; f->raw_calls++; *has = f->raw_has; return f->rc_raw; }
static int f_rumble(void *c, int32_t h, int32_t l, int32_t r)
{
    fake_t *f = c;
    (void)h;
    f->rumble_calls++;
    f->last_left = l;
    f->last_right = r;
    return f->rc_rumble;
}
static int f_event(void *c, int32_t h, int32_t *uid, int32_t *pr, uint32_t *ts, int *dr)
{
    fake_t *f = c;
    (void)h;
    *uid = f->ev_uid;
    *pr = f->ev_pressed;
    *ts = f->ev_ts;
    *dr = f->ev_dropped;
    return f->rc_event;
}

static zhp_hid_ops ops_for(fake_t *f)
{
    zhp_hid_ops o;
    o.ctx = f;
    o.get_info = f_info;
    o.get_num_buttons = f_btn;
    o.get_status = f_status;
    o.raw_query = f_raw;
    o.rumble = f_rumble;
    o.next_button_event = f_event;
    return o;
}

static int is_err_text(zhp_val v, int code)
{
    char s[32], want[32];
    int n = 0, k;
    zhp_val_fmt(s, sizeof(s), v, 4);
    want[n++] = 'E'; want[n++] = 'R'; want[n++] = 'R'; want[n++] = '(';
    k = code;
    if (k < 0) { want[n++] = '-'; k = -k; }
    if (k >= 10) { want[n++] = (char)('0' + k / 10); }
    want[n++] = (char)('0' + k % 10);
    want[n++] = ')';
    want[n] = '\0';
    return v.prov == ZHP_P_ERR && strcmp(s, want) == 0;
}

/* ---------------------------------------------------------------- CT-006 */
static void CT_006_errors_never_numbers(void)
{
    fake_t f;
    zhp_hid_ops ops;
    zhp_card c;
    char s[32];
    memset(&f, 0, sizeof(f));
    f.info.vid = 0x1EAA; f.info.pid = 0x135; f.info.type = 7; f.info.bt = 0;
    ops = ops_for(&f);

    /* healthy: measured values with the /M tag */
    zhp_hid_card(&ops, 5, &c);
    zhp_val_fmt(s, sizeof(s), c.vid, 4);
    CHECK(strcmp(s, "0x1EAA/M") == 0);
    zhp_val_fmt(s, sizeof(s), c.nbuttons, 0);
    CHECK(strcmp(s, "12/M") == 0);

    /* every source fails: no field may print a number or a zero */
    f.rc_info = -5; f.rc_btn = -6; f.rc_status = -7;
    zhp_hid_card(&ops, 5, &c);
    CHECK(is_err_text(c.vid, -5) && is_err_text(c.pid, -5) && is_err_text(c.type, -5) && is_err_text(c.bt, -5));
    CHECK(is_err_text(c.nbuttons, -6));
    CHECK(is_err_text(c.status, -7));

    /* an error code of 0 cannot be built: ERR keeps the code it was given, UNKNOWN has no number */
    zhp_val_fmt(s, sizeof(s), zhp_val_unknown(), 0);
    CHECK(strcmp(s, "UNKNOWN") == 0);
    zhp_val_fmt(s, sizeof(s), zhp_val_derived(3), 0);
    CHECK(strcmp(s, "3/D") == 0);
    CHECK(zhp_val_fmt(s, 3, zhp_val_err(-12345), 0) == -1 && s[0] == '\0');
}

/* ---------------------------------------------------------------- CT-007 */
static void CT_007_path_allowlist(void)
{
    zhp_fs fs;
    zhp_fs_init(&fs, "/mod/zhp/udata", "fs:/card0/zhp");
    CHECK(zhp_fs_allow(&fs, "/mod/zhp/udata/log.txt") == 1);
    CHECK(zhp_fs_allow(&fs, "fs:/card0/zhp/log.txt") == 1);
    CHECK(fs.refused == 0);
    CHECK(zhp_fs_allow(&fs, "/mod/zhp/udata/../../../sys/hid_devices.cfg") == 0);
    CHECK(zhp_fs_allow(&fs, "/sys/hid_devices.cfg") == 0);
    CHECK(zhp_fs_allow(&fs, "") == 0);
    CHECK(zhp_fs_allow(&fs, NULL) == 0);
    CHECK(zhp_fs_allow(&fs, "/mod/zhp/udata") == 0);       /* the folder itself */
    CHECK(zhp_fs_allow(&fs, "/mod/zhp/udata/") == 0);      /* no file name */
    CHECK(zhp_fs_allow(&fs, "/mod/zhp/udatax/log") == 0);  /* prefix trick */
    CHECK(zhp_fs_allow(&fs, "/mod/zhp/udata//log") == 0);  /* empty component */
    CHECK(zhp_fs_allow(&fs, "/mod/zhp/udata/./log") == 0);
    CHECK(zhp_fs_allow(&fs, "/mod/zhp/udata/a\\b") == 0);
    {
        char longp[400];
        memset(longp, 'a', sizeof(longp));
        memcpy(longp, "/mod/zhp/udata/", 15);
        longp[399] = '\0';
        CHECK(zhp_fs_allow(&fs, longp) == 0);
    }
    CHECK(fs.refused == 11);
    /* no SD folder configured: SD paths are refused */
    zhp_fs_init(&fs, "/mod/zhp/udata", NULL);
    CHECK(zhp_fs_allow(&fs, "fs:/card0/zhp/log.txt") == 0);
}

/* ---------------------------------------------------------------- CT-008 */
typedef struct {
    int fail_step; /* 1 write, 2 remove, 3 rename; 0 none */
    int remove_enoent;
    char calls[8];
    int ncalls;
    char last_tmp[300];
} ffs_t;

static int ff_write(void *c, const char *p, const uint8_t *d, size_t n)
{
    ffs_t *f = c;
    (void)d; (void)n;
    f->calls[f->ncalls++] = 'w';
    strcpy(f->last_tmp, p);
    return f->fail_step == 1 ? -11 : 0;
}
static int ff_remove(void *c, const char *p)
{
    ffs_t *f = c;
    (void)p;
    f->calls[f->ncalls++] = 'r';
    if (f->fail_step == 2) { return -12; }
    return f->remove_enoent ? ZHP_FS_ENOENT : 0;
}
static int ff_rename(void *c, const char *a, const char *b)
{
    ffs_t *f = c;
    (void)a; (void)b;
    f->calls[f->ncalls++] = 'n';
    return f->fail_step == 3 ? -13 : 0;
}

static void CT_008_save_steps(void)
{
    zhp_fs fs;
    zhp_fs_ops ops;
    ffs_t f;
    zhp_save_result r;
    const uint8_t data[3] = {1, 2, 3};
    int step;
    zhp_fs_init(&fs, "/mod/zhp/udata", NULL);
    ops.ctx = &f;
    ops.write = ff_write;
    ops.remove = ff_remove;
    ops.rename = ff_rename;

    memset(&f, 0, sizeof(f));
    r = zhp_fs_save(&fs, &ops, "/mod/zhp/udata/log.txt", data, 3);
    CHECK(r.step == ZHP_SAVE_OK && strcmp(f.calls, "wrn") == 0);
    CHECK(strcmp(f.last_tmp, "/mod/zhp/udata/log.txt.tmp") == 0);

    memset(&f, 0, sizeof(f));
    f.remove_enoent = 1; /* destination did not exist: still a success */
    r = zhp_fs_save(&fs, &ops, "/mod/zhp/udata/log.txt", data, 3);
    CHECK(r.step == ZHP_SAVE_OK && strcmp(f.calls, "wrn") == 0);

    for (step = 1; step <= 3; step++) {
        memset(&f, 0, sizeof(f));
        f.fail_step = step;
        r = zhp_fs_save(&fs, &ops, "/mod/zhp/udata/log.txt", data, 3);
        CHECK((int)r.step == step);
        CHECK(r.err == -10 - step);
        /* after a failure nothing later runs; the temp file is never removed by us */
        CHECK(strchr(f.calls, 'x') == NULL);
        CHECK((step == 1 && strcmp(f.calls, "w") == 0) || (step == 2 && strcmp(f.calls, "wr") == 0) ||
              (step == 3 && strcmp(f.calls, "wrn") == 0));
    }

    /* a denied path never reaches the file layer */
    memset(&f, 0, sizeof(f));
    r = zhp_fs_save(&fs, &ops, "/sys/hid_devices.cfg", data, 3);
    CHECK(r.step == ZHP_SAVE_DENIED && f.ncalls == 0);
}

/* ---------------------------------------------------------------- CT-009 */
static void CT_009_adapter_events_and_rumble(void)
{
    fake_t f;
    zhp_hid_ops ops;
    zhp_hid h;
    zhp_event store[8];
    zhp_ring ring;
    zhp_event e;
    zhp_rumble rb;
    int32_t err = 0;

    memset(&f, 0, sizeof(f));
    ops = ops_for(&f);
    zhp_hid_init(&h, &ops);
    zhp_ring_init(&ring, store, 8);

    /* an event with the dropped flag raises the indicator */
    f.ev_uid = 0x0106c40a; f.ev_pressed = 1; f.ev_ts = 1234; f.ev_dropped = 1;
    CHECK(zhp_hid_poll_button(&h, 7, &ring, &err) == 1);
    CHECK(h.dropped_seen == 1);
    CHECK(zhp_ring_pop(&ring, &e) == 1 && e.uid == 0x0106c40a && e.kind == ZHP_EV_BTN_DOWN && e.ts_ms == 1234 && e.dev == 7);

    /* nothing waiting, then an error code */
    f.rc_event = ZHP_HID_NOMORE;
    CHECK(zhp_hid_poll_button(&h, 7, &ring, &err) == 0);
    f.rc_event = -9;
    CHECK(zhp_hid_poll_button(&h, 7, &ring, &err) == -1 && err == -9);
    CHECK(ring.count == 0);

    /* rumble: one start, one stop within 1.1 s, a second press during the pulse is ignored */
    memset(&rb, 0, sizeof(rb));
    CHECK(zhp_rumble_press(&rb, &h, 7, 1000, &err) == 1);
    CHECK(f.rumble_calls == 1 && f.last_left == ZHP_RUMBLE_ON && f.last_right == ZHP_RUMBLE_ON);
    CHECK(zhp_rumble_press(&rb, &h, 7, 1200, &err) == 0);
    CHECK(f.rumble_calls == 1);
    zhp_rumble_tick(&rb, &h, 1999);
    CHECK(rb.active == 1 && f.rumble_calls == 1);
    zhp_rumble_tick(&rb, &h, 2000); /* 1000 ms after the start */
    CHECK(rb.active == 0 && f.rumble_calls == 2 && f.last_left == 0 && f.last_right == 0);
    CHECK(zhp_rumble_press(&rb, &h, 7, 5000, &err) == 1); /* usable again */

    /* the console refuses rumble: reported, not started */
    memset(&rb, 0, sizeof(rb));
    f.rc_rumble = -4;
    CHECK(zhp_rumble_press(&rb, &h, 7, 9000, &err) == -1 && err == -4 && rb.active == 0);
}

/* ---------------------------------------------------------------- CT-010 */
static void CT_010_raw_probe(void)
{
    fake_t f;
    zhp_hid_ops ops;
    zhp_hid h;
    zhp_raw_result r;
    char s[32];

    /* interface present */
    memset(&f, 0, sizeof(f));
    f.raw_has = 1;
    ops = ops_for(&f);
    zhp_hid_init(&h, &ops);
    r = zhp_hid_raw_probe(&h, 1);
    CHECK(r.state == ZHP_RAW_AVAILABLE);
    zhp_raw_fmt(s, sizeof(s), r);
    CHECK(strcmp(s, "AVAILABLE") == 0);
    r = zhp_hid_raw_probe(&h, 1);
    CHECK(f.raw_calls == 1); /* asked once per device */

    /* the query fails with a code */
    memset(&f, 0, sizeof(f));
    f.rc_raw = -3;
    ops = ops_for(&f);
    zhp_hid_init(&h, &ops);
    r = zhp_hid_raw_probe(&h, 2);
    CHECK(r.state == ZHP_RAW_NOT_AVAILABLE && r.code == -3);
    zhp_raw_fmt(s, sizeof(s), r);
    CHECK(strcmp(s, "NOT_AVAILABLE(-3)") == 0);

    /* QueryInterface handing back the device object: available, but flagged as a stub */
    memset(&f, 0, sizeof(f));
    f.raw_has = 2;
    ops = ops_for(&f);
    zhp_hid_init(&h, &ops);
    r = zhp_hid_raw_probe(&h, 4);
    CHECK(r.state == ZHP_RAW_AVAILABLE && r.alias == 1);
    zhp_raw_fmt(s, sizeof(s), r);
    CHECK(strcmp(s, "AVAILABLE(alias)") == 0);

    /* success but a NULL interface (TR-005): still not available, with its own code */
    memset(&f, 0, sizeof(f));
    f.raw_has = 0;
    ops = ops_for(&f);
    zhp_hid_init(&h, &ops);
    r = zhp_hid_raw_probe(&h, 3);
    CHECK(r.state == ZHP_RAW_NOT_AVAILABLE && r.code == ZHP_E_NULL_IFACE);
    zhp_raw_fmt(s, sizeof(s), r);
    CHECK(strcmp(s, "NOT_AVAILABLE(-1000)") == 0);
    zhp_hid_raw_probe(&h, 3);
    zhp_hid_raw_probe(&h, 3);
    CHECK(f.raw_calls == 1);
    CHECK(zhp_raw_fmt(s, 5, r) == -1 && s[0] == '\0');
}

int main(void)
{
    RUN(CT_001_ring_overflow);
    RUN(CT_002_devtab_overflow);
    RUN(CT_003_dpad_truth_table);
    RUN(CT_004_log_format);
    RUN(CT_005_hexdump);
    RUN(CT_006_errors_never_numbers);
    RUN(CT_007_path_allowlist);
    RUN(CT_008_save_steps);
    RUN(CT_009_adapter_events_and_rumble);
    RUN(CT_010_raw_probe);
    printf("%s\n", zt_total_fail ? "FAILED" : "ALL PASS");
    return zt_total_fail ? 1 : 0;
}
