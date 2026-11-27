/* ZeeHID Probe, BREW glue and a first screen. Walking skeleton (docs/ROADMAP.md, phase 3):
 * enumerate the HID devices the console exposes, per device-type query, show a card per device,
 * poll button events, run the raw-interface probe and one rumble pulse. Read-only: this file
 * opens no file for writing.
 * Requirements served: LF-001..005, LF-009, LF-011 (partly), TR-004, TR-005. */
#include "AEEModGen.h"
#include "AEEAppGen.h"
#include "AEEShell.h"
#include "AEEStdLib.h"
#include "AEEDisp.h"
#include "AEEHID.bid"
#include "AEEIHID.h"
#include "AEEIHIDDevice.h"
#include "AEEHIDDevice_Joystick.h"
#include "AEEHIDDevice_Keyboard.h"
#include "AEEHIDDevice_Mouse.h"
#include "zhp.bid"

#include "../core/zhp_core.h"
#include "../hidport/zhp_hid.h"

#define TICK_MS 33
#define ENUM_EVERY 15 /* ticks between enumerations (about half a second) */
#define HIST 7        /* events kept for the screen */
#define LINE_H 20
#define MARGIN_X 16
#define MARGIN_Y 16
#define AVK_ZB_CONFIRM 0xe064 /* the Zeebo controller's confirm button */
#define AEE_RAW_IID 0x1088045 /* AEEIID_IHIDRawDevice, read from the BREW MP SDK header (a fact, not code) */
#define NQ 4

typedef enum { PAGE_LIST = 0, PAGE_CARD } zhp_page;

typedef struct {
    AEEApplet a;
    IHID *hid;
    int hid_rc; /* result of creating IHID */
    IHIDDevice *dev[ZHP_MAX_DEVICES];
    zhp_devtab tab;
    zhp_card card[ZHP_MAX_DEVICES];
    zhp_raw_result raw[ZHP_MAX_DEVICES];
    zhp_hid h;
    zhp_hid_ops ops;
    zhp_val qres[NQ];
    zhp_event evbuf[256];
    zhp_ring ring;
    zhp_event hist[HIST];
    uint32 nhist;
    zhp_rumble rumble;
    int32_t rumble_err;
    int last_ev_rc; /* last non-zero result of GetNextButtonEvent that was not "nothing more" */
    int nomore_rc;  /* the code the console uses for "nothing more", as seen */
    int sel;
    zhp_page page;
    uint32 ticks;
    int dirty;
} zhp;

static const int qtype[NQ] = {0, AEEUID_HID_Joystick_Device, AEEUID_HID_Keyboard_Device, AEEUID_HID_Mouse_Device};
static const char *const qname[NQ] = {"any", "joy", "kbd", "mouse"};

static boolean zhp_HandleEvent(IApplet *pi, AEEEvent e, uint16 w, uint32 dw);
static void zhp_Draw(zhp *me);

/* ---------------------------------------------------------------- the HID ops over IHID */
static int ensure_dev(zhp *me, int32_t handle)
{
    int slot = zhp_devtab_find(&me->tab, handle);
    int rc;
    if (slot < 0) {
        return EFAILED;
    }
    if (me->dev[slot]) {
        return 0;
    }
    rc = IHID_CreateDevice(me->hid, handle, &me->dev[slot]);
    if (rc != SUCCESS) {
        me->dev[slot] = NULL;
        return rc;
    }
    return 0;
}

static IHIDDevice *devof(zhp *me, int32_t handle)
{
    int slot = zhp_devtab_find(&me->tab, handle);
    return slot < 0 ? NULL : me->dev[slot];
}

static int op_get_info(void *ctx, int32_t handle, zhp_raw_info *out)
{
    zhp *me = (zhp *)ctx;
    AEEHIDDeviceInfo info;
    int rc;
    MEMSET(&info, 0, sizeof(info));
    rc = IHID_GetDeviceInfo(me->hid, handle, &info);
    if (rc == SUCCESS) {
        out->type = info.nDeviceType;
        out->pid = info.wProductID;
        out->vid = info.wVendorID;
        out->bt = info.bBluetoothDevice ? 1 : 0;
    }
    return rc;
}

static int op_get_num_buttons(void *ctx, int32_t handle, int32_t *n)
{
    zhp *me = (zhp *)ctx;
    int rc = ensure_dev(me, handle), v = 0;
    if (rc) {
        return rc;
    }
    rc = IHIDDevice_GetNumberOfButtons(devof(me, handle), &v);
    *n = v;
    return rc;
}

