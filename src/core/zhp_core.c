#include "zhp_core.h"

#include <string.h>

/* ---- tiny string builder: no libc formatting, so the same code builds for BREW ---- */
typedef struct {
    char *p;
    size_t cap, len;
    int overflow;
} sb_t;

static void sb_putc(sb_t *s, char c)
{
    if (s->len + 1 < s->cap) {
        s->p[s->len] = c;
    } else {
        s->overflow = 1;
    }
    s->len++;
}

static void sb_puts(sb_t *s, const char *t)
{
    while (*t) {
        sb_putc(s, *t++);
    }
}

static void sb_dec(sb_t *s, int32_t v)
{
    char tmp[12];
    int n = 0;
    uint32_t u;
    if (v < 0) {
        sb_putc(s, '-');
        u = (uint32_t)0 - (uint32_t)v;
    } else {
        u = (uint32_t)v;
    }
    do {
        tmp[n++] = (char)('0' + (u % 10u));
        u /= 10u;
    } while (u);
    while (n) {
        sb_putc(s, tmp[--n]);
    }
}

static void sb_hex(sb_t *s, uint32_t v, int digits)
{
    static const char hx[] = "0123456789ABCDEF";
    int i;
    for (i = digits - 1; i >= 0; i--) {
        sb_putc(s, hx[(v >> (4 * i)) & 0xFu]);
    }
}

static int sb_done(sb_t *s)
{
    if (s->cap == 0) {
        return -1;
    }
    if (s->overflow) {
        s->p[0] = '\0';
        return -1;
    }
    s->p[s->len] = '\0';
    return (int)s->len;
}

/* ---- IM-006 ---- */
zhp_val zhp_val_measured(int32_t v) { zhp_val r; r.v = v; r.prov = ZHP_P_MEASURED; return r; }
zhp_val zhp_val_derived(int32_t v) { zhp_val r; r.v = v; r.prov = ZHP_P_DERIVED; return r; }
zhp_val zhp_val_unknown(void) { zhp_val r; r.v = 0; r.prov = ZHP_P_UNKNOWN; return r; }
zhp_val zhp_val_err(int32_t code) { zhp_val r; r.v = code; r.prov = ZHP_P_ERR; return r; }

int zhp_val_fmt(char *out, size_t cap, zhp_val v, int hex_digits)
{
    sb_t s;
    s.p = out;
    s.cap = cap;
    s.len = 0;
    s.overflow = 0;
    switch (v.prov) {
    case ZHP_P_UNKNOWN:
        sb_puts(&s, "UNKNOWN");
        break;
    case ZHP_P_ERR:
        sb_puts(&s, "ERR(");
        sb_dec(&s, v.v);
        sb_putc(&s, ')');
        break;
    default:
        if (hex_digits > 0) {
            sb_puts(&s, "0x");
            sb_hex(&s, (uint32_t)v.v, hex_digits);
        } else {
            sb_dec(&s, v.v);
        }
        sb_putc(&s, '/');
        sb_putc(&s, v.prov == ZHP_P_DERIVED ? 'D' : 'M');
        break;
    }
    return sb_done(&s);
}

/* ---- IM-001 ---- */
void zhp_ring_init(zhp_ring *r, zhp_event *storage, uint32_t cap)
{
    r->buf = storage;
    r->cap = cap;
    r->head = 0;
    r->count = 0;
    r->dropped = 0;
}

void zhp_ring_push(zhp_ring *r, const zhp_event *ev)
{
    if (r->cap == 0) {
        r->dropped++;
        return;
    }
    if (r->count == r->cap) {
        r->buf[r->head] = *ev;
        r->head = (r->head + 1u) % r->cap;
        r->dropped++;
    } else {
        r->buf[(r->head + r->count) % r->cap] = *ev;
        r->count++;
    }
}

int zhp_ring_pop(zhp_ring *r, zhp_event *out)
{
    if (r->count == 0) {
        return 0;
    }
    *out = r->buf[r->head];
    r->head = (r->head + 1u) % r->cap;
    r->count--;
    return 1;
}

/* ---- IM-002 ---- */
void zhp_devtab_init(zhp_devtab *t) { memset(t, 0, sizeof(*t)); }

int zhp_devtab_find(const zhp_devtab *t, int32_t handle)
{
    int i;
    for (i = 0; i < ZHP_MAX_DEVICES; i++) {
        if (t->used[i] && t->dev[i].handle == handle) {
            return i;
        }
    }
    return -1;
}

int zhp_devtab_add(zhp_devtab *t, int32_t handle, uint8_t query_bit)
{
    int i = zhp_devtab_find(t, handle);
    if (i >= 0) {
        t->dev[i].query_mask = (uint8_t)(t->dev[i].query_mask | (1u << (query_bit & 7u)));
        return i;
    }
    for (i = 0; i < ZHP_MAX_DEVICES; i++) {
        if (!t->used[i]) {
            memset(&t->dev[i], 0, sizeof(t->dev[i]));
            t->used[i] = 1;
            t->dev[i].handle = handle;
            t->dev[i].query_mask = (uint8_t)(1u << (query_bit & 7u));
            t->dev[i].vid = t->dev[i].pid = t->dev[i].type = zhp_val_unknown();
            t->dev[i].bt = t->dev[i].status = zhp_val_unknown();
            return i;
        }
    }
    t->overflow++;
    return -1;
}

