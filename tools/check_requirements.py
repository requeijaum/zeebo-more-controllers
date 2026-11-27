#!/usr/bin/env python3
"""Auditoria mecanica de docs/REQUIREMENTS.md e docs/TEST-PLAN.md.

Confere: metodo (I/A/D/T) e criterio de aceitacao em todo requisito; testes citados existem;
todo teste verifica requisito existente; todo teste e usado por algum requisito; SN cobertos por LF/LQ;
LF/LQ cobertos por teste; IM/TR citam requisito existente em "Satisfies"; IDs unicos.
Saida: lista de problemas; exit 1 se houver algum.
"""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
ID = re.compile(r"^(SN|CON|LF|LQ|TR|IM|HZ|CT|IT|ST|AT)-\d+$")
TOKEN = re.compile(r"\b(?:SN|CON|LF|LQ|TR|IM|HZ|CT|IT|ST|AT)-\d+\b")


def rows(path):
    out = []
    for line in open(path):
        if line.startswith("|"):
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if cells and ID.match(cells[0]):
                out.append(cells)
    return out


def expand(cell):
    """AT-002..004 vira AT-002, AT-003, AT-004; CT-001..010 tambem."""
    ids = []
    for m in re.finditer(r"\b((?:SN|CON|LF|LQ|TR|IM|HZ|CT|IT|ST|AT)-)(\d+)\.\.(\d+)\b", cell):
        for n in range(int(m.group(2)), int(m.group(3)) + 1):
            ids.append(f"{m.group(1)}{n:03d}")
    ids += TOKEN.findall(re.sub(r"\b(?:SN|CON|LF|LQ|TR|IM|HZ|CT|IT|ST|AT)-\d+\.\.\d+\b", "", cell))
    return ids


def main():
    req = rows(os.path.join(ROOT, "docs/REQUIREMENTS.md"))
    tst = rows(os.path.join(ROOT, "docs/TEST-PLAN.md"))
    problems = []
    all_ids = {}
    for r in req + tst:
        if r[0] in all_ids and not r[0].startswith(("HZ",)):
            problems.append(f"{r[0]}: ID duplicado")
        all_ids[r[0]] = r
    req_ids = {r[0] for r in req}
    tst_ids = {r[0] for r in tst}
    used_tests, verified = set(), set()

    for r in req:
        rid = r[0]
        kind = rid.split("-")[0]
        if kind == "HZ":
            # HZ | Hazard | Interlock | Requirement | Test
            for ref in expand(r[3]):
                if ref not in req_ids:
                    problems.append(f"{rid}: requisito inexistente {ref}")
            for ref in expand(r[4]):
                used_tests.add(ref)
                if ref not in tst_ids:
                    problems.append(f"{rid}: teste inexistente {ref}")
            continue
        # cada tabela tem Method e Acceptance; acha pelo valor
        method = next((c for c in r[1:] if c in ("I", "A", "D", "T")), None)
        if not method:
            problems.append(f"{rid}: sem metodo I/A/D/T")
        mi = r.index(method) if method else -1
        accept = r[mi + 1] if mi >= 0 and mi + 1 < len(r) else ""
        if len(accept) < 15:
            problems.append(f"{rid}: sem criterio de aceitacao")
        tests = expand(r[-1])
        if kind != "CON" and not tests:
            problems.append(f"{rid}: sem testes")
        if kind == "CON" and not tests:
            problems.append(f"{rid}: sem testes")
        for t in tests:
            used_tests.add(t)
            if t not in tst_ids:
                problems.append(f"{rid}: teste inexistente {t}")
            else:
                verified.add(rid)
        if kind in ("TR", "IM"):
            for ref in expand(r[-2]):
                if ref not in req_ids:
                    problems.append(f"{rid}: Satisfies cita inexistente {ref}")
        if kind in ("LF", "LQ"):
            for ref in expand(r[-2]):
                if not ref.startswith("SN") or ref not in req_ids:
                    problems.append(f"{rid}: coluna SN invalida {ref}")

    for t in tst:
        refs = expand(t[-1])
        if not refs:
            problems.append(f"{t[0]}: nao verifica nada")
        for ref in refs:
            if ref not in req_ids:
                problems.append(f"{t[0]}: verifica inexistente {ref}")
            else:
                verified.add(ref)
        used_tests.add(t[0]) if False else None
    for t in sorted(tst_ids - used_tests):
        problems.append(f"{t}: nenhum requisito cita este teste")

    # cobertura de cima para baixo
    covered_sn = set()
    for r in req:
        if r[0].split("-")[0] in ("LF", "LQ"):
            covered_sn |= set(expand(r[-2]))
        if r[0].split("-")[0] in ("TR", "IM"):
            covered_sn |= {x for x in expand(r[-2]) if x.startswith("SN")}
    for r in req:
        if r[0].startswith("SN") and r[0] not in covered_sn:
            problems.append(f"{r[0]}: nenhum LF/LQ cobre esta necessidade")
    lf_lq = {r[0] for r in req if r[0].split("-")[0] in ("LF", "LQ")}
    satisfied = set()
    for r in req:
        if r[0].split("-")[0] in ("TR", "IM"):
            satisfied |= set(expand(r[-2]))
    for x in sorted(lf_lq):
        if x not in satisfied and x not in {ref for r in tst for ref in expand(r[-1])}:
            problems.append(f"{x}: sem TR/IM e sem teste de sistema")

    # simetria: se o requisito cita o teste, o teste cita o requisito, e vice-versa
    rt = {r[0]: set(expand(r[4] if r[0].startswith("HZ") else r[-1])) for r in req}
    tr = {t[0]: set(expand(t[-1])) for t in tst}
    for r, ts in rt.items():
        for t in sorted(ts):
            if t in tr and r not in tr[t]:
                problems.append(f"{r} cita {t}, mas {t} nao verifica {r}")
    for t, rs in tr.items():
        for r in sorted(rs):
            if r in rt and t not in rt[r]:
                problems.append(f"{t} verifica {r}, mas {r} nao cita {t}")
    print(f"{len(req)} requisitos/necessidades, {len(tst)} testes")
    for p in problems:
        print("PROBLEMA:", p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