static int op_get_status(void *ctx, int32_t handle, int32_t *st)
{
    zhp *me = (zhp *)ctx;
    int rc = ensure_dev(me, handle), v = 0;
    if (rc) {
        return rc;
    }
    rc = IHIDDevice_GetDeviceStatus(devof(me, handle), &v);
    *st = v;
    return rc;
}

static int op_raw_query(void *ctx, int32_t handle, int *has)
{
    zhp *me = (zhp *)ctx;
    int rc = ensure_dev(me, handle);
    void *p = NULL;
    *has = 0;
    if (rc) {
        return rc;
    }
    {
        IHIDDevice *self = devof(me, handle);
        rc = IHIDDevice_QueryInterface(self, AEE_RAW_IID, &p);
        if (rc == SUCCESS && p) {
            /* a raw interface is a different object; the same pointer back is a stub */
            *has = (p == (void *)self) ? 2 : 1;
            if (p != (void *)self) {
                IHIDDevice_Release((IHIDDevice *)p); /* every interface has Release in slot 1 */
            }
        }
    }
    return rc;
}

static int op_rumble(void *ctx, int32_t handle, int32_t l, int32_t r)
{
    zhp *me = (zhp *)ctx;
    int rc = ensure_dev(me, handle);
    if (rc) {
        return rc;
    }
    return IHIDDevice_Rumble(devof(me, handle), l, r);
}

static int op_next_event(void *ctx, int32_t handle, int32_t *uid, int32_t *pressed, uint32_t *ts, int *dropped)
{
    zhp *me = (zhp *)ctx;
    AEEHIDButtonInfo b;
    uint32 t = 0;
    boolean lost = FALSE;
    int rc = ensure_dev(me, handle);
    if (rc) {
        return rc;
    }
    MEMSET(&b, 0, sizeof(b));
    rc = IHIDDevice_GetNextButtonEvent(devof(me, handle), &b, &t, &lost);
    if (rc == SUCCESS) {
        *uid = b.nButtonUID;
        *pressed = b.nState ? 1 : 0;
        *ts = t;
        *dropped = lost ? 1 : 0;
        return 0;
    }
    /* "nothing more" is AEE_ENOMORE (47) on the console per the header, EFAILED (1) in zeebx.
     * Both mean an empty queue; the code seen is shown on screen so the console can settle it. */
    if (rc == 47 || rc == EFAILED) {
        me->nomore_rc = rc;
        return ZHP_HID_NOMORE;
    }
    return rc;
}

/* ---------------------------------------------------------------- enumeration */
static void fill_card(zhp *me, int slot)
{
    zhp_card *c = &me->card[slot];
    zhp_hid_card(&me->ops, me->tab.dev[slot].handle, c);
    me->tab.dev[slot].vid = c->vid;
    me->tab.dev[slot].pid = c->pid;
    me->tab.dev[slot].type = c->type;
    me->tab.dev[slot].bt = c->bt;
    me->tab.dev[slot].status = c->status;
    me->raw[slot].state = ZHP_RAW_UNPROBED;
    me->raw[slot].code = 0;
}

static void drop_slot(zhp *me, int slot)
{
    if (me->dev[slot]) {
        IHIDDevice_Release(me->dev[slot]);
        me->dev[slot] = NULL;
    }
    me->tab.used[slot] = 0;
}

