#!/usr/bin/env python3
"""Extrai os descritores HID dos logs UART postados no topico do USB4MAPLE e resume cada um.

Entrada : data/dreamcast-talk-t17404/posts.json
Saida   : data/derived/descriptors.json  (um item por log com descritor)
          data/derived/descriptors.md    (tabela)
O parser de descritor e minimo e escrito aqui (HID 1.11, secao 6.2.2); nao copia codigo de terceiros.
"""
import json
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
POSTS = os.path.join(ROOT, "data/dreamcast-talk-t17404/posts.json")
OUT = os.path.join(ROOT, "data/derived")

HEX_LINE = re.compile(r"^\s*(?:[0-9A-Fa-f]{2}\s+)*[0-9A-Fa-f]{2}\s*$")
AXIS_NAMES = {0x30: "X", 0x31: "Y", 0x32: "Z", 0x33: "Rx", 0x34: "Ry", 0x35: "Rz",
              0x36: "Slider", 0x37: "Dial", 0x38: "Wheel", 0x39: "Hat"}


def read_items(data):
    """Rende (tag_byte, valor_sem_sinal, valor_com_sinal, tamanho, offset_inicial)."""
    i = 0
    while i < len(data):
        b = data[i]
        if b == 0xFE:  # item longo: nao usado em descritores de gamepad
            return
        size = (0, 1, 2, 4)[b & 3]
        if i + 1 + size > len(data):
            return
        raw = int.from_bytes(data[i + 1:i + 1 + size], "little") if size else 0
        signed = raw - (1 << (8 * size)) if size and raw >= 1 << (8 * size - 1) else raw
        yield b & 0xFC, raw, signed, size, i
        i += 1 + size


def cut_descriptor(data):
    """Corta no fim do ultimo coletor de nivel 0 que fecha antes de bytes que nao sao descritor."""
    depth = 0
    end = None
    for tag, raw, _, _, off in read_items(data):
        if tag == 0xA0:
            depth += 1
        elif tag == 0xC0:
            depth -= 1
            if depth == 0:
                end = off + 1
        elif depth == 0 and end is not None and tag not in (0x04, 0x84):  # nao e Usage Page nem Report ID
            break
        if depth < 0:
            break
    return data[:end] if end else data


def parse(data):
    g = {"page": 0, "lmin": 0, "lmax": 0, "size": 0, "count": 0, "rid": 0}
    stack = []
    local = {"usages": [], "umin": None, "umax": None}
    offs = {}          # report id -> bit offset corrente (entrada)
    fields = []
    for tag, raw, signed, size, _ in read_items(data):
        if tag == 0x04:
            g["page"] = raw
        elif tag == 0x14:
            g["lmin"] = signed
        elif tag == 0x24:
            g["lmax"] = signed if g["lmin"] < 0 else raw
        elif tag == 0x74:
            g["size"] = raw
        elif tag == 0x94:
            g["count"] = raw
        elif tag == 0x84:
            g["rid"] = raw
        elif tag == 0xA4:
            stack.append(dict(g))
        elif tag == 0xB4 and stack:
            g = stack.pop()
        elif tag == 0x08:
            local["usages"].append((g["page"], raw))
        elif tag == 0x18:
            local["umin"] = raw
        elif tag == 0x28:
            local["umax"] = raw
        elif tag == 0x80:  # Input
            base = offs.get(g["rid"], 0)
            const = bool(raw & 1)
            usages = list(local["usages"])
            if local["umin"] is not None and local["umax"] is not None:
                usages = [(g["page"], u) for u in range(local["umin"], local["umax"] + 1)]
            for n in range(g["count"]):
                u = usages[n] if n < len(usages) else (usages[-1] if usages else None)
                kind = "pad"
                if not const and u:
                    if u[0] == 9:
                        kind = "button"
                    elif u[0] == 1 and u[1] == 0x39:
                        kind = "hat"
                    elif u[0] == 1 and u[1] in AXIS_NAMES:
                        kind = "axis"
                    else:
                        kind = "other"
                fields.append({"rid": g["rid"], "bit": base + n * g["size"], "size": g["size"], "kind": kind,
                               "page": u[0] if u else None, "usage": u[1] if u else None,
                               "lmin": g["lmin"], "lmax": g["lmax"]})
            offs[g["rid"]] = base + g["count"] * g["size"]
        if tag in (0x80, 0x90, 0xB0, 0xA0, 0xC0):
            local = {"usages": [], "umin": None, "umax": None}
    return fields


