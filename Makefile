# 薄封装：实际构建由 CMake 完成。
#
# 默认流程会：编译 user/hello.elf → 嵌入为 build/hello_elf.o → 链接 kernel.elf →
#（若存在 grub-mkrescue）生成 build/chaseros.iso。
#
# 变量：
#   BUILD_DIR  构建目录，默认 build
#   TOOLCHAIN  首次 cmake 时传入的 CMAKE_TOOLCHAIN_FILE
#   QEMU       QEMU 可执行文件，默认 qemu-system-x86_64
#   QEMU_OPTS  附加 QEMU 参数
#   NATIVE_GCC_PREFIX ChaserOS 原生 GCC 安装前缀（默认 $(HOME)/opt/chaseros-native-gcc）
#   NATIVE_USE_LOCAL_ELF 1/true 时优先复用本地 x86_64-elf-*
#   LOCAL_ELF_PREFIX 本地 x86_64-elf 工具链前缀（可选）

BUILD_DIR ?= build
TOOLCHAIN ?= $(CURDIR)/cmake/x86_64-elf.cmake
# CNAF 用户 ELF 独立构建目录（ANAF + cnaf-anaf.cmake）
CNAF_USER_BUILD_DIR ?= build-cnaf-user
CNAF_USER_TOOLCHAIN ?= $(CURDIR)/cmake/cnaf-anaf.cmake
QEMU      ?= qemu-system-x86_64
QEMU_OPTS ?=
NATIVE_GCC_PREFIX ?= $(HOME)/opt/chaseros-native-gcc
NATIVE_USE_LOCAL_ELF ?= auto
LOCAL_ELF_PREFIX ?=

.PHONY: all clean configure kernel iso run run-cmake cnaf-user native-gcc native-gcc-test native-gcc-print-env proc-api-test qemu-proc-smoke help

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
	cmake --build $(BUILD_DIR) --target chaseros_iso

clean:
	@cmake --build $(BUILD_DIR) --target clean 2>/dev/null || true
	rm -rf $(BUILD_DIR)

# 仅构建 CNAF 用户示例 hello.elf（需 PATH 中有 ANAF / x86_64-elf-gcc）
cnaf-user:
	cmake -S user -B $(CNAF_USER_BUILD_DIR) -DCMAKE_TOOLCHAIN_FILE=$(CNAF_USER_TOOLCHAIN)
	cmake --build $(CNAF_USER_BUILD_DIR)

# 构建并 QEMU 从 ISO 启动（与 README 一致）
run: all
	$(QEMU) -cdrom $(BUILD_DIR)/chaseros.iso -m 128M -serial stdio $(QEMU_OPTS)

# 使用 CMake 定义的 run 目标（依赖 chaseros_iso，等价于上方 QEMU 命令）
run-cmake: all
	cmake --build $(BUILD_DIR) --target run

# 构建 ChaserOS 原生 GCC 引导链（x86_64-chaseros）
native-gcc:
	CHASEROS_NATIVE_USE_LOCAL_ELF=$(NATIVE_USE_LOCAL_ELF) \
	CHASEROS_LOCAL_ELF_PREFIX="$(LOCAL_ELF_PREFIX)" \
	bash scripts/build-chaseros-native-gcc.sh "$(NATIVE_GCC_PREFIX)"

# 运行最小用户态 ELF 链接自检
native-gcc-test: native-gcc
	bash scripts/test-chaseros-native-link.sh "$(NATIVE_GCC_PREFIX)"

# 打印可 source 的环境脚本路径
native-gcc-print-env:
	@echo "source $(NATIVE_GCC_PREFIX)/bin/chaseros-native-gcc-env.sh"

proc-api-test:
	bash scripts/test-proc-api-static.sh

qemu-proc-smoke:
	bash scripts/test-qemu-proc-smoke.sh

help:
	@echo "ChaserOS Makefile（封装 CMake）"
	@echo "  make / make all   - 配置（若需）并完整构建"
	@echo "  make kernel       - 仅构建 $(BUILD_DIR)/kernel.elf"
	@echo "  make iso          - 生成 $(BUILD_DIR)/chaseros.iso（需 grub-mkrescue）"
	@echo "  make run          - 构建后用 QEMU 运行 ISO"
	@echo "  make run-cmake    - 同上，通过 cmake --target run"
	@echo "  make clean        - 清理构建目录"
	@echo "  make configure    - 仅运行 cmake 配置"
	@echo "  make cnaf-user    - 用 cnaf-anaf.cmake 构建 $(CNAF_USER_BUILD_DIR)/hello.elf"
	@echo "  make native-gcc   - 构建 x86_64-chaseros 原生 GCC 引导链"
	@echo "  make native-gcc-test - 构建并验证最小用户态 ELF 链接"
	@echo "  make native-gcc-print-env - 输出 source 环境脚本命令"
	@echo "  make proc-api-test  - 进程/exec 接口静态检查"
	@echo "  make qemu-proc-smoke - QEMU 启动 smoke（进程体系回归）"
	@echo "示例: make run QEMU_OPTS=\"-display none\""
	@echo "示例: make native-gcc NATIVE_USE_LOCAL_ELF=1 LOCAL_ELF_PREFIX=\"$$HOME/opt/cross\""