static void enumerate(zhp *me)
{
    int32_t seen[ZHP_MAX_DEVICES * NQ];
    int nseen = 0, q, i, rc, req, handles[ZHP_MAX_DEVICES];
    if (!me->hid) {
        return;
    }
    for (q = 0; q < NQ; q++) {
        zhp_val before = me->qres[q];
        req = 0;
        rc = IHID_GetConnectedDevices(me->hid, qtype[q], handles, ZHP_MAX_DEVICES, &req);
        me->qres[q] = rc == SUCCESS ? zhp_val_measured(req) : zhp_val_err(rc);
        if (before.prov != me->qres[q].prov || before.v != me->qres[q].v) {
            me->dirty = 1;
            DBGPRINTF("ZHP query %s -> %s %d", qname[q], rc == SUCCESS ? "n" : "ERR", rc == SUCCESS ? req : rc);
        }
        if (rc != SUCCESS) {
            continue;
        }
        for (i = 0; i < req && i < ZHP_MAX_DEVICES; i++) {
            int existed = zhp_devtab_find(&me->tab, handles[i]) >= 0;
            int slot = zhp_devtab_add(&me->tab, handles[i], (uint8_t)q);
            if (nseen < (int)(sizeof(seen) / sizeof(seen[0]))) {
                seen[nseen++] = handles[i];
            }
            if (slot >= 0 && !existed) {
                fill_card(me, slot);
                me->dirty = 1;
                DBGPRINTF("ZHP device h=%d vid=%x pid=%x type=%x bt=%d", handles[i], me->card[slot].vid.v,
                          me->card[slot].pid.v, me->card[slot].type.v, me->card[slot].bt.v);
            }
        }
    }
    for (i = 0; i < ZHP_MAX_DEVICES; i++) {
        int k, found = 0;
        if (!me->tab.used[i]) {
            continue;
        }
        for (k = 0; k < nseen; k++) {
            if (seen[k] == me->tab.dev[i].handle) {
                found = 1;
            }
        }
        if (!found) {
            DBGPRINTF("ZHP device gone h=%d", me->tab.dev[i].handle);
            drop_slot(me, i);
            if (me->sel >= (int)zhp_devtab_count(&me->tab)) {
                me->sel = 0;
            }
            me->dirty = 1;
        }
    }
}

static int slot_of_row(zhp *me, int row)
{
    int i, n = 0;
    for (i = 0; i < ZHP_MAX_DEVICES; i++) {
        if (me->tab.used[i]) {
            if (n == row) {
                return i;
            }
            n++;
        }
    }
    return -1;
}

static void poll_events(zhp *me)
{
    int i, guard;
    int32_t err = 0;
    zhp_event e;
    for (i = 0; i < ZHP_MAX_DEVICES; i++) {
        if (!me->tab.used[i]) {
            continue;
        }
        for (guard = 0; guard < 64; guard++) {
            int r = zhp_hid_poll_button(&me->h, me->tab.dev[i].handle, &me->ring, &err);
            if (r == 0) {
                break;
            }
            if (r < 0) {
                me->last_ev_rc = (int)err;
                me->dirty = 1;
                break;
            }
        }
    }
    while (zhp_ring_pop(&me->ring, &e)) {
        if (me->nhist < HIST) {
            me->hist[me->nhist++] = e;
        } else {
            for (i = 1; i < HIST; i++) {
                me->hist[i - 1] = me->hist[i];
            }
            me->hist[HIST - 1] = e;
        }
        me->dirty = 1;
        DBGPRINTF("ZHP event dev=%d uid=%x %s t=%u", e.dev, e.uid, e.kind == ZHP_EV_BTN_DOWN ? "down" : "up", e.ts_ms);
    }
}

/* ---------------------------------------------------------------- drawing */
static void put(zhp *me, int row, const char *text)
{
    AECHAR ws[96];
    STRTOWSTR(text, ws, sizeof(ws));
    IDISPLAY_DrawText(me->a.m_pIDisplay, AEE_FONT_NORMAL, ws, -1, MARGIN_X, MARGIN_Y + row * LINE_H, NULL,
                      IDF_ALIGN_LEFT | IDF_ALIGN_TOP | IDF_TEXT_TRANSPARENT);
}

static const char *val_s(char *buf, size_t cap, zhp_val v, int hex)
{
    if (zhp_val_fmt(buf, cap, v, hex) < 0) {
        buf[0] = '?';
        buf[1] = '\0';
    }
    return buf;
}

static void draw_events(zhp *me, int row)
{
    char line[96];
    uint32 i;
    put(me, row++, "events (newest last):");
    for (i = 0; i < me->nhist; i++) {
        const zhp_event *e = &me->hist[i];
        SNPRINTF(line, sizeof(line), " dev %d  uid 0x%08X  %s", e->dev, (unsigned)e->uid,
                 e->kind == ZHP_EV_BTN_DOWN ? "DOWN" : "UP");
        put(me, row++, line);
    }
}

