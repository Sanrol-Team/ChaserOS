ChaserOS AHCI（Rust staticlib）
==============================

定位
----
* 内核块设备 **AHCI** 实现放在本 crate，编译为 **`libchaseros_ahci_rust.a`**，由 CMake **与 C/汇编目标文件一同链入** `kernel.elf`。
* **Slime** 仍通过 **slimec → NASM → 用户 ELF/CNAF**；本仓库不把 Slime 源码编译进内核。
* **与 Slime 的整合点**：同一构建机上的 **`cargo` / `rustc`**（以及可选 **`rustup target add x86_64-unknown-none`**），与编译 slimec 时一致，便于 CI 统一安装 Rust。

构建（统一用 **cargo**）
----
```bash
rustup target add x86_64-unknown-none   # 一次性
cd kernel/ahci_rust
cargo build --release
# 默认 target 见 .cargo/config.toml；产物目录可用 CARGO_TARGET_DIR 覆盖
```

CMake 使用 **`-DCHASEROS_WITH_AHCI_RUST=ON`** 时在 **`kernel/ahci_rust`** 下执行  
**`CARGO_TARGET_DIR=<build>/ahci_rust cargo build --release`**（与手动 `cargo` 一致），  
产出 **`<build>/ahci_rust/x86_64-unknown-none/release/libchaseros_ahci_rust.a`**。

可选指定 cargo 路径：**`-DCHASEROS_CARGO=/path/to/cargo`**（与 Slime 内建 slimec 共用）。

手动与 CMake 同路径：

```bash
BUILD_DIR="$(pwd)/build" bash scripts/build-ahci-rust.sh
```

QEMU
----
启用 AHCI 时勿再使用 legacy IDE 主盘；示例见仓库根目录 **`run.sh`**（`CHASEROS_QEMU_AHCI=1`）。

C 侧入口
--------
`kernel/drivers/ahci_pci.c`：PCI 枚举 class **0x01/0x06/0x01**（AHCI），取 **BAR5** 为 ABAR，调用 `chaseros_ahci_rust_init` / `chaseros_ahci_rust_identify`。
