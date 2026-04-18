#!/bin/bash

# CNOS 虚拟机运行脚本 (GRUB2 ISO，由 CMake 生成 build/cnos.iso)

if ! command -v qemu-system-x86_64 &> /dev/null
then
    echo "错误: 未找到 qemu-system-x86_64，请先安装 QEMU。"
    exit 1
fi

ISO="build/cnos.iso"
if [ ! -f "$ISO" ]; then
    echo "正在编译项目..."
    cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-elf.cmake
    cmake --build build
fi

echo "正在启动 CNOS 64位微内核 (GRUB2)..."

qemu-system-x86_64 \
    -cdrom "$ISO" \
    -m 128M \
    -no-reboot \
    -no-shutdown \
    -serial stdio