static void zhp_Draw(zhp *me)
{
    char line[120], a[24], b[24], c[24], d[24];
    int q, row = 0, i, n = 0;
    IDISPLAY_ClearScreen(me->a.m_pIDisplay);
    put(me, row++, "ZeeHID Probe  (read-only)");
    if (!me->hid) {
        SNPRINTF(line, sizeof(line), "no IHID: ERR(%d)", me->hid_rc);
        put(me, row++, line);
        IDISPLAY_Update(me->a.m_pIDisplay);
        return;
    }
    line[0] = '\0';
    for (q = 0; q < NQ; q++) {
        char one[40];
        SNPRINTF(one, sizeof(one), "%s=%s  ", qname[q], val_s(a, sizeof(a), me->qres[q], 0));
        STRLCAT(line, one, sizeof(line));
    }
    put(me, row++, line);
    if (me->page == PAGE_LIST) {
        put(me, row++, "devices (UP/DOWN, SELECT opens, CLR exits):");
        for (i = 0; i < ZHP_MAX_DEVICES; i++) {
            if (!me->tab.used[i]) {
                continue;
            }
            SNPRINTF(line, sizeof(line), "%s h=%d  %s:%s  type %s  q=0x%x", n == me->sel ? ">" : " ",
                     me->tab.dev[i].handle, val_s(a, sizeof(a), me->card[i].vid, 4),
                     val_s(b, sizeof(b), me->card[i].pid, 4), val_s(c, sizeof(c), me->card[i].type, 8),
                     me->tab.dev[i].query_mask);
            put(me, row++, line);
            n++;
        }
        if (n == 0) {
            put(me, row++, "(none exposed by any query)");
        }
        if (me->tab.overflow) {
            SNPRINTF(line, sizeof(line), "OVERFLOW: %u device(s) not shown", (unsigned)me->tab.overflow);
            put(me, row++, line);
        }
    } else {
        int slot = slot_of_row(me, me->sel);
        if (slot >= 0) {
            const zhp_card *cd = &me->card[slot];
            SNPRINTF(line, sizeof(line), "device h=%d  vid %s  pid %s", me->tab.dev[slot].handle,
                     val_s(a, sizeof(a), cd->vid, 4), val_s(b, sizeof(b), cd->pid, 4));
            put(me, row++, line);
            SNPRINTF(line, sizeof(line), "type %s  bluetooth %s  status %s", val_s(a, sizeof(a), cd->type, 8),
                     val_s(b, sizeof(b), cd->bt, 0), val_s(c, sizeof(c), cd->status, 0));
            put(me, row++, line);
            SNPRINTF(line, sizeof(line), "buttons %s", val_s(a, sizeof(a), cd->nbuttons, 0));
            put(me, row++, line);
            if (me->raw[slot].state == ZHP_RAW_UNPROBED) {
                SNPRINTF(line, sizeof(line), "raw interface: not probed (SELECT)");
            } else {
                zhp_raw_fmt(d, sizeof(d), me->raw[slot]);
                SNPRINTF(line, sizeof(line), "raw interface: %s", d);
            }
            put(me, row++, line);
            put(me, row++, "RIGHT = rumble pulse (1 s), CLR = back");
        }
    }
    if (me->last_ev_rc != 0) {
        SNPRINTF(line, sizeof(line), "last event read error: ERR(%d)", me->last_ev_rc);
        put(me, row++, line);
    }
    if (me->nomore_rc != 0) {
        SNPRINTF(line, sizeof(line), "empty queue answers %d", me->nomore_rc);
        put(me, row++, line);
    }
    if (me->h.dropped_seen) {
        put(me, row++, "console reported DROPPED events");
    }
    draw_events(me, row + 1);
    IDISPLAY_Update(me->a.m_pIDisplay);
}

/* ---------------------------------------------------------------- applet */
static void zhp_Tick(zhp *me)
{
    me->ticks++;
    if (me->hid) {
        if (me->ticks % ENUM_EVERY == 1) {
            enumerate(me);
        }
        poll_events(me);
        zhp_rumble_tick(&me->rumble, &me->h, (uint32_t)GETUPTIMEMS());
    }
    if (me->dirty) {
        me->dirty = 0;
        zhp_Draw(me);
    }
    ISHELL_SetTimer(me->a.m_pIShell, TICK_MS, (PFNNOTIFY)zhp_Tick, me);
}

int AEEClsCreateInstance(AEECLSID id, IShell *sh, IModule *mod, void **pp)
{
    *pp = NULL;
    if (id != AEECLSID_ZHP) {
        return EFAILED;
    }
    if (AEEApplet_New(sizeof(zhp), id, sh, mod, (IApplet **)pp, (AEEHANDLER)zhp_HandleEvent, NULL) == TRUE) {
        return AEE_SUCCESS;
    }
    return EFAILED;
}

