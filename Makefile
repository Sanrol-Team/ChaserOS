# 薄封装：实际构建由 CMake 完成。
#
# 默认流程会：编译 user/hello.elf → 嵌入为 build/hello_elf.o → 链接 kernel.elf →
#（若存在 grub-mkrescue）生成 build/cnos.iso。
#
# 变量：
#   BUILD_DIR  构建目录，默认 build
#   TOOLCHAIN  首次 cmake 时传入的 CMAKE_TOOLCHAIN_FILE
#   QEMU       QEMU 可执行文件，默认 qemu-system-x86_64
#   QEMU_OPTS  附加 QEMU 参数

BUILD_DIR ?= build
TOOLCHAIN ?= $(CURDIR)/cmake/x86_64-elf.cmake
# CNAF 用户 ELF 独立构建目录（ANAF + cnaf-anaf.cmake）
CNAF_USER_BUILD_DIR ?= build-cnaf-user
CNAF_USER_TOOLCHAIN ?= $(CURDIR)/cmake/cnaf-anaf.cmake
QEMU      ?= qemu-system-x86_64
QEMU_OPTS ?=

.PHONY: all clean configure kernel iso run run-cmake cnaf-user help

all: configure
	cmake --build $(BUILD_DIR)

# 已有 build/CMakeCache 时不再传工具链，避免 “CMAKE_TOOLCHAIN_FILE was not used” 警告
configure:
	@test -f $(BUILD_DIR)/CMakeCache.txt && cmake -B $(BUILD_DIR) || \
		cmake -B $(BUILD_DIR) -DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN)

# 仅构建内核 ELF（含嵌入的 user 程序二进制）
kernel: configure
	cmake --build $(BUILD_DIR) --target kernel.elf

# 在已有 kernel.elf 基础上生成 ISO（需 grub-mkrescue）
iso: configure
	cmake --build $(BUILD_DIR) --target cnos_iso

clean:
	@cmake --build $(BUILD_DIR) --target clean 2>/dev/null || true
	rm -rf $(BUILD_DIR)

# 仅构建 CNAF 用户示例 hello.elf（需 PATH 中有 ANAF / x86_64-elf-gcc）
cnaf-user:
	cmake -S user -B $(CNAF_USER_BUILD_DIR) -DCMAKE_TOOLCHAIN_FILE=$(CNAF_USER_TOOLCHAIN)
	cmake --build $(CNAF_USER_BUILD_DIR)

# 构建并 QEMU 从 ISO 启动（与 README 一致）
run: all
	$(QEMU) -cdrom $(BUILD_DIR)/cnos.iso -m 128M -serial stdio $(QEMU_OPTS)

# 使用 CMake 定义的 run 目标（依赖 cnos_iso，等价于上方 QEMU 命令）
run-cmake: all
	cmake --build $(BUILD_DIR) --target run

help:
	@echo "CNOS Makefile（封装 CMake）"
	@echo "  make / make all   - 配置（若需）并完整构建"
	@echo "  make kernel       - 仅构建 $(BUILD_DIR)/kernel.elf"
	@echo "  make iso          - 生成 $(BUILD_DIR)/cnos.iso（需 grub-mkrescue）"
	@echo "  make run          - 构建后用 QEMU 运行 ISO"
	@echo "  make run-cmake    - 同上，通过 cmake --target run"
	@echo "  make clean        - 清理构建目录"
	@echo "  make configure    - 仅运行 cmake 配置"
	@echo "  make cnaf-user    - 用 cnaf-anaf.cmake 构建 $(CNAF_USER_BUILD_DIR)/hello.elf"
	@echo "示例: make run QEMU_OPTS=\"-display none\""
