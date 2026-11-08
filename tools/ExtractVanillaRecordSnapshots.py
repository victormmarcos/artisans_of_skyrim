#!/usr/bin/env python3
"""Merge exact source records from local Bethesda masters into the snapshot table."""

from __future__ import annotations

import argparse
import json
import struct
import zlib
from pathlib import Path


TARGETS = {
    "skyrim": {
        0x0007A0F6,  # EnchFortifyDestructionConstantSelf
        0x000E8279,  # MQGreybeardsFus
        0x0009F25C,  # SkyforgeSteelSword
        0x0009F25D,  # SkyforgeSteelDagger
        0x0009F25E,  # SkyforgeSteelGreatsword
        0x0009F25F,  # SkyforgeSteelBattleaxe
        0x0009F260,  # SkyforgeSteelWarAxe
        0x000F46AA,  # TemperWeaponSkyforgeBattleaxe
        0x000F46AB,  # TemperWeaponSkyforgeDagger
        0x000F46AC,  # TemperWeaponSkyforgeGreatsword
        0x000F46AE,  # TemperWeaponSkyforgeSword
        0x000F46AF,  # TemperWeaponSkyforgeWarAxe
    },
}


def iter_fields(payload: bytes):
    offset = 0
    extended_size = None
    while offset + 6 <= len(payload):
        signature = payload[offset : offset + 4].decode("ascii", errors="replace")
        size = struct.unpack_from("<H", payload, offset + 4)[0]
        offset += 6
        if signature == "XXXX":
            if size != 4 or offset + 4 > len(payload):
                raise ValueError("Malformed XXXX field")
            extended_size = struct.unpack_from("<I", payload, offset)[0]
            offset += 4
            continue
        if extended_size is not None:
            size = extended_size
            extended_size = None
        data = payload[offset : offset + size]
        if len(data) != size:
            raise ValueError(f"Truncated {signature} field")
        offset += size
        yield signature, data


def decode_payload(raw: bytes, flags: int) -> bytes:
    if not flags & 0x00040000:
        return raw
    expected = struct.unpack_from("<I", raw, 0)[0]
    result = zlib.decompress(raw[4:])
    if len(result) != expected:
        raise ValueError("Compressed record size mismatch")
    return result


def scan_region(
    blob: bytes,
    start: int,
    end: int,
    targets: set[int],
    found: dict[int, dict[str, object]],
) -> None:
    offset = start
    while offset + 24 <= end:
        signature = blob[offset : offset + 4]
        if signature == b"GRUP":
            group_size = struct.unpack_from("<I", blob, offset + 4)[0]
            if group_size < 24 or offset + group_size > end:
                raise ValueError(f"Malformed GRUP at {offset:#x}")
            scan_region(blob, offset + 24, offset + group_size, targets, found)
            offset += group_size
            continue

        data_size, flags, form_id = struct.unpack_from("<III", blob, offset + 4)
        record_end = offset + 24 + data_size
        if record_end > end:
            raise ValueError(f"Truncated record at {offset:#x}")
        if form_id in targets:
            payload = decode_payload(blob[offset + 24 : record_end], flags)
            fields = list(iter_fields(payload))
            editor_id = next(
                (
                    data.rstrip(b"\0").decode("utf-8", errors="replace")
                    for field_signature, data in fields
                    if field_signature == "EDID"
                ),
                "",
            )
            found[form_id] = {
                "signature": signature.decode("ascii", errors="replace"),
                "editor_id": editor_id,
                "flags": flags,
                "fields": [
                    {"signature": field_signature, "data": data.hex()}
                    for field_signature, data in fields
                ],
            }
        offset = record_end


def extract(master: Path, targets: set[int]) -> dict[int, dict[str, object]]:
    blob = master.read_bytes()
    found: dict[int, dict[str, object]] = {}
    scan_region(blob, 0, len(blob), targets, found)
    missing = sorted(targets - found.keys())
    if missing:
        raise ValueError(
            f"{master.name} is missing requested records: "
            + ", ".join(f"{form_id:08X}" for form_id in missing)
        )
    return found


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--skyrim", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    snapshots = json.loads(args.output.read_text(encoding="utf-8"))
    for form_id, record in extract(args.skyrim, TARGETS["skyrim"]).items():
        snapshots[f"{form_id:08X}"] = record

    ordered = {key: snapshots[key] for key in sorted(snapshots)}
    args.output.write_text(
        json.dumps(ordered, indent=2) + "\n",
        encoding="utf-8",
    )
    print(
        "Merged vanilla snapshots: "
        + ", ".join(
            f"{form_id:08X}"
            for form_id in sorted(TARGETS["skyrim"])
        )
    )


if __name__ == "__main__":
    main()
