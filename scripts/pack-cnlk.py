#!/usr/bin/env python3
"""Pack CNLK (library package): manifest + image."""

from __future__ import annotations

import argparse
import struct


def align8(v: int) -> int:
    return (v + 7) & ~7


def pack(output: str, manifest: bytes, image: bytes) -> None:
    magic = 0x4B4C4E43
    major = 1
    minor = 0
    kind_library = 2
    section_count = 2

    header_size = 40
    entry_size = 24
    meta_end = header_size + section_count * entry_size
    header_bytes = align8(meta_end)

    man_off = header_bytes
    man_sz = len(manifest)
    img_off = align8(man_off + man_sz)
    img_sz = len(image)

    hdr = struct.pack(
        "<IHHIIII16s",
        magic,
        major,
        minor,
        kind_library,
        header_bytes,
        0,
        section_count,
        bytes(16),
    )
    e_manifest = struct.pack("<IIQQ", 1, 0, man_off, man_sz)
    e_image = struct.pack("<IIQQ", 2, 0, img_off, img_sz)
    blob = hdr + e_manifest + e_image
    blob += bytes(header_bytes - len(blob))
    blob += manifest
    blob += bytes(img_off - (header_bytes + man_sz))
    blob += image
    with open(output, "wb") as f:
        f.write(blob)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("output")
    ap.add_argument("image")
    ap.add_argument("--soname", required=True)
    ap.add_argument("--abi-major", type=int, default=1)
    ap.add_argument("--abi-minor", type=int, default=0)
    ap.add_argument("--version", default="1.0.0")
    args = ap.parse_args()
    with open(args.image, "rb") as f:
        image = f.read()
    manifest = (
        "format_version = 1\n"
        f"soname = {args.soname}\n"
        f"version = {args.version}\n"
        f"abi_major = {args.abi_major}\n"
        f"abi_minor = {args.abi_minor}\n"
    ).encode("utf-8")
    pack(args.output, manifest, image)


if __name__ == "__main__":
    main()
