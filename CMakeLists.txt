cmake_minimum_required(VERSION 3.16)

# 首次配置须指定工具链，例如：
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-elf.cmake
# 或使用：cmake --preset default（见 CMakePresets.json）
# 已有 build/ 后只需：cmake -B build 或 cmake --build build，勿重复传工具链，否则 CMake 会警告变量未使用。
if(CMAKE_TOOLCHAIN_FILE)
    message(STATUS "CNOS: CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
endif()

project(CNOS LANGUAGES C ASM_NASM)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

set(KERNEL_INCLUDE ${CMAKE_SOURCE_DIR}/kernel)
set(KERNEL_FS_CNOS ${CMAKE_SOURCE_DIR}/kernel/fs/cnos)
set(KERNEL_FS_INCLUDE ${CMAKE_SOURCE_DIR}/kernel/fs/include)
set(KERNEL_FS_LIB ${CMAKE_SOURCE_DIR}/kernel/fs/lib)
set(KERNEL_FS_EXT2FS ${CMAKE_SOURCE_DIR}/kernel/fs/lib/ext2fs)
set(KERNEL_FS_CNAF ${CMAKE_SOURCE_DIR}/kernel/fs/cnaf)

# e2fsprogs 整树在 kernel/fs/lib 等目录；CNOS 只链接下列 .c，按需再追加 lib/ext2fs/*.c（见 kernel/fs/README）

set(KERNEL_ASM
    kernel/entry.asm
    kernel/interrupts.asm
    kernel/gdt_load.asm
)

set(KERNEL_C
    kernel/console.c
    kernel/sysinfo.c
    kernel/main.c
    kernel/multiboot.c
    kernel/pmm.c
    kernel/vmm.c
    kernel/gdt.c
    kernel/idt.c
    kernel/isr.c
    kernel/power.c
    kernel/drivers/ttfont.c
    kernel/shell.c
    kernel/ipc.c
    kernel/hybrid_ipc.c
    kernel/fs/vfs.c
    kernel/fs/cnos/cnos_alloc.c
    kernel/fs/cnos/cnos_ext2.c
    kernel/fs/cnaf/cnaf_validate.c
    kernel/drivers/vga.c
    kernel/drivers/font.c
    kernel/drivers/graphics.c
    kernel/drivers/serial.c
    kernel/drivers/pci.c
    kernel/drivers/ide.c
    kernel/fs/cnos/cnos_ext2_vol.c
    kernel/process.c
    kernel/elf_load.c
    kernel/user.c
    kernel/user_fd.c
)

set(USER_HELLO_ELF ${CMAKE_BINARY_DIR}/user/hello.elf)
set(USER_HELLO_OBJ ${CMAKE_BINARY_DIR}/hello_elf.o)

# 可选：嵌入 Slime（slimec --target cnos）编译的 hello_sm.elf；默认在仓库内 cargo 编译 slimec（见 CNOS_BUILD_SLIMEC）
option(CNOS_WITH_SLIME_USER "嵌入 Slime 用户态 hello_sm.elf（需 nasm；slimec 见 CNOS_BUILD_SLIMEC / CNOS_SLIMEC）" OFF)
set(CNOS_SLIMEC "" CACHE FILEPATH "仅在 CNOS_BUILD_SLIMEC=OFF 时使用：宿主 slimec 路径；留空则在 PATH 中查找")
set(CNOS_SLIME_SRC "${CMAKE_SOURCE_DIR}/integrations/slime-for-cnos/upstream/slime" CACHE PATH "Slime 源码根目录（须含 Cargo.toml；slimec 须支持 --target cnos）")
option(CNOS_BUILD_SLIMEC "在 CMake 构建内执行 cargo build 生成 slimec（写入 build/slime-tools），不依赖 PATH" ON)
set(KERNEL_EXTRA_OBJ "")

add_custom_command(
    OUTPUT ${USER_HELLO_ELF}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/user
    COMMAND ${CMAKE_C_COMPILER} -ffreestanding -O2 -Wall -Wextra -nostdlib -fno-builtin -fno-stack-protector -m64 -I${CMAKE_SOURCE_DIR}/user/include -c -o ${CMAKE_BINARY_DIR}/user/hello.o ${CMAKE_SOURCE_DIR}/user/hello.c
    COMMAND ${CMAKE_LINKER} -nostdlib -T ${CMAKE_SOURCE_DIR}/user/user.ld -o ${USER_HELLO_ELF} ${CMAKE_BINARY_DIR}/user/hello.o
    DEPENDS ${CMAKE_SOURCE_DIR}/user/hello.c ${CMAKE_SOURCE_DIR}/user/user.ld ${CMAKE_SOURCE_DIR}/user/include/cnos_user.h
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

if(CNOS_WITH_SLIME_USER)
    find_program(CNOS_NASM_FOR_SLIME NAMES nasm REQUIRED)
    set(USER_SLIME_HELLO_ELF ${CMAKE_BINARY_DIR}/user/hello_sm.elf)
    set(USER_SLIME_HELLO_OBJ ${CMAKE_BINARY_DIR}/hello_sm_elf.o)
    set(USER_SLIME_ASM ${CMAKE_BINARY_DIR}/user/hello_sm.asm)
    set(USER_SLIME_O ${CMAKE_BINARY_DIR}/user/hello_sm.o)
    set(CNOS_SLIMEC_EXE "${CMAKE_BINARY_DIR}/slime-tools/slimec")

    set(CNOS_SLIMEC_DEP "")
    if(CNOS_BUILD_SLIMEC)
        if(NOT EXISTS "${CNOS_SLIME_SRC}/Cargo.toml")
            message(FATAL_ERROR "未找到 Slime 源码（${CNOS_SLIME_SRC}/Cargo.toml）。请先: bash scripts/fetch-slime-upstream.sh\n或设置 -DCNOS_SLIME_SRC=含 cnos 目标的 Slime 源码路径。")
        endif()
        find_program(CNOS_CARGO cargo REQUIRED)
        file(GLOB_RECURSE CNOS_SLIME_RUST_SOURCES CONFIGURE_DEPENDS "${CNOS_SLIME_SRC}/src/*.rs")
        add_custom_command(
            OUTPUT ${CNOS_SLIMEC_EXE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/slime-tools"
            COMMAND ${CNOS_CARGO} build --manifest-path "${CNOS_SLIME_SRC}/Cargo.toml" --release
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CNOS_SLIME_SRC}/target/release/slimec" "${CNOS_SLIMEC_EXE}"
            DEPENDS "${CNOS_SLIME_SRC}/Cargo.toml" ${CNOS_SLIME_RUST_SOURCES}
            COMMENT "cargo build slimec（CNOS 内建，源码: ${CNOS_SLIME_SRC}）"
            VERBATIM
        )
        set(CNOS_SLIMEC_FOR_KERNEL "${CNOS_SLIMEC_EXE}")
        set(CNOS_SLIMEC_DEP "${CNOS_SLIMEC_EXE}")
        message(STATUS "CNOS: Slime 用户态 — 将 cargo 编译 slimec 至 ${CNOS_SLIMEC_EXE}")
    else()
        if(NOT CNOS_SLIMEC)
            find_program(CNOS_SLIMEC NAMES slimec REQUIRED)
        endif()
        set(CNOS_SLIMEC_FOR_KERNEL "${CNOS_SLIMEC}")
        message(STATUS "CNOS: Slime 用户态 — 使用宿主 slimec: ${CNOS_SLIMEC_FOR_KERNEL}")
    endif()

    add_custom_command(
        OUTPUT ${USER_SLIME_HELLO_ELF}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/user"
        COMMAND "${CNOS_SLIMEC_FOR_KERNEL}" --target cnos "${CMAKE_SOURCE_DIR}/user/hello.sm" -o "${USER_SLIME_ASM}"
        COMMAND ${CNOS_NASM_FOR_SLIME} -felf64 "${USER_SLIME_ASM}" -o "${USER_SLIME_O}"
        COMMAND ${CMAKE_LINKER} -nostdlib -T "${CMAKE_SOURCE_DIR}/user/user.ld" -o "${USER_SLIME_HELLO_ELF}" "${USER_SLIME_O}"
        DEPENDS "${CMAKE_SOURCE_DIR}/user/hello.sm" "${CMAKE_SOURCE_DIR}/user/user.ld" ${CNOS_SLIMEC_DEP}
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

    set(KERNEL_EXTRA_OBJ ${USER_SLIME_HELLO_OBJ})
endif()

add_executable(kernel.elf ${KERNEL_ASM} ${KERNEL_C} ${USER_HELLO_OBJ} ${KERNEL_EXTRA_OBJ})

target_include_directories(kernel.elf PRIVATE
    ${KERNEL_INCLUDE}
    ${KERNEL_FS_CNOS}
    ${KERNEL_FS_CNAF}
    ${KERNEL_FS_INCLUDE}
    ${KERNEL_FS_LIB}
    ${KERNEL_FS_EXT2FS}
)

if(EXISTS "${CMAKE_SOURCE_DIR}/kernel/third_party/stb_truetype.h")
    target_include_directories(kernel.elf PRIVATE ${CMAKE_SOURCE_DIR}/kernel/third_party)
    target_compile_definitions(kernel.elf PRIVATE CNOS_HAVE_STBTT=1)
    message(STATUS "CNOS: stb_truetype 已找到，启用 TTF/OTF 矢量字体")
endif()

target_compile_options(kernel.elf PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-ffreestanding;-O2;-Wall;-Wextra;-m64;-fno-stack-protector;-nostdlib;-fno-builtin>
)

if(CNOS_WITH_SLIME_USER)
    target_compile_definitions(kernel.elf PRIVATE CNOS_HAVE_SLIME_USER=1)
endif()

target_compile_definitions(kernel.elf PRIVATE CNOS_HYBRID_FS_VIA_IPC=1)

target_link_options(kernel.elf PRIVATE
    -nostdlib
    "-T${CMAKE_SOURCE_DIR}/linker.ld"
)

set_target_properties(kernel.elf PROPERTIES
    LINK_DEPENDS ${CMAKE_SOURCE_DIR}/linker.ld
)

find_program(GRUB_MKRESCUE NAMES grub-mkrescue grub2-mkrescue)

if(GRUB_MKRESCUE)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/cnos.iso
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/iso/boot
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:kernel.elf>
                ${CMAKE_SOURCE_DIR}/iso/boot/kernel.elf
        COMMAND ${GRUB_MKRESCUE} -o ${CMAKE_BINARY_DIR}/cnos.iso ${CMAKE_SOURCE_DIR}/iso
        DEPENDS kernel.elf ${CMAKE_SOURCE_DIR}/iso/boot/grub/grub.cfg
        COMMENT "生成 cnos.iso (GRUB2)"
        VERBATIM
    )
    add_custom_target(cnos_iso ALL DEPENDS ${CMAKE_BINARY_DIR}/cnos.iso)
else()
    message(WARNING "未找到 grub-mkrescue / grub2-mkrescue，跳过 ISO；仅生成 kernel.elf")
endif()

find_program(QEMU_SYSTEM_X86_64 qemu-system-x86_64)
if(QEMU_SYSTEM_X86_64 AND GRUB_MKRESCUE)
    add_custom_target(run
        COMMAND ${QEMU_SYSTEM_X86_64}
            -cdrom ${CMAKE_BINARY_DIR}/cnos.iso
            -m 128M
            -serial stdio
        DEPENDS cnos_iso
        USES_TERMINAL
    )
endif()
