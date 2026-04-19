#!/bin/bash

# ChaserOS 虚拟机运行脚本 (GRUB2 ISO，由 CMake 生成 build/chaseros.iso)

if ! command -v qemu-system-x86_64 &> /dev/null
then
    echo "错误: 未找到 qemu-system-x86_64，请先安装 QEMU。"
    exit 1
fi

ISO="build/chaseros.iso"
if [ ! -f "$ISO" ]; then
    echo "正在编译项目..."
    cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-elf.cmake
    cmake --build build
fi

# 可选：磁盘镜像。默认 8G 稀疏 raw，首次自动创建。
# CHASEROS_QEMU_AHCI=1：AHCI（须内核 -DCHASEROS_WITH_AHCI_RUST=ON）；否则 legacy IDE 主盘（attach ide 0）
# 覆盖路径: DISK_IMG=...；不挂盘: CHASEROS_NO_DISK=1 ./run.sh
DISK_IMG="${DISK_IMG:-build/chaseros-disk8g.img}"
QEMU_EXTRA=()
if [ -z "${CHASEROS_NO_DISK:-}" ] && [ -n "$DISK_IMG" ]; then
    if [ ! -f "$DISK_IMG" ]; then
        echo "正在创建 8G 磁盘镜像: $DISK_IMG"
        qemu-img create -f raw "$DISK_IMG" 8G
    fi
    if [ -n "${CHASEROS_QEMU_AHCI:-}" ]; then
        QEMU_EXTRA+=(-device ahci,id=ahci)
        QEMU_EXTRA+=(-drive "file=$DISK_IMG,if=none,id=chaseros_hd0,format=raw")
        QEMU_EXTRA+=(-device "ide-hd,drive=chaseros_hd0,bus=ahci.0")
    else
        QEMU_EXTRA+=(-drive "file=$DISK_IMG,format=raw,if=ide,index=0,media=disk")
    fi
fi

echo "正在启动 ChaserOS 64 位混合内核 (GRUB2)..."

qemu-system-x86_64 \
    "${QEMU_EXTRA[@]}" \
    -cdrom "$ISO" \
    -m 128M \
    -no-reboot \
    -no-shutdown \
    -serial stdio
