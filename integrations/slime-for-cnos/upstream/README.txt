Slime 上游源码位置（CMake 变量 CNOS_SLIME_SRC）
============================================

默认路径：**integrations/slime-for-cnos/upstream/slime**（与本文件同目录）。

获取上游：

  bash ../../../scripts/fetch-slime-upstream.sh

`-DCNOS_WITH_SLIME_USER=ON` 且 `-DCNOS_BUILD_SLIMEC=ON`（默认）时，内核构建会在该目录执行 **`cargo build --release`**，并把 **`target/release/slimec`** 复制到 **`build/slime-tools/slimec`**，无需把 slimec 安装进 PATH。

**重要**：公用上游仓库若尚未合并 **`--target cnos`**，需在本地 Slime 中保留 **Target::CnosX64** 相关改动，或将 **CNOS_SLIME_SRC** 指到你的 fork（含 cnos 后端）。
