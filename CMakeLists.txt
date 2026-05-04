cmake_minimum_required(VERSION 3.16)

# 首次配置须指定工具链，例如：
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-elf.cmake
# 或使用：cmake --preset default（见 CMakePresets.json）
# 已有 build/ 后只需：cmake -B build 或 cmake --build build，勿重复传工具链，否则 CMake 会警告变量未使用。
if(CMAKE_TOOLCHAIN_FILE)
    message(STATUS "CHASEROS: CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
endif()

project(ChaserOS LANGUAGES C ASM_NASM)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

set(KERNEL_INCLUDE ${CMAKE_SOURCE_DIR}/kernel)
set(KERNEL_FS_CHASEROS ${CMAKE_SOURCE_DIR}/kernel/fs/chaseros)
set(KERNEL_FS_INCLUDE ${CMAKE_SOURCE_DIR}/kernel/fs/include)
set(KERNEL_FS_LIB ${CMAKE_SOURCE_DIR}/kernel/fs/lib)
set(KERNEL_FS_EXT2FS ${CMAKE_SOURCE_DIR}/kernel/fs/lib/ext2fs)
set(KERNEL_FS_CNAF ${CMAKE_SOURCE_DIR}/kernel/fs/cnaf)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

get_filename_component(_CHASEROS_GCC_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
find_program(CHASEROS_OBJCOPY NAMES x86_64-elf-objcopy HINTS "${_CHASEROS_GCC_DIR}" REQUIRED)

# e2fsprogs 整树在 kernel/fs/lib 等目录；ChaserOS 只链接下列 .c，按需再追加 lib/ext2fs/*.c（见 kernel/fs/README）

set(KERNEL_ASM
    kernel/entry.asm
    kernel/interrupts.asm
    kernel/gdt_load.asm
)

# 可选：Rust staticlib AHCI（须: rustup target add x86_64-unknown-none）
option(CHASEROS_WITH_AHCI_RUST "链入 kernel/ahci_rust（cargo → libchaseros_ahci_rust.a）" OFF)
set(CHASEROS_AHCI_RUST_LIB "${CMAKE_BINARY_DIR}/ahci_rust/x86_64-unknown-none/release/libchaseros_ahci_rust.a")
# Slime 内建 slimec 与 AHCI 共用同一 cargo；可 cmake -DCHASEROS_CARGO=/path/to/cargo
set(CHASEROS_CARGO "" CACHE FILEPATH "cargo 可执行文件（留空则从 PATH 查找）")

set(KERNEL_C
    kernel/console.c
    kernel/sysinfo.c
    kernel/main.c
    kernel/multiboot.c
    kernel/pmm.c
    kernel/vmm.c
    kernel/vmm_cow.c
    kernel/gdt.c
    kernel/idt.c
    kernel/isr.c
    kernel/power.c
    kernel/drivers/ttfont.c
    kernel/shell.c
    kernel/ipc.c
    kernel/hybrid_ipc.c
    kernel/fs/vfs.c
    kernel/fs/vfs_devfs.c
    kernel/fs/chaseros/chaseros_alloc.c
    kernel/fs/chaseros/chaseros_ext2.c
    kernel/fs/cnaf/cnaf_validate.c
    kernel/fs/cnaf/cnaf_image.c
    kernel/drivers/vga.c
    kernel/drivers/font.c
    kernel/drivers/graphics.c
    kernel/drivers/serial.c
    kernel/drivers/pci.c
    kernel/drivers/ide.c
    kernel/fs/chaseros/chaseros_ext2_vol.c
    kernel/process.c
    kernel/sched.c
    kernel/elf_load.c
    kernel/fs/cnaf/cnab_load.c
    kernel/loader/cndyn_loader.c
    kernel/loader/user_exec.c
    kernel/user.c
    kernel/user_fd.c
)

if(CHASEROS_WITH_AHCI_RUST)
    list(APPEND KERNEL_C kernel/drivers/ahci_pci.c)
endif()

set(USER_HELLO_ELF ${CMAKE_BINARY_DIR}/user/hello.elf)
set(USER_HELLO_OBJ ${CMAKE_BINARY_DIR}/hello_elf.o)
set(DEMO_CNPK_BIN ${CMAKE_BINARY_DIR}/demo_cnpk/demo.cnpk)
set(DEMO_CNPK_OBJ ${CMAKE_BINARY_DIR}/demo_cnpk_obj.o)
set(USER_HELLO_RAW ${CMAKE_BINARY_DIR}/user/hello_raw.bin)
set(USER_HELLO_CNIM ${CMAKE_BINARY_DIR}/user/hello.cnim)
# 内嵌 demo_cnpk.bin 的 IMAGE：CNIM（非 ELF）；默认由 C hello 经 objcopy+wrap-cnim 生成
set(CHASEROS_DEMO_CNPK_PAYLOAD ${USER_HELLO_CNIM})

# 可选：嵌入 Slime（slimec --target cnos）编译的 hello_sm.elf；默认在仓库内 cargo 编译 slimec（见 CHASEROS_BUILD_SLIMEC）
option(CHASEROS_WITH_SLIME_USER "嵌入 Slime 用户态 hello_sm.elf（需 nasm；slimec 见 CHASEROS_BUILD_SLIMEC / CHASEROS_SLIMEC）；并以内嵌 CNAFLOADER（cnafloader.sm）替代 C hello 作为无参 cnrun 的 demo 包 IMAGE" OFF)
set(CHASEROS_SLIMEC "" CACHE FILEPATH "仅在 CHASEROS_BUILD_SLIMEC=OFF 时使用：宿主 slimec 路径；留空则在 PATH 中查找")
set(CHASEROS_SLIME_SRC "${CMAKE_SOURCE_DIR}/integrations/slime-for-chaseros/upstream/slime" CACHE PATH "Slime 源码根目录（须含 Cargo.toml；slimec 须支持 --target cnos）")
option(CHASEROS_BUILD_SLIMEC "在 CMake 构建内执行 cargo build 生成 slimec（写入 build/slime-tools），不依赖 PATH" ON)
set(KERNEL_EXTRA_OBJ "")

# CACHE 中 CHASEROS_CARGO 可为空；勿用 if(CHASEROS_CARGO) 判断（空 FILEPATH 仍可能为真）
set(CHASEROS_CARGO_FOR_BUILD "")
if("${CHASEROS_CARGO}" STREQUAL "")
    if(CHASEROS_WITH_AHCI_RUST OR (CHASEROS_WITH_SLIME_USER AND CHASEROS_BUILD_SLIMEC))
        find_program(CHASEROS_CARGO_EXE NAMES cargo REQUIRED PATHS /usr/bin /usr/local/bin "$ENV{HOME}/.cargo/bin")
        set(CHASEROS_CARGO_FOR_BUILD "${CHASEROS_CARGO_EXE}")
    endif()
else()
    set(CHASEROS_CARGO_FOR_BUILD "${CHASEROS_CARGO}")
endif()

add_custom_command(
    OUTPUT ${USER_HELLO_ELF}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/user
    COMMAND ${CMAKE_C_COMPILER} -ffreestanding -O2 -Wall -Wextra -nostdlib -fno-builtin -fno-stack-protector -m64 -I${CMAKE_SOURCE_DIR}/user/include -c -o ${CMAKE_BINARY_DIR}/user/hello.o ${CMAKE_SOURCE_DIR}/user/hello.c
    COMMAND ${CMAKE_LINKER} -nostdlib -T ${CMAKE_SOURCE_DIR}/user/user.ld -o ${USER_HELLO_ELF} ${CMAKE_BINARY_DIR}/user/hello.o
    DEPENDS ${CMAKE_SOURCE_DIR}/user/hello.c ${CMAKE_SOURCE_DIR}/user/user.ld ${CMAKE_SOURCE_DIR}/user/include/chaseros_user.h
    COMMENT "构建 user/hello.elf"
    VERBATIM
)

add_custom_command(
    OUTPUT ${USER_HELLO_OBJ}
    COMMAND ${CMAKE_COMMAND} -E copy ${USER_HELLO_ELF} ${CMAKE_BINARY_DIR}/hello.bin
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR} ${CMAKE_LINKER} -r -b binary -o hello_elf.o hello.bin
    DEPENDS ${USER_HELLO_ELF}
    COMMENT "嵌入 hello.bin"
    VERBATIM
)

add_custom_command(
    OUTPUT ${USER_HELLO_CNIM}
    COMMAND ${CHASEROS_OBJCOPY} -O binary ${USER_HELLO_ELF} ${USER_HELLO_RAW}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/wrap-cnim.py --load-base 0x400000 --entry-rva 0xe0
            ${USER_HELLO_RAW} ${USER_HELLO_CNIM}
    DEPENDS ${USER_HELLO_ELF} ${CMAKE_SOURCE_DIR}/scripts/wrap-cnim.py
    COMMENT "hello.elf → raw → CNIM（CNPK IMAGE 载荷）"
    VERBATIM
)

if(CHASEROS_WITH_SLIME_USER)
    find_program(CHASEROS_NASM_FOR_SLIME NAMES nasm REQUIRED)
    set(USER_SLIME_HELLO_ELF ${CMAKE_BINARY_DIR}/user/hello_sm.elf)
    set(USER_SLIME_HELLO_OBJ ${CMAKE_BINARY_DIR}/hello_sm_elf.o)
    set(USER_SLIME_ASM ${CMAKE_BINARY_DIR}/user/hello_sm.asm)
    set(USER_SLIME_O ${CMAKE_BINARY_DIR}/user/hello_sm.o)
    set(USER_SLIME_CNAFLOADER_ELF ${CMAKE_BINARY_DIR}/user/cnafloader.elf)
    set(USER_SLIME_CNAFLOADER_CNIM ${CMAKE_BINARY_DIR}/user/cnafloader.cnim)
    set(USER_SLIME_CNAFLOADER_ASM ${CMAKE_BINARY_DIR}/user/cnafloader.asm)
    set(USER_SLIME_CNAFLOADER_O ${CMAKE_BINARY_DIR}/user/cnafloader.o)
    set(CHASEROS_SLIMEC_EXE "${CMAKE_BINARY_DIR}/slime-tools/slimec")

    set(CHASEROS_SLIMEC_DEP "")
    if(CHASEROS_BUILD_SLIMEC)
        if(NOT EXISTS "${CHASEROS_SLIME_SRC}/Cargo.toml")
            message(FATAL_ERROR "未找到 Slime 源码（${CHASEROS_SLIME_SRC}/Cargo.toml）。请先: bash scripts/fetch-slime-upstream.sh\n或设置 -DCHASEROS_SLIME_SRC=含 cnos 目标的 Slime 源码路径。")
        endif()
        file(GLOB_RECURSE CHASEROS_SLIME_RUST_SOURCES CONFIGURE_DEPENDS "${CHASEROS_SLIME_SRC}/src/*.rs")
        add_custom_command(
            OUTPUT ${CHASEROS_SLIMEC_EXE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/slime-tools"
            COMMAND ${CHASEROS_CARGO_FOR_BUILD} build --manifest-path "${CHASEROS_SLIME_SRC}/Cargo.toml" --release
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CHASEROS_SLIME_SRC}/target/release/slimec" "${CHASEROS_SLIMEC_EXE}"
            DEPENDS "${CHASEROS_SLIME_SRC}/Cargo.toml" ${CHASEROS_SLIME_RUST_SOURCES}
            COMMENT "cargo build slimec（ChaserOS 内建，源码: ${CHASEROS_SLIME_SRC}）"
            VERBATIM
        )
        set(CHASEROS_SLIMEC_FOR_KERNEL "${CHASEROS_SLIMEC_EXE}")
        set(CHASEROS_SLIMEC_DEP "${CHASEROS_SLIMEC_EXE}")
        message(STATUS "CHASEROS: Slime 用户态 — 将 cargo 编译 slimec 至 ${CHASEROS_SLIMEC_EXE}")
    else()
        if(NOT CHASEROS_SLIMEC)
            find_program(CHASEROS_SLIMEC NAMES slimec REQUIRED)
        endif()
        set(CHASEROS_SLIMEC_FOR_KERNEL "${CHASEROS_SLIMEC}")
        message(STATUS "CHASEROS: Slime 用户态 — 使用宿主 slimec: ${CHASEROS_SLIMEC_FOR_KERNEL}")
    endif()

    add_custom_command(
        OUTPUT ${USER_SLIME_HELLO_ELF}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/user"
        COMMAND "${CHASEROS_SLIMEC_FOR_KERNEL}" --target cnos "${CMAKE_SOURCE_DIR}/user/hello.sm" -o "${USER_SLIME_ASM}"
        COMMAND ${CHASEROS_NASM_FOR_SLIME} -felf64 "${USER_SLIME_ASM}" -o "${USER_SLIME_O}"
        COMMAND ${CMAKE_LINKER} -nostdlib -T "${CMAKE_SOURCE_DIR}/user/user.ld" -o "${USER_SLIME_HELLO_ELF}" "${USER_SLIME_O}"
        DEPENDS "${CMAKE_SOURCE_DIR}/user/hello.sm" "${CMAKE_SOURCE_DIR}/user/user.ld" ${CHASEROS_SLIMEC_DEP}
        COMMENT "构建 user/hello_sm.elf（Slime → cnos NASM）"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${USER_SLIME_HELLO_OBJ}
        COMMAND ${CMAKE_COMMAND} -E copy "${USER_SLIME_HELLO_ELF}" "${CMAKE_BINARY_DIR}/hello_sm.bin"
        COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_BINARY_DIR}" ${CMAKE_LINKER} -r -b binary -o hello_sm_elf.o hello_sm.bin
        DEPENDS ${USER_SLIME_HELLO_ELF}
        COMMENT "嵌入 hello_sm.bin（Slime 用户映像）"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${USER_SLIME_CNAFLOADER_ELF}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/user"
        COMMAND "${CHASEROS_SLIMEC_FOR_KERNEL}" --target cnos "${CMAKE_SOURCE_DIR}/user/cnafloader.sm" -o "${USER_SLIME_CNAFLOADER_ASM}"
        COMMAND ${CHASEROS_NASM_FOR_SLIME} -felf64 "${USER_SLIME_CNAFLOADER_ASM}" -o "${USER_SLIME_CNAFLOADER_O}"
        COMMAND ${CMAKE_LINKER} -nostdlib -T "${CMAKE_SOURCE_DIR}/user/user.ld" -o "${USER_SLIME_CNAFLOADER_ELF}" "${USER_SLIME_CNAFLOADER_O}"
        DEPENDS "${CMAKE_SOURCE_DIR}/user/cnafloader.sm" "${CMAKE_SOURCE_DIR}/user/user.ld" ${CHASEROS_SLIMEC_DEP}
        COMMENT "构建 user/cnafloader.elf（Slime CNAFLOADER，供内嵌 demo_cnaf）"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${USER_SLIME_CNAFLOADER_CNIM}
        COMMAND ${CHASEROS_OBJCOPY} -O binary ${USER_SLIME_CNAFLOADER_ELF}
                ${CMAKE_BINARY_DIR}/user/cnafloader_raw.bin
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/wrap-cnim.py --load-base 0x400000 --entry-rva 0xe0
                ${CMAKE_BINARY_DIR}/user/cnafloader_raw.bin ${USER_SLIME_CNAFLOADER_CNIM}
        DEPENDS ${USER_SLIME_CNAFLOADER_ELF} ${CMAKE_SOURCE_DIR}/scripts/wrap-cnim.py
        COMMENT "cnafloader.elf → CNIM（Slime 内嵌 demo IMAGE）"
        VERBATIM
    )

    set(CHASEROS_DEMO_CNPK_PAYLOAD ${USER_SLIME_CNAFLOADER_CNIM})
    set(KERNEL_EXTRA_OBJ ${USER_SLIME_HELLO_OBJ})
