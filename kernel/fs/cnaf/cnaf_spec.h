/* ChaserOS DLL & Application General Format (CNPK/CNLK/CNIM, v1). */

#ifndef CHASEROS_CNAF_SPEC_H
#define CHASEROS_CNAF_SPEC_H

#include <stdint.h>

#define CHASEROS_DAGF_NAME "ChaserOS DLL & Application General Format"

/* -------------------------------------------------------------------------- */
/* 命名                                                                        */
/* -------------------------------------------------------------------------- */

#define CNPK_FILE_EXT ".cnpk"
#define CNLK_FILE_EXT ".cnlk"
#define CNPKG_SYS_APPS_DIR "/System/Apps"
#define CNPKG_SYS_LIB_DIR "/System/Lib"
#define CNPKG_USER_APPS_DIR "/Home/Apps"

/* -------------------------------------------------------------------------- */
/* 文件头（磁盘 / 内存映像共用；小端）                                           */
/* -------------------------------------------------------------------------- */

#define CNPKG_MAGIC_CNPK 0x4B504E43u /* "CNPK" */
#define CNPKG_MAGIC_CNLK 0x4B4C4E43u /* "CNLK" */
#define CNPKG_FMT_MAJOR 1u
#define CNPKG_FMT_MINOR 0u

typedef enum {
    CNPKG_KIND_PROGRAM = 1,
    CNPKG_KIND_LIBRARY = 2,
} cnpkg_kind_t;

#define CNPKG_FLAG_SIGNED (1u << 0)
#define CNPKG_FLAG_COMPRESSED (1u << 1)

typedef struct cnpkg_file_header {
    uint32_t magic;           /* CNPKG_MAGIC_CNPK/CNLK */
    uint16_t fmt_major;
    uint16_t fmt_minor;
    uint32_t pkg_kind;        /* cnpkg_kind_t */
    uint32_t header_bytes;    /* includes section table */
    uint32_t flags;           /* CNPKG_FLAG_* */
    uint32_t section_count;
    uint8_t build_id[16];
} cnpkg_file_header_t;

#define CNPKG_HEADER_ALIGN 8u

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(cnpkg_file_header_t) == 40, "cnpkg_file_header size");
#endif

/* -------------------------------------------------------------------------- */
/* 节表（v0.2；当 flags & CNAF_FLAG_SECTION_TABLE 时存在）                       */
/* -------------------------------------------------------------------------- */

typedef enum {
    CNPKG_SECTION_MANIFEST = 1,
    CNPKG_SECTION_IMAGE = 2,
    CNPKG_SECTION_DYNSTR = 3,
    CNPKG_SECTION_DYNSYM = 4,
    CNPKG_SECTION_RELA = 5,
    CNPKG_SECTION_RESOURCES = 6,
} cnpkg_section_type_t;

typedef struct cnpkg_section_entry {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t size;
} cnpkg_section_entry_t;

/* -------------------------------------------------------------------------- */
/* 清单（MANIFEST 节）：UTF-8，行文本                                           */
/* -------------------------------------------------------------------------- */

/*
 * 语法（极简键值，便于无 libc 解析）：
 *   - 每行一条：key = value（等号两侧允许空格）。
 *   - 以 # 开头的行与空行忽略。
 *   - key：小写字母、数字、下划线 [a-z0-9_]+。
 *   - value：去首尾空白后的剩余部分；若需含 # 或换行，使用转义或后续版本改为二进制 TLV（未定义前勿依赖）。
 *
 * 通用键（CNAF 与 CNAFL 均可出现）：
 *   format_version = 1          # 清单语法版本，当前固定 1
 *   bundle_id = com.example.foo # 反向 DNS 风格唯一 id（CNAF 强烈建议；CNAFL 建议）
 *   name = My App               # 短显示名（UTF-8）
 *   version = 1.2.3             # 语义化版本字符串（工具链约束，内核可只作字符串比较）
 *
 * CNAF 专用：
 *   image_kind = cnab           # 建议显式声明（内核只认 CNAB IMAGE）
 *   entry_symbol = _start       # 文档/工具用；实际入口由 CNAB entry_rva + load_base 决定
 *   requires = cnui@1.0, net@2  # 依赖 CNAFL，逗号分隔，见下「依赖语法」
 *
 * CNAFL 专用：
 *   lib_name = cnui             # 逻辑名，用于 requires 解析；与文件名（去 .cnafl）应对应
 *   soname = libcnui.so.1       # 动态链占位：SONAME 等价物；静态库可省略
 *   abi_major = 1               # 与 ChaserOS 加载器/内核约定的接口主版本
 *   abi_minor = 0               # 次版本：兼容同 abi_major 时递增
 *   lib_kind = static           # static | shared（见 cnaf_lib_kind_t）
 */

