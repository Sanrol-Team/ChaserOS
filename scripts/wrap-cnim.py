#!/usr/bin/env python3
"""Wrap flat raw payload into CNIM image."""

from __future__ import annotations

import argparse
import struct
import sys

CNIM_MAGIC = 0x4D494E43
CNIM_FMT_MAJOR = 1
CNIM_FMT_MINOR = 0
CNIM_HEADER_SIZE = 64


def wrap_cnim(raw: bytes, load_base: int, entry_rva: int) -> bytes:
    seg_off = CNIM_HEADER_SIZE
    seg_tbl_off = CNIM_HEADER_SIZE
    seg_tbl_size = 40
    payload_off = seg_off + seg_tbl_size
    header = struct.pack(
        "<IHHIIQQLLQQQ",
        CNIM_MAGIC,
        CNIM_FMT_MAJOR,
        CNIM_FMT_MINOR,
        CNIM_HEADER_SIZE,
        0,
        entry_rva,
        load_base,
        1,
        0,
        seg_tbl_off,
        0,
        0,
    )
    assert len(header) == CNIM_HEADER_SIZE
    seg = struct.pack(
        "<QQQQLL",
        payload_off,
        load_base,
        len(raw),
        len(raw),
        0x5,
        16,
    )
    return header + seg + raw


def main() -> None:
    ap = argparse.ArgumentParser(description="raw machine code -> CNIM")
    ap.add_argument("input_raw")
    ap.add_argument("output_cnim")
    ap.add_argument("--load-base", default="0x400000")
    ap.add_argument("--entry-rva", required=True)
    args = ap.parse_args()

    load_base = int(args.load_base, 0)
    entry_rva = int(args.entry_rva, 0)
    with open(args.input_raw, "rb") as f:
        raw = f.read()
    if not raw:
        print("wrap-cnim: empty input", file=sys.stderr)
        sys.exit(1)
    out = wrap_cnim(raw, load_base, entry_rva)
    with open(args.output_cnim, "wb") as f:
        f.write(out)
    print("OK:", args.output_cnim, "payload", len(raw), "bytes")


if __name__ == "__main__":
    main()