endif()

# 首个 CNPK 演示包：MANIFEST + IMAGE（CNIM 载荷），供 cnrun（无参数）解析并运行
add_custom_command(
    OUTPUT ${DEMO_CNPK_OBJ}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/demo_cnpk
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/pack-cnpk.py ${DEMO_CNPK_BIN} ${CHASEROS_DEMO_CNPK_PAYLOAD}
            --bundle-id chaseros.demo.first_cnpk --name CNPKLOADER --version 1.0.0
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DEMO_CNPK_BIN} ${CMAKE_BINARY_DIR}/demo_cnaf.bin
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}
            ${CMAKE_LINKER} -r -b binary -o demo_cnpk_obj.o demo_cnaf.bin
    DEPENDS ${CHASEROS_DEMO_CNPK_PAYLOAD} ${CMAKE_SOURCE_DIR}/scripts/pack-cnpk.py
    COMMENT "打包并嵌入首个 CNPK（demo_cnaf.bin）"
    VERBATIM
)

if(CHASEROS_WITH_AHCI_RUST)
    if("${CHASEROS_CARGO_FOR_BUILD}" STREQUAL "")
        message(FATAL_ERROR "CHASEROS_WITH_AHCI_RUST 需要 cargo（安装 Rust 或 cmake -DCHASEROS_CARGO=/path/to/cargo）")
    endif()
    add_custom_command(
        OUTPUT ${CHASEROS_AHCI_RUST_LIB}
        COMMAND ${CMAKE_COMMAND} -E env "CARGO_TARGET_DIR=${CMAKE_BINARY_DIR}/ahci_rust" ${CHASEROS_CARGO_FOR_BUILD} build --release
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/kernel/ahci_rust
        DEPENDS
            ${CMAKE_SOURCE_DIR}/kernel/ahci_rust/Cargo.toml
            ${CMAKE_SOURCE_DIR}/kernel/ahci_rust/src/lib.rs
            ${CMAKE_SOURCE_DIR}/kernel/ahci_rust/.cargo/config.toml
        COMMENT "cargo: libchaseros_ahci_rust.a（见 kernel/ahci_rust/.cargo/config.toml）"
        VERBATIM
    )
    add_custom_target(chaseros_ahci_rust_a DEPENDS ${CHASEROS_AHCI_RUST_LIB})
    message(STATUS "CHASEROS: AHCI Rust — rustup target add x86_64-unknown-none；cargo=${CHASEROS_CARGO_FOR_BUILD}")