static void start(zhp *me)
{
    int i;
    me->sel = 0;
    me->page = PAGE_LIST;
    me->nhist = 0;
    me->last_ev_rc = 0;
    me->nomore_rc = 0;
    MEMSET(me->dev, 0, sizeof(me->dev));
    MEMSET(me->card, 0, sizeof(me->card));
    MEMSET(me->raw, 0, sizeof(me->raw));
    MEMSET(&me->rumble, 0, sizeof(me->rumble));
    zhp_devtab_init(&me->tab);
    zhp_ring_init(&me->ring, me->evbuf, sizeof(me->evbuf) / sizeof(me->evbuf[0]));
    me->ops.ctx = me;
    me->ops.get_info = op_get_info;
    me->ops.get_num_buttons = op_get_num_buttons;
    me->ops.get_status = op_get_status;
    me->ops.raw_query = op_raw_query;
    me->ops.rumble = op_rumble;
    me->ops.next_button_event = op_next_event;
    zhp_hid_init(&me->h, &me->ops);
    for (i = 0; i < NQ; i++) {
        me->qres[i] = zhp_val_unknown();
    }
    me->hid_rc = ISHELL_CreateInstance(me->a.m_pIShell, AEECLSID_HID, (void **)&me->hid);
    if (me->hid_rc != SUCCESS) {
        me->hid = NULL;
    }
    DBGPRINTF("ZHP start: IHID %s rc=%d", me->hid ? "ok" : "missing", me->hid_rc);
    me->dirty = 1;
    zhp_Draw(me);
    ISHELL_SetTimer(me->a.m_pIShell, TICK_MS, (PFNNOTIFY)zhp_Tick, me);
}

static void stop(zhp *me)
{
    int i;
    ISHELL_CancelTimer(me->a.m_pIShell, (PFNNOTIFY)zhp_Tick, me);
    if (me->rumble.active) { /* HZ-2: never leave the motors on */
        me->ops.rumble(me, me->rumble.handle, 0, 0);
        me->rumble.active = 0;
    }
    for (i = 0; i < ZHP_MAX_DEVICES; i++) {
        if (me->dev[i]) {
            IHIDDevice_Release(me->dev[i]);
            me->dev[i] = NULL;
        }
    }
    if (me->hid) {
        IHID_Release(me->hid);
        me->hid = NULL;
    }
}

static boolean zhp_HandleEvent(IApplet *pi, AEEEvent e, uint16 w, uint32 dw)
{
    zhp *me = (zhp *)pi;
    int slot, n;
    (void)dw;
    switch (e) {
    case EVT_APP_START:
        start(me);
        return TRUE;
    case EVT_APP_STOP:
    case EVT_APP_SUSPEND:
        stop(me);
        return TRUE;
    case EVT_APP_RESUME:
        start(me);
        return TRUE;
    case EVT_KEY:
        n = (int)zhp_devtab_count(&me->tab);
        DBGPRINTF("ZHP key 0x%x page=%d sel=%d n=%d", (int)w, me->page, me->sel, n);
        if (w == AVK_UP || w == AVK_DOWN) {
            if (me->page == PAGE_LIST && n > 0) {
                me->sel = (me->sel + (w == AVK_DOWN ? 1 : n - 1)) % n;
                me->dirty = 1;
            }
            return TRUE;
        }
        if (w == AVK_SELECT || w == AVK_ZB_CONFIRM) {
            if (me->page == PAGE_LIST && n > 0) {
                me->page = PAGE_CARD;
            } else if (me->page == PAGE_CARD) {
                slot = slot_of_row(me, me->sel);
                if (slot >= 0) {
                    me->raw[slot] = zhp_hid_raw_probe(&me->h, me->tab.dev[slot].handle);
                    DBGPRINTF("ZHP raw probe h=%d state=%d code=%d", me->tab.dev[slot].handle, me->raw[slot].state,
                              me->raw[slot].code);
                }
            }
            me->dirty = 1;
            return TRUE;
        }
        if (w == AVK_RIGHT && me->page == PAGE_CARD) {
            slot = slot_of_row(me, me->sel);
            if (slot >= 0) {
                int r = zhp_rumble_press(&me->rumble, &me->h, me->tab.dev[slot].handle, (uint32_t)GETUPTIMEMS(),
                                         &me->rumble_err);
                DBGPRINTF("ZHP rumble press -> %d err=%d", r, me->rumble_err);
            }
            return TRUE;
        }
        if (w == AVK_CLR) {
            if (me->page == PAGE_CARD) {
                me->page = PAGE_LIST;
                me->dirty = 1;
            } else {
                ISHELL_CloseApplet(me->a.m_pIShell, FALSE);
            }
            return TRUE;
        }
        return FALSE;
    default:
        break;
    }
    return FALSE;
}
