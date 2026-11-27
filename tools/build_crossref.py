#!/usr/bin/env python3
"""Cruza as fontes de dados por VID:PID e gera data/derived/crossref.{json,md}.

Fontes (todas em data/):
  descriptors.json            descritores HID dos logs do topico do USB4MAPLE (tools/extract_descriptors.py)
  vidpid.csv                  pares VID:PID citados no topico
  nintendont-controllerconfigs/*.ini   configs por byte+mascara do Nintendont (dados, 43 arquivos)
  linux-extracts/xpad_table.csv        tabela de xpad.c do Linux (VID, PID, nome, XTYPE), gerada aqui de xpad.c
Classes (campo `class`), so com evidencia lida nos fontes:
  vendor-xbox : esta na tabela do xpad.c (protocolo Xbox, nao HID padrao)
  init        : precisa de comando de ativacao (DS3: hid-sony.c; Switch Pro: hid-nintendo.c)
  hid         : tem descritor HID com botoes nos logs, sem init visto (nao verificado no hid-*.c por dispositivo)
  desconhecido: nenhuma das anteriores
"""
import configparser
import csv
import glob
import io
import json
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
D = lambda *p: os.path.join(ROOT, "data", *p)

# init exigido, lido em hid-sony.c (sixaxis_set_operational_usb) e hid-nintendo.c (JC_USB_CMD_HANDSHAKE)
INIT = {("054C", "0268"): "DS3: feature report 0xF4 (hid-sony.c)",
        ("057E", "2009"): "Switch Pro: handshake USB 0x80/0x02 (hid-nintendo.c)"}


def load_xpad(path):
    rows = {}
    pat = re.compile(r'\{\s*0x([0-9a-fA-F]{4}),\s*0x([0-9a-fA-F]{4}),\s*"([^"]*)",\s*[^,]+,\s*(XTYPE_\w+)\s*\}')
    for m in pat.finditer(open(path, errors="replace").read()):
        rows[(m.group(1).upper(), m.group(2).upper())] = (m.group(3).strip(), m.group(4))
    return rows


def load_nintendont():
    out = {}
    for f in sorted(glob.glob(D("nintendont-controllerconfigs", "*.ini"))):
        txt = open(f, errors="replace").read().replace("\r", "")
        cp = configparser.RawConfigParser(strict=False)
        cp.optionxform = str
        try:
            cp.read_file(io.StringIO(txt))
        except configparser.Error:
            continue
        for sec in cp.sections():
            s = dict(cp.items(sec))
            if "VID" in s and "PID" in s:
                key = (s["VID"].upper().zfill(4), s["PID"].upper().zfill(4))
                out.setdefault(key, []).append({"file": os.path.basename(f), "name": sec,
                                                "dpad": s.get("DPAD"), "polltype": s.get("Polltype"),
                                                "buttons": {k: s[k] for k in ("A", "B", "X", "Y", "Z", "L", "R", "S", "Up", "Left") if k in s},
                                                "rumble": s.get("Rumble")})
    return out


def main():
    # XPAD_C=caminho do xpad.c (torvalds/linux, GPL-2.0) regenera a tabela; o codigo nao e guardado aqui
    os.makedirs(D("linux-extracts"), exist_ok=True)
    csvp = D("linux-extracts", "xpad_table.csv")
    if os.environ.get("XPAD_C"):
        xp = load_xpad(os.environ["XPAD_C"])
        with open(csvp, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["vid", "pid", "name", "xtype"])
            for (v, p), (n, t) in sorted(xp.items()):
                w.writerow([v, p, n, t])
    xp = {(r["vid"], r["pid"]): (r["name"], r["xtype"]) for r in csv.DictReader(open(csvp))}
    nd = load_nintendont()
    desc = json.load(open(D("derived", "descriptors.json")))
    forum = {(r["vid"].upper(), r["pid"].upper()) for r in csv.DictReader(open(D("dreamcast-talk-t17404", "vidpid.csv")))}
    byid = {}
    for r in desc:
        byid.setdefault((r["vid"], r["pid"]), []).append(r)
    keys = set(byid) | set(forum) | set(nd) | set(xp)
    rows = []
    for k in sorted(keys):
        ds = byid.get(k, [])
        best = max(ds, key=lambda r: r["summary"]["buttons"]) if ds else None
        cls = "desconhecido"
        if k in xp:
            cls = "vendor-xbox"
        elif k in INIT:
            cls = "init"
        elif best and best["summary"]["buttons"] > 0:
            cls = "hid"
        rows.append({"vid": k[0], "pid": k[1], "class": cls, "init": INIT.get(k, ""),
                     "name": (xp.get(k, ("",))[0] or (nd[k][0]["name"] if k in nd else "")),
                     "xtype": xp.get(k, ("", ""))[1],
                     "in_forum": k in forum, "in_nintendont": k in nd,
                     "descriptor": ({"buttons": best["summary"]["buttons"], "hat": bool(best["summary"]["hat"]),
                                     "axes": [a["name"] for a in best["summary"]["axes"]],
                                     "report_ids": best["summary"]["report_ids"]} if best else None),
                     "nintendont": nd.get(k, [])})
    os.makedirs(D("derived"), exist_ok=True)
    json.dump(rows, open(D("derived", "crossref.json"), "w"), indent=1, ensure_ascii=False)
    # tabela so dos que aparecem em 2+ fontes de "uso real" (descritor, forum, nintendont)
    sel = [r for r in rows if sum([r["descriptor"] is not None, r["in_forum"], r["in_nintendont"]]) >= 2 or r["init"] or (r["class"] == "vendor-xbox" and r["in_forum"])]
    with open(D("derived", "crossref.md"), "w") as f:
        f.write(f"{len(rows)} VID:PID no total; {len(sel)} com 2+ fontes (descritor, forum, Nintendont), init conhecido, ou Xbox (xpad) citado no forum.\n\n")
        f.write("| VID:PID | classe | nome | descritor (botoes/hat/eixos) | forum | Nintendont | init |\n|---|---|---|---|---|---|---|\n")
        for r in sel:
            d = r["descriptor"]
            ds = f"{d['buttons']}b/{'hat' if d['hat'] else '-'}/{len(d['axes'])}ax" if d else "-"
            f.write(f"| {r['vid']}:{r['pid']} | {r['class']} | {r['name'][:38]} | {ds} | {'s' if r['in_forum'] else '-'} | "
                    f"{'s' if r['in_nintendont'] else '-'} | {r['init'][:40]} |\n")
    from collections import Counter
    print(len(rows), "VID:PID;", Counter(r["class"] for r in rows), "; xpad", len(xp), "; nintendont", len(nd))


if __name__ == "__main__":
    main()