endif()

add_executable(kernel.elf ${KERNEL_ASM} ${KERNEL_C} ${USER_HELLO_OBJ} ${DEMO_CNPK_OBJ} ${KERNEL_EXTRA_OBJ})

target_include_directories(kernel.elf PRIVATE
    ${KERNEL_INCLUDE}
    ${KERNEL_FS_CHASEROS}
    ${KERNEL_FS_CNAF}
    ${KERNEL_FS_INCLUDE}
    ${KERNEL_FS_LIB}
    ${KERNEL_FS_EXT2FS}
)

if(EXISTS "${CMAKE_SOURCE_DIR}/kernel/third_party/stb_truetype.h")
    target_include_directories(kernel.elf PRIVATE ${CMAKE_SOURCE_DIR}/kernel/third_party)
    target_compile_definitions(kernel.elf PRIVATE CHASEROS_HAVE_STBTT=1)
    message(STATUS "CHASEROS: stb_truetype 已找到，启用 TTF/OTF 矢量字体")
endif()

target_compile_options(kernel.elf PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-ffreestanding;-O2;-Wall;-Wextra;-m64;-fno-stack-protector;-nostdlib;-fno-builtin>
)

if(CHASEROS_WITH_SLIME_USER)
    target_compile_definitions(kernel.elf PRIVATE CHASEROS_HAVE_SLIME_USER=1)
