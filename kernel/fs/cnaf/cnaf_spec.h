/*
 * CNAF / CNAFL — 规范（ChaserOS Application Files & Libs）
 *
 * 本文档以头文件形式固定命名与布局约定，便于内核、加载器与宿主工具链共用一个“真相源”。
 * 实现顺序：先只读解析与校验 → 再与 VFS/块设备挂钩 → 最后用户态加载与链接。
 */

#ifndef CHASEROS_CNAF_SPEC_H
#define CHASEROS_CNAF_SPEC_H

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* 命名                                                                        */
/* -------------------------------------------------------------------------- */

/*
 * CNAF — ChaserOS Application File（.cnaf）
 *   可部署的「应用单元」：清单 + 可选资源包 + 主可加载映像。
 *   内核 `cnrun` / 内嵌 demo 路径下，IMAGE 节载荷为 **CNAB**（见 kernel/fs/cnaf/cnab.h）：
 *   平铺机器码 + 头描述 load_base / entry_rva；**不解析 ELF**。宿主工具链可仍用 ELF 链接，
 *   再以 objcopy + wrap-cnab.py 转为 CNAB 后打包。
 *
 * CNAFL — ChaserOS Application Files Library（.cnafl）
 *   可分发「库单元」：清单 + 一个可链接/可加载映像（静态归档 a 或动态共享 ELF .so 占位）。
 *
 * 关系：CNAF 在清单中声明对若干 CNAFL 的依赖；CNAFL 不依赖 CNAF。系统库放在 CNAF_SYS_LIB_DIR，
 * 用户应用放在 CNAF_USER_APPS_DIR 或 CNAF_SYS_APPS_DIR。
 */

#define CNAF_FILE_EXT  ".cnaf"
#define CNAFL_FILE_EXT ".cnafl"

#define CNAF_SYS_APPS_DIR   "/System/Apps"
#define CNAF_SYS_LIB_DIR    "/System/Lib"
#define CNAF_USER_APPS_DIR  "/Home/Apps"

/* -------------------------------------------------------------------------- */
/* 文件头（磁盘 / 内存映像共用；小端）                                           */
/* -------------------------------------------------------------------------- */

#define CNAF_MAGIC_U32 0x46414E43u /* 'C' 'N' 'A' 'F' 小端序 */

#define CNAF_FMT_MAJOR 0u
/*
 * v0.1 — 仅文件头；header_bytes 之后可视为整段 IMAGE（遗留兼容）。
 * v0.2 — 节表 + 清单/资源/映像分离；见「节表与布局」。
 */
#define CNAF_FMT_MINOR 2u

typedef enum {
    CNAF_PAYLOAD_APP  = 1,
    CNAF_PAYLOAD_LIB  = 2,
} cnaf_payload_kind_t;

/*
 * flags（按位；未使用位必须为 0）
 */
#define CNAF_FLAG_SECTION_TABLE  (1u << 0) /* 1：header 后紧跟节表（v0.2）；0：遗留单段映像 */
#define CNAF_FLAG_SIGNED         (1u << 1) /* 预留：尾部或独立块含签名 */
#define CNAF_FLAG_COMPRESSED     (1u << 2) /* 预留：某节或整包压缩 */

typedef struct cnaf_file_header {
    uint32_t magic;           /* CNAF_MAGIC_U32 */
    uint16_t fmt_major;
    uint16_t fmt_minor;
    uint32_t payload_kind;    /* cnaf_payload_kind_t */
    uint32_t header_bytes;    /* 从文件开头到「节载荷区」起始的字节数（含本头与节表），8 字节对齐 */
    uint32_t flags;           /* CNAF_FLAG_* */
    uint8_t  id[16];          /* UUID 或构建 id；全 0 表示未设置 */
} cnaf_file_header_t;

#define CNAF_HEADER_ALIGN 8u

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(cnaf_file_header_t) == 36, "cnaf_file_header size");
#endif

/* -------------------------------------------------------------------------- */
/* 节表（v0.2；当 flags & CNAF_FLAG_SECTION_TABLE 时存在）                       */
/* -------------------------------------------------------------------------- */

typedef enum {
    CNAF_SECTION_MANIFEST   = 1, /* UTF-8 文本清单，见下「清单格式」 */
    CNAF_SECTION_RESOURCES  = 2, /* 可选；tar/zip 或 ChaserOS 自定义资源包（由 manifest 指明子格式） */
    CNAF_SECTION_IMAGE      = 3, /* CNAF App：CNAB（内核装载）；CNAFL：.a 或 ELF .so 等 */
    CNAF_SECTION_DEBUG      = 4, /* 可选；DWARF 等，加载器默认忽略 */
} cnaf_section_type_t;