def summarize(fields):
    btn = [f for f in fields if f["kind"] == "button"]
    hat = [f for f in fields if f["kind"] == "hat"]
    axes = [f for f in fields if f["kind"] == "axis"]
    other = [f for f in fields if f["kind"] == "other"]
    return {
        "report_ids": sorted({f["rid"] for f in fields}),
        "buttons": len(btn),
        "button_bits": [f["bit"] for f in btn],
        "hat": [{"bit": f["bit"], "size": f["size"], "lmin": f["lmin"], "lmax": f["lmax"]} for f in hat],
        "axes": [{"name": AXIS_NAMES[f["usage"]], "bit": f["bit"], "size": f["size"]} for f in axes],
        "other_fields": len(other),
        "input_bits": max([f["bit"] + f["size"] for f in fields], default=0),
    }


def device_name(text):
    for line in text.splitlines():
        s = line.strip()
        if not s or s.lower().startswith(("code", "starting", "hid ", "a massstorage", "vid", "protocol")):
            continue
        if re.fullmatch(r"(?:[0-9A-Fa-f]{2}\s?)+", s):
            continue
        return s[:80]
    return ""


def main():
    data = json.load(open(POSTS))
    posts = []
    for pg in sorted(data, key=int):
        for p in data[pg]:
            p["page"] = int(pg) + 1
            posts.append(p)
    rows = []
    for p in posts:
        t = p["text"]
        for m in re.finditer(r"HID receive desc report:?[ \t]*\r?\n", t):
            lines, buf = t[m.end():].splitlines(), []
            for ln in lines:
                if HEX_LINE.match(ln):
                    buf += [int(x, 16) for x in ln.split()]
                elif ln.strip() == "" and not buf:
                    continue
                else:
                    break
            if len(buf) < 8:
                continue
            desc = bytes(buf)
            desc = cut_descriptor(desc)
            head = t[:m.start()]
            ids = re.findall(r"VID = ?([0-9A-Fa-f]{4}),? ?PID = ?([0-9A-Fa-f]{4})", head)
            vid, pid = (ids[-1] if ids else ("", ""))
            try:
                fields = parse(desc)
            except Exception as e:  # descritor truncado/ruim
                fields, err = [], str(e)
            rows.append({"page": p["page"], "post": p["id"], "author": p["author"], "when": p["when"][-24:].strip(),
                         "name": device_name(t), "vid": vid.upper(), "pid": pid.upper(),
                         "descriptor_len": len(desc), "descriptor_hex": desc.hex(" ").upper(),
                         "summary": summarize(fields)})
    os.makedirs(OUT, exist_ok=True)
    json.dump(rows, open(os.path.join(OUT, "descriptors.json"), "w"), indent=1, ensure_ascii=False)
    with open(os.path.join(OUT, "descriptors.md"), "w") as f:
        f.write("| pag | VID:PID | nome (1a linha do post) | bytes | RID | botoes | hat | eixos | outros |\n|---|---|---|---|---|---|---|---|---|\n")
        for r in rows:
            s = r["summary"]
            hat = ",".join(f"b{h['bit']}/{h['size']}" for h in s["hat"]) or "-"
            ax = " ".join(f"{a['name']}@{a['bit']}/{a['size']}" for a in s["axes"]) or "-"
            f.write(f"| {r['page']} | {r['vid']}:{r['pid']} | {r['name']} | {r['descriptor_len']} | {s['report_ids']} | "
                    f"{s['buttons']} | {hat} | {ax} | {s['other_fields']} |\n")
    print(len(rows), "descritores ->", OUT)


if __name__ == "__main__":
    sys.exit(main())
