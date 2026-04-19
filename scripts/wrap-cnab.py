#!/usr/bin/env python3
"""将平铺机器码 raw.bin 封装为 CNAB（见 kernel/fs/cnaf/cnab.h），供 pack-cnaf.py 作为 IMAGE 节写入 CNAF。"""

from __future__ import annotations

import argparse
import struct
import sys

CNAB_MAGIC = 0x42414E43
CNAB_FMT_MAJOR = 0
CNAB_FMT_MINOR = 1
CNAB_HEADER_SIZE = 64


def wrap_cnab(raw: bytes, load_base: int, entry_rva: int) -> bytes:
    payload_size = len(raw)
    hdr = struct.pack(
        "<IHHQQQII",
        CNAB_MAGIC,
        CNAB_FMT_MAJOR,
        CNAB_FMT_MINOR,
        load_base,
        entry_rva,
        payload_size,
        CNAB_HEADER_SIZE,
        0,
    )
    hdr += bytes(24)  # reserved（与 cnab.h 一致，共 64 字节）
    assert len(hdr) == CNAB_HEADER_SIZE
    return hdr + raw


def main() -> None:
    ap = argparse.ArgumentParser(description="raw 机器码 → CNAB（非 ELF）")
    ap.add_argument("input_raw", help="平铺二进制（如 objcopy -O binary 或 nasm -fbin）")
    ap.add_argument("output_cnab", help="输出 .cnab 映像文件")
    ap.add_argument("--load-base", default="0x400000", help="映射基址（默认 0x400000）")
    ap.add_argument("--entry-rva", required=True, help="入口相对 load_base 的偏移（如 0xe0 对应 _start@0x4000e0）")
    args = ap.parse_args()
    load_base = int(args.load_base, 0)
    entry_rva = int(args.entry_rva, 0)
    with open(args.input_raw, "rb") as f:
        raw = f.read()
    if not raw:
        print("wrap-cnab: empty raw input", file=sys.stderr)
        sys.exit(1)
    blob = wrap_cnab(raw, load_base, entry_rva)
    with open(args.output_cnab, "wb") as f:
        f.write(blob)
    print("OK:", args.output_cnab, "payload", len(raw), "bytes")


if __name__ == "__main__":
    main()