/*
 * 每条描述文件中一段连续字节区域；offset 为从文件开头的绝对偏移。
 */
typedef struct cnaf_section_entry {
    uint32_t type;      /* cnaf_section_type_t */
    uint32_t flags;     /* 按节类型细化；默认 0 */
    uint64_t offset;    /* 节内容起始 */
    uint64_t size;      /* 节内容长度（字节） */
} cnaf_section_entry_t;

typedef struct cnaf_section_table {
    uint32_t count;     /* 条目数 */
    uint32_t reserved;  /* 0 */
    /* 紧随 count * cnaf_section_entry_t */
} cnaf_section_table_t;

/*
 * 布局约定
 * --------
 * 遗留 v0.1（fmt_minor==1 或未置 CNAF_FLAG_SECTION_TABLE）：
 *   [cnaf_file_header][IMAGE 直至 EOF]，且通常 header_bytes == sizeof(cnaf_file_header)。
 *
 * v0.2（fmt_minor>=2 且 CNAF_FLAG_SECTION_TABLE）：
 *   [cnaf_file_header][cnaf_section_table 头][cnaf_section_entry × count]
 *   节表结束后按 CNAF_HEADER_ALIGN 向上对齐得到 header_bytes，其后为各节数据（offset 可指向对齐后任意位置）。
 *
 * CNAF（App）必须包含：MANIFEST + IMAGE；RESOURCES/DEBUG 可选。
 * CNAFL（Lib）必须包含：MANIFEST + IMAGE；RESOURCES/DEBUG 可选。
 */

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

#define CNAF_MANIFEST_KEY_FORMAT_VERSION "format_version"
#define CNAF_MANIFEST_KEY_BUNDLE_ID      "bundle_id"
#define CNAF_MANIFEST_KEY_NAME           "name"
#define CNAF_MANIFEST_KEY_VERSION        "version"
#define CNAF_MANIFEST_KEY_IMAGE_KIND     "image_kind"
#define CNAF_MANIFEST_KEY_ENTRY_SYMBOL   "entry_symbol"
#define CNAF_MANIFEST_KEY_REQUIRES       "requires"

#define CNAF_MANIFEST_KEY_LIB_NAME  "lib_name"
#define CNAF_MANIFEST_KEY_SONAME    "soname"
#define CNAF_MANIFEST_KEY_ABI_MAJOR "abi_major"
#define CNAF_MANIFEST_KEY_ABI_MINOR "abi_minor"
#define CNAF_MANIFEST_KEY_LIB_KIND  "lib_kind"

/*
 * requires 语法（逗号分隔；每项）：
 *   lib_name [@abi_major[.abi_minor]]
 * 示例：cnui@1.0  net@2  libc（省略版本时由加载器按 CNAF_SYS_LIB_DIR 中最高兼容 abi 解析，策略可配置）。
 */

typedef enum {
    CNAF_LIB_KIND_STATIC = 1, /* IMAGE 为 ar 归档，链接时并入 */
    CNAF_LIB_KIND_SHARED = 2, /* IMAGE 为 ELF shared object，运行时加载 */
} cnaf_lib_kind_t;

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
    CNAF_OK = 0,
    CNAF_ERR_MAGIC = 1,
    CNAF_ERR_VERSION = 2,
    CNAF_ERR_TRUNCATED = 3,
    CNAF_ERR_KIND = 4,
    CNAF_ERR_SECTION = 5,
    CNAF_ERR_MANIFEST = 6,
    /* 预留 */
} cnaf_err_t;

/* -------------------------------------------------------------------------- */
/* 实施路线图（非枚举，仅供维护者对照）                                          */
/* -------------------------------------------------------------------------- */

/*
 * Phase A — 头校验：魔数、fmt、payload_kind；cnaf_probe_header。
 * Phase B — VFS：从路径打开 .cnaf/.cnafl，校验头与（可选）节表一致性。
 * Phase C — 内核/用户态加载器：解析 MANIFEST + IMAGE；App 映像按 CNAB 平铺装入（非 ELF）。
 * Phase D — requires：按 lib_name/abi 解析 CNAFL，共享库符号解析与重定位。
 *
 * 与 libext2fs：仅通过 VFS 读文件；CNAF 不依赖磁盘 inode 布局。
 */

#endif /* CHASEROS_CNAF_SPEC_H */
