#!/usr/bin/env python3
"""将 MANIFEST（UTF-8）与 ELF 二进制打成 CNAF v0.2（cnaf_spec.h）。"""

from __future__ import annotations

import argparse
import struct
import sys


def align8(n: int) -> int:
    return (n + 7) & ~7


def write_cnaf_file(
    path: str,
    manifest_utf8: bytes,
    elf_blob: bytes,
) -> None:
    CNAF_MAGIC = 0x46414E43
    CNAF_FMT_MAJOR = 0
    CNAF_FMT_MINOR = 2
    CNAF_PAYLOAD_APP = 1
    CNAF_FLAG_SECTION_TABLE = 1
    CNAF_SECTION_MANIFEST = 1
    CNAF_SECTION_IMAGE = 3

    count = 2
    raw_meta_end = 36 + 8 + count * 24
    header_bytes = align8(raw_meta_end)

    manifest_off = header_bytes
    manifest_sz = len(manifest_utf8)
    image_off = align8(manifest_off + manifest_sz)
    image_sz = len(elf_blob)

    id_zero = bytes(16)
    hdr = (
        struct.pack(
            "<IHHIII",
            CNAF_MAGIC,
            CNAF_FMT_MAJOR,
            CNAF_FMT_MINOR,
            CNAF_PAYLOAD_APP,
            header_bytes,
            CNAF_FLAG_SECTION_TABLE,
        )
        + id_zero
    )
    assert len(hdr) == 36

    tbl = struct.pack("<II", count, 0)
    e1 = struct.pack("<IIQQ", CNAF_SECTION_MANIFEST, 0, manifest_off, manifest_sz)
    e2 = struct.pack("<IIQQ", CNAF_SECTION_IMAGE, 0, image_off, image_sz)
    meta = hdr + tbl + e1 + e2
    assert len(meta) == raw_meta_end

    pad_meta = bytes(header_bytes - len(meta))
    gap = bytes(image_off - manifest_off - manifest_sz)
    blob = meta + pad_meta + manifest_utf8 + gap + elf_blob

    if len(blob) != image_off + image_sz:
        raise RuntimeError(
            f"CNAF size mismatch: got {len(blob)}, expect {image_off + image_sz}"
        )

    with open(path, "wb") as f:
        f.write(blob)


def main() -> None:
    ap = argparse.ArgumentParser(description="打包 CNAF v0.2（MANIFEST + ELF IMAGE）")
    ap.add_argument("output", help="输出 .cnaf")
    ap.add_argument("elf", help="输入 ELF64 文件路径")
    ap.add_argument("--bundle-id", default="cnos.generated.app")
    ap.add_argument("--name", default="CNOS App")
    ap.add_argument("--version", default="0.1.0")
    args = ap.parse_args()

    with open(args.elf, "rb") as f:
        elf = f.read()

    manifest = (
        "format_version = 1\n"
        f"bundle_id = {args.bundle_id}\n"
        f"name = {args.name}\n"
        f"version = {args.version}\n"
        "entry_symbol = _start\n"
    ).encode("utf-8")

    write_cnaf_file(args.output, manifest, elf)


if __name__ == "__main__":
    main()
