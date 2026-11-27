#include "zhp_hid.h"

#include <string.h>

/* every call: 0 -> measured value, anything else -> ERR(code) */
static zhp_val take(int rc, int32_t v)
{
    return rc == 0 ? zhp_val_measured(v) : zhp_val_err(rc);
}

void zhp_hid_card(const zhp_hid_ops *ops, int32_t handle, zhp_card *out)
{
    zhp_raw_info info;
    int32_t n = 0, st = 0;
    int rc;
    memset(&info, 0, sizeof(info));
    rc = ops->get_info(ops->ctx, handle, &info);
    out->vid = take(rc, info.vid);
    out->pid = take(rc, info.pid);
    out->type = take(rc, info.type);
    out->bt = take(rc, info.bt);
    rc = ops->get_num_buttons(ops->ctx, handle, &n);
    out->nbuttons = take(rc, n);
    rc = ops->get_status(ops->ctx, handle, &st);
    out->status = take(rc, st);
}

void zhp_hid_init(zhp_hid *h, const zhp_hid_ops *ops)
{
    memset(h, 0, sizeof(*h));
    h->ops = ops;
}

zhp_raw_result zhp_hid_raw_probe(zhp_hid *h, int32_t handle)
{
    zhp_raw_result r;
    int has = 0, rc, i;
    for (i = 0; i < h->cached; i++) {
        if (h->cache[i].handle == handle) {
            return h->cache[i].res;
        }
    }
    rc = h->ops->raw_query(h->ops->ctx, handle, &has);
    r.alias = 0;
    if (rc == 0 && has) {
        r.state = ZHP_RAW_AVAILABLE;
        r.code = 0;
        r.alias = has == 2 ? 1 : 0;
    } else {
        r.state = ZHP_RAW_NOT_AVAILABLE;
        r.code = rc != 0 ? rc : ZHP_E_NULL_IFACE;
    }
    if (h->cached < ZHP_MAX_DEVICES) {
        h->cache[h->cached].handle = handle;
        h->cache[h->cached].res = r;
        h->cached++;
    }
    return r;
}

int zhp_raw_fmt(char *out, size_t cap, zhp_raw_result r)
{
    char num[16];
    size_t n = 0, i;
    const char *head = r.state == ZHP_RAW_AVAILABLE ? "AVAILABLE" : "NOT_AVAILABLE(";
    if (r.state == ZHP_RAW_UNPROBED) {
        head = "UNPROBED";
    }
    while (head[n]) {
        n++;
    }
    if (n + 1 > cap) {
        if (cap) {
            out[0] = '\0';
        }
        return -1;
    }
    memcpy(out, head, n);
    if (r.state == ZHP_RAW_AVAILABLE && r.alias) {
        const char *tail = "(alias)";
        size_t t = 0;
        while (tail[t]) {
            if (n + 1 >= cap) {
                out[0] = '\0';
                return -1;
            }
            out[n++] = tail[t++];
        }
        out[n] = '\0';
        return (int)n;
    }
    if (r.state == ZHP_RAW_NOT_AVAILABLE) {
        int k = zhp_val_fmt(num, sizeof(num), zhp_val_err(r.code), 0); /* "ERR(code)" */
        /* reuse the number between the parentheses */
        for (i = 4; (int)i < k - 1 && n + 2 < cap; i++) {
            out[n++] = num[i];
        }
        if (n + 2 > cap) {
            out[0] = '\0';
            return -1;
        }
        out[n++] = ')';
    }
    out[n] = '\0';
    return (int)n;
}

int zhp_hid_poll_button(zhp_hid *h, int32_t handle, zhp_ring *ring, int32_t *err_code)
{
    int32_t uid = 0, pressed = 0;
    uint32_t ts = 0;
    int dropped = 0, rc;
    zhp_event ev;
    rc = h->ops->next_button_event(h->ops->ctx, handle, &uid, &pressed, &ts, &dropped);
    if (rc == ZHP_HID_NOMORE) {
        return 0;
    }
    if (rc != 0) {
        if (err_code) {
            *err_code = rc;
        }
        return -1;
    }
    if (dropped) {
        h->dropped_seen = 1;
    }
    ev.ts_ms = ts;
    ev.dev = handle;
    ev.uid = uid;
    ev.value = pressed ? 1 : 0;
    ev.kind = (uint8_t)(pressed ? ZHP_EV_BTN_DOWN : ZHP_EV_BTN_UP);
    zhp_ring_push(ring, &ev);
    return 1;
}

int zhp_rumble_press(zhp_rumble *r, const zhp_hid *h, int32_t handle, uint32_t now_ms, int32_t *err)
{
    int rc;
    if (r->active) {
        return 0;
    }
    rc = h->ops->rumble(h->ops->ctx, handle, ZHP_RUMBLE_ON, ZHP_RUMBLE_ON);
    if (rc != 0) {
        if (err) {
            *err = rc;
        }
        return -1;
    }
    r->active = 1;
    r->handle = handle;
    r->start_ms = now_ms;
    r->stop_err = 0;
    return 1;
}

void zhp_rumble_tick(zhp_rumble *r, const zhp_hid *h, uint32_t now_ms)
{
    if (r->active && (uint32_t)(now_ms - r->start_ms) >= ZHP_RUMBLE_MS) {
        r->stop_err = h->ops->rumble(h->ops->ctx, r->handle, 0, 0);
        r->active = 0;
    }
}
