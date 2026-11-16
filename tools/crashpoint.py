# Which recompiled function a crash dump's exception is in.
# usage: python crashpoint.py <dump.dmp> <CrashNitroKart.dll exports listing from llvm-objdump -p>
import struct, sys, re, bisect

dump = open(sys.argv[1], 'rb').read()
sig, ver, nstreams, dir_rva = struct.unpack_from('<4sIII', dump, 0)
streams = {}
for i in range(nstreams):
    t, size, rva = struct.unpack_from('<III', dump, dir_rva + i * 12)
    streams[t] = (size, rva)


def mdstring(rva):
    n = struct.unpack_from('<I', dump, rva)[0]
    return dump[rva + 4: rva + 4 + n].decode('utf-16le')


mods = []
size, rva = streams[4]  # ModuleListStream
count = struct.unpack_from('<I', dump, rva)[0]
for i in range(count):
    base, msize, _, _, name_rva = struct.unpack_from('<QIIII', dump, rva + 4 + i * 108)
    mods.append((base, msize, mdstring(name_rva)))

size, rva = streams[6]  # ExceptionStream
thread_id = struct.unpack_from('<I', dump, rva)[0]
code, flags, rec, addr = struct.unpack_from('<IIQQ', dump, rva + 8)
print(f'exception {code:#x} at {addr:#x} (thread {thread_id})')
mod = next((m for m in mods if m[0] <= addr < m[0] + m[1]), None)
print('module:', mod[2] if mod else '?', hex(mod[0]) if mod else '')

exports = []
for l in open(sys.argv[2], encoding='utf-8', errors='replace'):
    m = re.match(r'^\s+\d+\s+0x([0-9a-f]+)\s+__imp__sub_([0-9A-F]+)', l)
    if m:
        exports.append((int(m.group(1), 16), m.group(2)))
exports.sort()
if mod and mod[2].lower().endswith('crashnitrokart.dll'):
    off = addr - mod[0]
    i = bisect.bisect_right([e[0] for e in exports], off) - 1
    if i >= 0:
        print(f'in sub_{exports[i][1]} + {off - exports[i][0]:#x} (host rva {off:#x})')
