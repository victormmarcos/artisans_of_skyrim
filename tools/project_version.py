"""Project version, read from the project() line in CMakeLists.txt (the only place it is set)."""

from __future__ import annotations

import re
from pathlib import Path

_CMAKE = Path(__file__).resolve().parents[1] / "CMakeLists.txt"
_match = re.search(r"^project\(\s*\w+\s+VERSION\s+([0-9.]+)", _CMAKE.read_text(encoding="utf-8"), re.M)
if not _match:
    raise RuntimeError("project(... VERSION x.y[.z]) not found in CMakeLists.txt")

TEXT: str = _match.group(1)
"""Version as written in CMakeLists.txt, e.g. "1.8" or "1.0.0"."""

PARTS: tuple[int, int, int] = tuple((list(map(int, TEXT.split("."))) + [0, 0])[:3])  # type: ignore[assignment]

PACKED: int = (PARTS[0] << 24) | (PARTS[1] << 16) | (PARTS[2] << 4)
"""SKSE/REL::Version packed form (build number 0)."""
