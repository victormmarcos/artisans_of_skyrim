#!/usr/bin/env python3
"""Check that a release binary contains no path from the user profile (read-only)."""
import argparse
import re
from pathlib import Path

def validate(path: Path) -> None:
    data = path.read_bytes()
    profile = re.compile(rb"[A-Za-z]:[\\/]Users[\\/][^\x00]+", re.I)
    if profile.search(data.replace(b"\x00", b"")):
        raise ValueError("Embedded user-profile path: rebuild the dependency with compiler path mapping")
    print("Release binary privacy audit: no embedded profile roots; binary unchanged")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    validate(parser.parse_args().binary)
