#!/usr/bin/env python3
"""Check the shipped PE metadata, legacy ESP format, and compiled menu."""
import hashlib
import json
from pathlib import Path
import struct
import zlib

import project_version

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = [0x01050610, 0x010613D0, 0x010613E0, 0x01061430, 0x01061560,
    0x01061610, 0x01062750, 0x01062800, 0x01062931, 0x010646A0,
    0x01064920, 0x010649B1, 0x01070630, 0x01070680]

def validate_dll(data):
    u16 = lambda at: struct.unpack_from("<H", data, at)[0]
    u32 = lambda at: struct.unpack_from("<I", data, at)[0]
    pe = u32(0x3C)
    assert data[:2] == b"MZ" and data[pe:pe+4] == b"PE\0\0"
    assert u16(pe+4) == 0x8664
    opt = pe+24
    sec = opt+u16(pe+20)
    sections = [(u32(sec+i*40+12), u32(sec+i*40+8), u32(sec+i*40+20), u32(sec+i*40+16)) for i in range(u16(pe+6))]
    def offset(rva):
        for va, size, raw, raw_size in sections:
            if va <= rva < va+max(size,raw_size): return raw+rva-va
        raise AssertionError(f"Unmapped RVA {rva:x}")
    def string(at): return data[at:data.index(b"\0", at)].decode("ascii")
    export = offset(u32(opt+112))
    names, ordinals, funcs = offset(u32(export+32)), offset(u32(export+36)), offset(u32(export+28))
    exports = {string(offset(u32(names+i*4))): offset(u32(funcs+u16(ordinals+i*2)*4)) for i in range(u32(export+24))}
    assert {"SKSEPlugin_Version", "SKSEPlugin_Query", "SKSEPlugin_Load"} <= exports.keys()
    version = exports["SKSEPlugin_Version"]
    assert u32(version+4) == project_version.PACKED, "Wrong mod version in shipped DLL"
    assert u32(version+0x308) == 0, "Explicit runtime allowlist expected"
    runtimes = [u32(version+0x30C+i*4) for i in range(16)]
    assert runtimes == EXPECTED+[0,0], "Wrong runtime/store identifiers in shipped DLL"
    debug_size = u32(opt+112+6*8+4)
    if debug_size:
        debug = offset(u32(opt+112+6*8))
        kinds = [u32(debug+i+12) for i in range(0,debug_size,28)]
        assert 2 not in kinds, "Release must not ship a CodeView/PDB path"

    return runtimes

def validate_esp(data):
    assert data[:4] == b"TES4"
    size = struct.unpack_from("<I", data, 4)[0]
    pos = 24
    while pos < 24+size:
        sig, length = struct.unpack_from("<4sH", data, pos)
        body = data[pos+6:pos+6+length]
        if sig == b"HEDR": assert abs(struct.unpack_from("<f", body)[0]-1.70)<0.00001
        pos += 6+length
    def records(start,end):
        while start<end:
            sig, n = struct.unpack_from("<4sI", data, start)
            if sig == b"GRUP":
                yield from records(start+24,start+n);start+=n
            else:
                yield struct.unpack_from("<I",data,start+12)[0];start+=24+n
        assert start==end
    ids = [form for form in records(0,len(data)) if form]
    assert all(0x800 <= form & 0xFFFFFF <= 0xFFF for form in ids), "Expanded ESL range requires a newer engine"

def validate_swf(data):
    assert data[:3] in (b"FWS",b"CWS") and data[3] == 8
    body = data[8:] if data[:3] == b"FWS" else zlib.decompress(data[8:])
    assert len(body)+8 == struct.unpack_from("<I",data,4)[0]
    for api in (b"setPage", b"setInputMode", b"aosCommand", b"navigate",
                b"artisan", b"tier", b"profession",
                b"costDefined", b"effectValue", b"statusValue",
                b"carry", b"capacity", b"Carry Weight", b"picked",
                b"canFinalize", b"finalizePrice", b"selectionOnePrice",
                b"Rename item", b"renameText", b"textOn", b"textOff",
                b"selectionTwoPrice", b"craftSelection", b"notice", b"wheelUp", b"Result Quality",
                b"labels", b"Search", b"setSort", b"selectCategory"):
        assert api in body, f"Missing menu contract field {api!r}"
    for removed in (b"Received weight", b"weightDelta", b"previewDrag", b"cycleSort", b"cycleGroup", b"cycleCategory"):
        assert removed not in body, f"Removed menu element remains in SWF: {removed!r}"
    return body

def main():
    validate_dll((ROOT/"dist/Data/SKSE/Plugins/ArtisansOfSkyrim.dll").read_bytes())
    validate_esp((ROOT/"dist/Data/ArtisansOfSkyrim.esp").read_bytes())
    validate_swf((ROOT/"dist/Data/Interface/ArtisansOfSkyrim/ServiceMenu.swf").read_bytes())
    manifest = json.loads((ROOT/"interface/build-manifest.json").read_text())
    for file, expected in manifest.items():
        assert hashlib.sha256((ROOT/file).read_bytes()).hexdigest() == expected, f"Recompile UI: {file} changed"
    print(f"Shipped DLL: {project_version.TEXT}, 14 exact runtime/store IDs, GOG subtype, no CodeView/PDB path: OK")
    print("ESP: header 1.70 and legacy ESL record range: OK")
    print("SWF: Flash 8 / AS2 contract and matching source hashes: OK")

if __name__ == "__main__":
    main()
