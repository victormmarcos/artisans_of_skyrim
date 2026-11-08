#!/usr/bin/env python3
"""Audit the reviewed SharedInfo table against local vanilla masters and voices.

The game files are supplied as command-line arguments and are never copied into
the project.  This checks both halves of a SharedInfo response: the localized
subtitle stored in the source INFO record and a FUZ asset for the selected
VoiceType.
"""

from __future__ import annotations

import argparse
import csv
import mmap
import re
import struct
import zlib
from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RESPONSES_PATH = ROOT / "source-data" / "dialogue" / "vanilla_voice_responses.csv"
COMPRESSED_RECORD = 0x00040000
FUZ_RE = re.compile(r"_([0-9a-f]{8})_(\d+)\.fuz$", re.IGNORECASE)


@dataclass(frozen=True)
class Record:
    signature: str
    form_id: int
    body: bytes


def load_strings(path: Path) -> dict[int, str]:
    blob = path.read_bytes()
    count, _data_size = struct.unpack_from("<II", blob, 0)
    entries = [struct.unpack_from("<II", blob, 8 + index * 8) for index in range(count)]
    data_start = 8 + count * 8
    length_prefixed = path.suffix.lower() in {".dlstrings", ".ilstrings"}
    result: dict[int, str] = {}
    for string_id, relative_offset in entries:
        cursor = data_start + relative_offset
        if length_prefixed:
            length = struct.unpack_from("<I", blob, cursor)[0]
            raw = blob[cursor + 4 : cursor + 4 + length].rstrip(b"\0")
        else:
            end = blob.index(b"\0", cursor)
            raw = blob[cursor:end]
        result[string_id] = raw.decode("utf-8", errors="replace")
    return result


def iter_records(blob: bytes, start: int = 0, end: int | None = None) -> Iterator[Record]:
    if end is None:
        end = len(blob)
    cursor = start
    while cursor < end:
        signature = blob[cursor : cursor + 4]
        if signature == b"GRUP":
            size = struct.unpack_from("<I", blob, cursor + 4)[0]
            yield from iter_records(blob, cursor + 24, cursor + size)
            cursor += size
            continue
        if len(signature) < 4:
            return
        size, flags, form_id = struct.unpack_from("<III", blob, cursor + 4)
        body = blob[cursor + 24 : cursor + 24 + size]
        if flags & COMPRESSED_RECORD:
            expected = struct.unpack_from("<I", body, 0)[0]
            body = zlib.decompress(body[4:])
            assert len(body) == expected
        yield Record(signature.decode("ascii"), form_id, body)
        cursor += 24 + size


def fields(body: bytes) -> Iterator[tuple[str, bytes]]:
    cursor = 0
    extended_size: int | None = None
    while cursor < len(body):
        signature = body[cursor : cursor + 4].decode("ascii", errors="replace")
        size = struct.unpack_from("<H", body, cursor + 4)[0]
        cursor += 6
        data = body[cursor : cursor + size]
        cursor += size
        if signature == "XXXX":
            extended_size = struct.unpack_from("<I", data, 0)[0]
            continue
        if extended_size is not None:
            start = cursor - size
            data = body[start : start + extended_size]
            cursor = start + extended_size
            extended_size = None
        yield signature, data


def master_subtitles(master: Path, strings_dir: Path) -> dict[int, set[str]]:
    strings: dict[int, str] = {}
    stem = master.stem.lower()
    for suffix in ("strings", "dlstrings", "ilstrings"):
        strings.update(load_strings(strings_dir / f"{stem}_english.{suffix}"))

    result: dict[int, set[str]] = {}
    for record in iter_records(master.read_bytes()):
        if record.signature != "INFO":
            continue
        texts = {
            strings[struct.unpack_from("<I", value, 0)[0]]
            for signature, value in fields(record.body)
            if signature == "NAM1"
            and len(value) == 4
            and struct.unpack_from("<I", value, 0)[0] in strings
        }
        if texts:
            result[record.form_id & 0xFFFFFF] = texts
    return result


def voice_assets(voice_bsa: Path) -> set[tuple[str, str, int]]:
    result: set[tuple[str, str, int]] = set()
    with voice_bsa.open("rb") as handle, mmap.mmap(
        handle.fileno(), 0, access=mmap.ACCESS_READ
    ) as blob:
        if blob[:4] != b"BSA\0":
            raise ValueError(f"Not a BSA archive: {voice_bsa}")
        version, header_offset, _flags, folder_count, file_count = struct.unpack_from(
            "<5I", blob, 4
        )
        if version != 105 or header_offset != 36:
            raise ValueError(f"Unsupported BSA version/header: {version}/{header_offset}")

        cursor = header_offset
        folder_counts: list[int] = []
        for _ in range(folder_count):
            _folder_hash, count, _unknown, _offset = struct.unpack_from("<QIIQ", blob, cursor)
            cursor += 24
            folder_counts.append(count)

        folders: list[str] = []
        for count in folder_counts:
            length = blob[cursor]
            cursor += 1
            folder = blob[cursor : cursor + length].rstrip(b"\0").decode("cp1252")
            cursor += length + count * 16
            folders.extend([folder] * count)

        if len(folders) != file_count:
            raise ValueError(f"Expected {file_count} files, parsed {len(folders)}")

        for folder in folders:
            end = blob.find(b"\0", cursor)
            if end < 0:
                raise ValueError("Unterminated BSA file name")
            filename = blob[cursor:end].decode("cp1252")
            cursor = end + 1
            match = FUZ_RE.search(filename)
            if not match:
                continue
            parts = folder.replace("/", "\\").split("\\")
            if len(parts) < 4 or parts[-4:-2] != ["sound", "voice"]:
                continue
            plugin, voice_type = parts[-2], parts[-1]
            result.add((plugin.casefold(), voice_type.casefold(), int(match.group(1), 16)))
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--masters-dir", type=Path, required=True)
    parser.add_argument("--strings-dir", type=Path, required=True)
    parser.add_argument("--voice-bsa", type=Path, required=True)
    parser.add_argument("--responses", type=Path, default=RESPONSES_PATH)
    args = parser.parse_args()

    with args.responses.open("r", encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.DictReader(handle))

    plugins = sorted({row["ResponsePlugin"] for row in rows})
    subtitles = {
        plugin.casefold(): master_subtitles(args.masters_dir / plugin, args.strings_dir)
        for plugin in plugins
    }
    assets = voice_assets(args.voice_bsa)

    errors: list[str] = []
    for row in rows:
        plugin = row["ResponsePlugin"]
        info_id = int(row["ResponseInfoFormID"], 16)
        text = row["ResponseText"]
        voice_type = row["VoiceType"]
        found_texts = subtitles[plugin.casefold()].get(info_id, set())
        if text not in found_texts:
            errors.append(
                f"{row['Service']} / {row['SelectorName']}: subtitle {text!r} "
                f"not found in {plugin}|{info_id:06X}; found {sorted(found_texts)!r}"
            )
        if (plugin.casefold(), voice_type.casefold(), info_id) not in assets:
            errors.append(
                f"{row['Service']} / {row['SelectorName']}: no FUZ for "
                f"{plugin}|{info_id:06X} / {voice_type}"
            )

    if errors:
        raise SystemExit("\n".join(errors))
    print(f"Validated {len(rows)} dialogue mappings against localized masters and voice BSA")
    print(f"Distinct SharedInfo/VoiceType assets: {len({(r['ResponsePlugin'], r['ResponseInfoFormID'], r['VoiceType']) for r in rows})}")
    print("Every reviewed subtitle has a matching vanilla FUZ asset")


if __name__ == "__main__":
    main()