int zhp_devtab_remove(zhp_devtab *t, int32_t handle)
{
    int i = zhp_devtab_find(t, handle);
    if (i < 0) {
        return 0;
    }
    t->used[i] = 0;
    return 1;
}

uint32_t zhp_devtab_count(const zhp_devtab *t)
{
    uint32_t n = 0;
    int i;
    for (i = 0; i < ZHP_MAX_DEVICES; i++) {
        n += t->used[i] ? 1u : 0u;
    }
    return n;
}

/* ---- IM-003 ---- */
zhp_dpad_class zhp_dpad_classify(const zhp_dir_obs obs[ZHP_DIR_COUNT])
{
    int nb = 0, np = 0, i;
    for (i = 0; i < ZHP_DIR_COUNT; i++) {
        nb += obs[i].btn_event ? 1 : 0;
        np += obs[i].pos_event ? 1 : 0;
    }
    if (nb == 0 && np == 0) {
        return ZHP_DPAD_NONE;
    }
    if (nb == ZHP_DIR_COUNT && np == ZHP_DIR_COUNT) {
        return ZHP_DPAD_BOTH;
    }
    if (nb == ZHP_DIR_COUNT && np == 0) {
        return ZHP_DPAD_BUTTONS;
    }
    if (np == ZHP_DIR_COUNT && nb == 0) {
        return ZHP_DPAD_POSITION;
    }
    return ZHP_DPAD_PARTIAL;
}

const char *zhp_dpad_name(zhp_dpad_class c)
{
    switch (c) {
    case ZHP_DPAD_BUTTONS: return "DPAD_BUTTONS";
    case ZHP_DPAD_POSITION: return "POSITION_AXIS";
    case ZHP_DPAD_BOTH: return "BOTH";
    case ZHP_DPAD_PARTIAL: return "PARTIAL";
    default: return "NONE";
    }
}

/* ---- IM-005 ---- */
int zhp_hexdump(const uint8_t *data, size_t len, char *out, size_t cap)
{
    sb_t s;
    size_t off, i, n;
    if (len > 65536u || (len && !data)) {
        if (cap) {
            out[0] = '\0';
        }
        return -1;
    }
    s.p = out;
    s.cap = cap;
    s.len = 0;
    s.overflow = 0;
    for (off = 0; off < len; off += 16) {
        n = len - off < 16 ? len - off : 16;
        sb_hex(&s, (uint32_t)off, 4);
        sb_putc(&s, ':');
        for (i = 0; i < n; i++) {
            sb_putc(&s, ' ');
            sb_hex(&s, data[off + i], 2);
        }
        sb_putc(&s, '\n');
    }
    return sb_done(&s);
}

/* ---- IM-004 ---- */
void zhp_log_init(zhp_log *l, char *buf, size_t cap, size_t max)
{
    l->buf = buf;
    l->cap = cap;
    l->max = max < cap ? max : cap;
    l->len = 0;
    l->lines = 0;
    l->truncated = 0;
}

/* Appends one finished line (without its newline). Keeps room for the truncation mark. */
static int log_commit(zhp_log *l, const char *line, size_t n)
{
    const size_t mark = sizeof(ZHP_LOG_TRUNC_MARK) - 1;
    if (l->truncated) {
        return 0;
    }
    if (l->len + n + 1 + mark > l->max) {
        memcpy(l->buf + l->len, ZHP_LOG_TRUNC_MARK, mark);
        l->len += mark;
        l->truncated = 1;
        return 0;
    }
    memcpy(l->buf + l->len, line, n);
    l->len += n;
    l->buf[l->len++] = '\n';
    l->lines++;
    return 1;
}

int zhp_log_header(zhp_log *l, const char *build_id)
{
    zhp_kv kv;
    kv.key = "BUILD";
    kv.val = build_id ? build_id : "";
    return zhp_log_line(l, "ZHP-LOG 1", &kv, 1);
}

int zhp_log_line(zhp_log *l, const char *tag, const zhp_kv *kv, size_t n)
{
    char line[ZHP_LOG_MAX_LINE + 1];
    sb_t s;
    size_t k;
    const char *v;
    if (!l || !tag || (n && !kv)) {
        return -1;
    }
    s.p = line;
    s.cap = sizeof(line);
    s.len = 0;
    s.overflow = 0;
    sb_puts(&s, tag);
    for (k = 0; k < n; k++) {
        sb_putc(&s, ' ');
        sb_puts(&s, kv[k].key);
        sb_putc(&s, '=');
        for (v = kv[k].val; *v; v++) {
            unsigned char c = (unsigned char)*v;
            if (c < 0x21 || c > 0x7E || c == '\\') {
                sb_putc(&s, '\\');
                sb_putc(&s, 'x');
                sb_hex(&s, c, 2);
            } else {
                sb_putc(&s, (char)c);
            }
        }
    }
    if (s.overflow || s.len > ZHP_LOG_MAX_LINE) {
        /* cut and mark: the last visible character is '~' */
        line[ZHP_LOG_MAX_LINE - 1] = '~';
        s.len = ZHP_LOG_MAX_LINE;
    }
    return log_commit(l, line, s.len);
}
