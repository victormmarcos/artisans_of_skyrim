#!/usr/bin/env python3
"""Compile the standalone AS2 menu; no game installation is needed."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mtasc", type=Path, default=os.environ.get("MTASC_PATH"))
    args = parser.parse_args()
    if not args.mtasc or not args.mtasc.is_file():
        parser.error("Pass --mtasc pointing to the MTASC 1.15 executable, or set MTASC_PATH")
    output = ROOT / "dist/Data/Interface/ArtisansOfSkyrim/ServiceMenu.swf"
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(args.mtasc.resolve()), "-cp", str(args.mtasc.resolve().parent / "std"),
        "-cp", str(ROOT / "interface"), "-version", "8", "-header", "1280:720:60",
        "-main", "-swf", str(output), str(ROOT / "interface/ServiceMovie.as")], check=True)
    files = [ROOT / "interface/ArtisansMenu.as", ROOT / "interface/ArtisansDraw.as", ROOT / "interface/ArtisansControls.as", ROOT / "interface/ServiceMovie.as", output]
    manifest = {str(f.relative_to(ROOT)).replace("\\", "/"): hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
    (ROOT / "interface/build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("Compiled ServiceMenu.swf and recorded source/output hashes")

if __name__ == "__main__":
    main()