endif()

if(CHASEROS_WITH_AHCI_RUST)
    target_compile_definitions(kernel.elf PRIVATE CHASEROS_HAVE_AHCI_RUST=1)
    add_dependencies(kernel.elf chaseros_ahci_rust_a)
    target_link_libraries(kernel.elf PRIVATE ${CHASEROS_AHCI_RUST_LIB})
endif()

target_compile_definitions(kernel.elf PRIVATE CHASEROS_HYBRID_FS_VIA_IPC=1)

target_link_options(kernel.elf PRIVATE
    -nostdlib
    "-T${CMAKE_SOURCE_DIR}/linker.ld"
)

set_target_properties(kernel.elf PROPERTIES
    LINK_DEPENDS ${CMAKE_SOURCE_DIR}/linker.ld
)

find_program(GRUB_MKRESCUE NAMES grub-mkrescue grub2-mkrescue)

if(GRUB_MKRESCUE AND NOT EXISTS "${CMAKE_SYSROOT}/usr/lib/grub/x86_64-efi")
    message(WARNING "未找到 ${CMAKE_SYSROOT}/usr/lib/grub/x86_64-efi（需安装 grub2-efi-x64 等包）。grub-mkrescue 可能生成仅 BIOS 的 ISO，VMware UEFI 将无法从光盘启动。")