#define CNPKG_MANIFEST_KEY_BUNDLE_ID "bundle_id"
#define CNPKG_MANIFEST_KEY_NAME "name"
#define CNPKG_MANIFEST_KEY_VERSION "version"
#define CNPKG_MANIFEST_KEY_REQUIRES "requires"
#define CNPKG_MANIFEST_KEY_SONAME "soname"
#define CNPKG_MANIFEST_KEY_ABI_MAJOR "abi_major"
#define CNPKG_MANIFEST_KEY_ABI_MINOR "abi_minor"

typedef enum {
    CNLK_LIB_KIND_STATIC = 1,
    CNLK_LIB_KIND_SHARED = 2,
} cnlk_lib_kind_t;

/* -------------------------------------------------------------------------- */
/* IMAGE 节内容（按 payload_kind / lib_kind）                                    */
/* -------------------------------------------------------------------------- */

/*
 * CNAF + IMAGE（内核加载路径）：**CNAB** 裸映像。构建侧可先用 x86_64-elf 链出 ELF，再
 *   objcopy -O binary → wrap-cnab.py → pack-cnaf.py。
 * CNAFL + static：IMAGE 为 .a（通用 ar，成员为 ELF64 .o）。
 * CNAFL + shared：IMAGE 为 ET_DYN，SONAME 应与 manifest soname 一致（或由加载器校验）。
 */

/* -------------------------------------------------------------------------- */
/* 错误码（加载器 / 内核解析共用）                                               */
/* -------------------------------------------------------------------------- */

typedef enum {
    CNPKG_OK = 0,
    CNPKG_ERR_MAGIC = 1,
    CNPKG_ERR_VERSION = 2,
    CNPKG_ERR_TRUNCATED = 3,
    CNPKG_ERR_KIND = 4,
    CNPKG_ERR_SECTION = 5,
    CNPKG_ERR_MANIFEST = 6,
} cnpkg_err_t;

typedef struct cnim_segment {
    uint64_t file_offset;
    uint64_t virt_addr;
    uint64_t file_size;
    uint64_t mem_size;
    uint32_t flags;
    uint32_t align;
} cnim_segment_t;

typedef struct cnim_rela {
    uint64_t offset;
    uint64_t addend;
    uint32_t type;
    uint32_t sym_index;
} cnim_rela_t;

#define CNIM_MAGIC_U32 0x4D494E43u /* "CNIM" */
#define CNIM_FMT_MAJOR 1u
#define CNIM_FMT_MINOR 0u
#define CNIM_HEADER_SIZE 64u

typedef struct cnim_header {
    uint32_t magic;
    uint16_t fmt_major;
    uint16_t fmt_minor;
    uint32_t header_size;
    uint32_t flags;
    uint64_t entry_rva;
    uint64_t image_base;
    uint32_t segment_count;
    uint32_t rela_count;
    uint64_t segment_table_offset;
    uint64_t rela_table_offset;
    uint64_t reserved0;
} cnim_header_t;

typedef enum {
    CNIM_RELOC_NONE = 0,
    CNIM_RELOC_RELATIVE64 = 1,
} cnim_reloc_type_t;

#endif /* CHASEROS_CNAF_SPEC_H */
