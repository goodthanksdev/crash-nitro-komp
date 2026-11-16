# Candidates for the XDK functions xbe_symbols left at 0: shift-aware constants (5849 -> CNK 5558 device offsets)
# plus how close the function's length is. usage: python candidates.py cnk.asm game.toml 5849/signatures.toml
import re, sys, collections

asm, cfg, sig = sys.argv[1:4]
mapped, missing = {}, []
for l in open(cfg, encoding='utf-8'):
    m = re.match(r'^(\w+)\s*=\s*0x([0-9A-Fa-f]+)', l)
    if m and int(m.group(2), 16):
        mapped[int(m.group(2), 16)] = m.group(1)
    m2 = re.match(r'^(\w+)\s*=\s*0\s*(#.*)?$', l)
    if m2:
        missing.append(m2.group(1))

sigs = {}
for l in open(sig, encoding='utf-8'):
    m = re.match(r'^(\w+) = \{(.*)\}\s*$', l)
    if not m or m.group(1) not in missing:
        continue
    body = m.group(2)
    ln = re.search(r'len = 0x([0-9A-F]+)', body)
    k = re.search(r'k = \[([^\]]*)\]', body)
    sigs[m.group(1)] = (int(ln.group(1), 16) if ln else 0,
                        [int(x, 16) for x in re.findall(r'0x[0-9A-F]+', k.group(1))] if k else [])

lines = collections.defaultdict(list)
cur = None
for l in open(asm, encoding='utf-8', errors='replace'):
    m = re.match(r'^([0-9a-f]{8}) <', l)
    if m:
        cur = int(m.group(1), 16)
        continue
    m = re.match(r'^\s+([0-9a-f]+):\s+(.*)$', l)
    if m and cur is not None:
        lines[cur].append((int(m.group(1), 16), m.group(2)))


def consts(ins):
    out = []
    for _, t in ins:
        if 'esp' in t or 'call' in t or t.startswith('j'):
            continue
        for x in re.findall(r'0x[0-9a-f]+', t):
            v = int(x, 16)
            if v < 0x40 or v >= 0xFFFFFF00 or 0x10000 <= v < 0x4000000:
                continue
            out.append(v)
    return out


def shifted(v):
    if 0x774 <= v < 0xA00:
        return v - 4
    if 0xC00 <= v < 0x2500:
        return v - 0x10
    return v


lo, hi = (int(x, 16) for x in (sys.argv[4] if len(sys.argv) > 4 else '0x372000-0x37C000').split('-'))
info = {}
for s, ins in lines.items():
    if lo <= s < hi and ins:
        info[s] = (ins[-1][0] - s + 1, collections.Counter(consts(ins)))

prefix = tuple((sys.argv[5] if len(sys.argv) > 5 else 'D3D,CDevice').split(','))
for name, (ln, k) in sorted(sigs.items()):
    if not name.startswith(prefix):
        continue
    res = []
    for s, (fl, ck) in info.items():
        if s in mapped:
            continue
        left = ck.copy()
        both = 0
        for v in k:
            for w in (v, shifted(v)):
                if left[w] > 0:
                    left[w] -= 1
                    both += 1
                    break
        union = len(k) + sum(ck.values()) - both
        kscore = both / union if union else 0
        lscore = 1 - min(1, abs(fl - ln) / max(ln, 1)) if ln else 0.5
        score = (0.7 * kscore + 0.3 * lscore) if k else 0.4 * lscore
        res.append((score, s, fl, both))
    res.sort(reverse=True)
    top = ' | '.join(f'{s:#x} len={fl:#x} k{b}/{len(k)} {sc:.2f}' for sc, s, fl, b in res[:3])
    print(f'{name:48s} len={ln:#x} k={len(k)}: {top}')
