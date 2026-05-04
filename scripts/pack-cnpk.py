#!/usr/bin/env python3
"""Pack CNPK (program package): manifest + image."""

from __future__ import annotations

import argparse
import struct


def align8(v: int) -> int:
    return (v + 7) & ~7


def pack(output: str, manifest: bytes, image: bytes) -> None:
    magic = 0x4B504E43
    major = 1
    minor = 0
    kind_program = 1
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
        kind_program,
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
    ap.add_argument("--bundle-id", default="chaseros.demo")
    ap.add_argument("--name", default="CNPK Demo")
    ap.add_argument("--version", default="1.0.0")
    ap.add_argument("--requires", default="")
    args = ap.parse_args()
    with open(args.image, "rb") as f:
        image = f.read()
    manifest = (
        "format_version = 1\n"
        f"bundle_id = {args.bundle_id}\n"
        f"name = {args.name}\n"
        f"version = {args.version}\n"
        f"requires = {args.requires}\n"
    ).encode("utf-8")
    pack(args.output, manifest, image)


if __name__ == "__main__":
    main()