endif()

if(GRUB_MKRESCUE)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/chaseros.iso
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/iso/boot
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:kernel.elf>
                ${CMAKE_SOURCE_DIR}/iso/boot/kernel.elf
        COMMAND ${GRUB_MKRESCUE} -o ${CMAKE_BINARY_DIR}/chaseros.iso ${CMAKE_SOURCE_DIR}/iso
        DEPENDS kernel.elf ${CMAKE_SOURCE_DIR}/iso/boot/grub/grub.cfg
        COMMENT "生成 chaseros.iso (GRUB2)"
        VERBATIM
    )
    add_custom_target(chaseros_iso ALL DEPENDS ${CMAKE_BINARY_DIR}/chaseros.iso)
else()
    message(WARNING "未找到 grub-mkrescue / grub2-mkrescue，跳过 ISO；仅生成 kernel.elf")
endif()

find_program(QEMU_SYSTEM_X86_64 qemu-system-x86_64)
if(QEMU_SYSTEM_X86_64 AND GRUB_MKRESCUE)
    add_custom_target(run
        COMMAND ${QEMU_SYSTEM_X86_64}
            -cdrom ${CMAKE_BINARY_DIR}/chaseros.iso
            -m 128M
            -serial stdio
        DEPENDS chaseros_iso
        USES_TERMINAL
    )
endif()
