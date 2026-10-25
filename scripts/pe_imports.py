#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Dump the real import table of a PE file (regular + delay imports).

Standard library only, so it also runs on msys2 python inside CI.

Usage:
  pe-imports.py <file>            human-readable dump
  pe-imports.py --list <file>     one imported dll name per line (for BFS)
"""
import mmap
import struct
import sys
import io

if __name__ == "__main__":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

def imports(path):
    f = open(path, "rb")
    data = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[e_lfanew:e_lfanew+4] == b"PE\0\0", "not PE"
    coff = e_lfanew + 4
    nsec = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic == 0x20B:
        ddir = opt + 112
    elif magic == 0x10B:
        ddir = opt + 96
    else:
        raise SystemExit("unknown PE magic %#x" % magic)
    # DataDirectory: [0]=export [1]=import [13]=delay import
    imp_rva = struct.unpack_from("<I", data, ddir + 8 * 1)[0]
    dly_rva = struct.unpack_from("<I", data, ddir + 8 * 13)[0]
    sec_off = opt + opt_size
    sections = []
    for i in range(nsec):
        o = sec_off + i * 40
        vsize = struct.unpack_from("<I", data, o + 8)[0]
        va = struct.unpack_from("<I", data, o + 12)[0]
        rawsize = struct.unpack_from("<I", data, o + 16)[0]
        raw = struct.unpack_from("<I", data, o + 20)[0]
        sections.append((va, vsize, raw, rawsize))

    def rva_to_off(rva):
        for va, vsize, raw, rawsize in sections:
            if rawsize and va <= rva < va + max(vsize, rawsize):
                off = raw + (rva - va)
                if off < len(data):
                    return off
        return None

    def cstr(off):
        end = data.find(b"\0", off)
        if end < 0:
            end = off + 260
        return data[off:end].decode("ascii", "replace")

    result = []
    def walk(rva, kind, entry_size):
        off = rva_to_off(rva) if rva else None
        if off is None:
            return
        while off + entry_size <= len(data):
            if kind == "import":
                # IMAGE_IMPORT_DESCRIPTOR: OriginalFirstThunk, TimeDateStamp,
                # ForwarderChain, Name, FirstThunk
                name_loc = struct.unpack_from("<I", data, off + 12)[0]
            else:
                # IMAGE_DELAYLOAD_DESCRIPTOR: grAttrs, szName, phmod, pIAT,
                # pINT, pBoundIAT, pUnloadIAT
                gr, name_loc = struct.unpack_from("<II", data, off)
                if name_loc == 0:
                    break
                if gr & 1:  # szName is an absolute VA -> convert to RVA
                    for va, vsize, raw, rawsize in sections:
                        if va <= name_loc < va + max(vsize, rawsize):
                            name_loc = name_loc - va + raw
                            break
            if name_loc == 0:
                break
            noff = rva_to_off(name_loc)
            if noff is None:
                break
            try:
                result.append((kind, cstr(noff)))
            except Exception:
                break
            off += entry_size

    walk(imp_rva, "import", 20)
    walk(dly_rva, "delay", 32)
    return result

if __name__ == "__main__":
    args = sys.argv[1:]
    list_mode = False
    if args and args[0] == "--list":
        list_mode = True
        args = args[1:]
    if not args:
        raise SystemExit(__doc__)
    for kind, name in imports(args[0]):
        print(name if list_mode else f"{kind:6} {name}")
