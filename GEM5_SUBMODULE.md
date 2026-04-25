# gem5 子模块使用说明

本仓库将完整 gem5 源码作为子模块放在：

```text
third_party/gem5
```

当前目录下原有的 `gem5/` 不是完整 gem5 树，而是 Xerxes 对 gem5 的 integration overlay。构建前需要把 overlay 应用到完整 gem5 子模块。

## overlay 与原始 gem5 的关系

`third_party/gem5` 是未经 fork 提交的官方 gem5 子模块，负责提供完整 gem5 源码、SCons 构建系统、Ruby 协议生成器、CPU/内存/设备模型等主体代码。

`gem5/` 是 Xerxes 仓库自己维护的一组 overlay 文件，表示“要放进官方 gem5 树里的 Xerxes 集成补丁”。它不是另一个可独立编译的 gem5，也不是完整源码副本。这个目录里的 `MANIFEST.txt` 明确列出需要复制到 full gem5 树的文件，例如：

- `src/mem/xerxes_wrapper.cc/.hh`：gem5 侧的 Xerxes 内存设备 wrapper。
- `src/mem/coh_interface.cc/.hh`：连接 Ruby DMA/coherence 路径的接口。
- `src/mem/ruby/protocol/*_Xerxes*`：Xerxes 专用 Ruby 协议文件。
- `configs/example/xerxes_*.py` 和 `configs/ruby/*_Xerxes.py`：可运行的 gem5 配置入口。
- `build_opts/X86_*_Xerxes`：SCons/Kconfig 构建目标配置。

`scripts/apply_gem5_overlay.sh` 会按 `gem5/MANIFEST.txt` 把这些文件复制到 `third_party/gem5` 对应位置，并额外创建：

```text
third_party/gem5/ext/xerxes -> ../../..
```

这个 symlink 让 gem5 编译 `xerxes_wrapper.cc` 时能包含 Xerxes standalone 头文件，例如 `bus.hh`、`snoop.hh`、`simulation.hh`。因此，实际编译发生在 `third_party/gem5` 中，但 Xerxes 集成源码的权威来源仍然是本仓库的 `gem5/` overlay 目录。

应用 overlay 后，`third_party/gem5` 会显示 dirty，这是预期的：dirty 内容是由 overlay 复制进去的工作区改动和 symlink，不应该提交到官方 gem5 子模块。需要更新集成逻辑时，应修改 Xerxes 仓库中的 `gem5/` overlay 文件，再重新运行 overlay/build 脚本。

如果想把 `third_party/gem5` 恢复成干净官方子模块：

```bash
cd CXL_BETTER_SIM/Xerxes/third_party/gem5
git reset --hard
git clean -fdx
```

清理后再次运行 `../scripts/build_gem5_xerxes.sh` 或 `./scripts/build_xerxes.sh` 会重新应用 overlay。仓库根目录下仍保留了 `./build.sh` / `./build_debug.sh` 兼容包装脚本。

## 一键构建

常规 Xerxes 构建脚本已经默认包含 gem5：

```bash
./scripts/build_xerxes.sh
./scripts/build_xerxes_debug.sh
```

`scripts/build_xerxes.sh` 默认构建 `gem5.opt`，`scripts/build_xerxes_debug.sh` 默认构建 `gem5.debug`。如果只想构建 standalone Xerxes，可跳过 gem5：

```bash
BUILD_GEM5=0 ./scripts/build_xerxes.sh
BUILD_GEM5=0 ./scripts/build_xerxes_debug.sh
```

也可以手动指定 gem5 目标和二进制类型：

```bash
GEM5_TARGET=X86_MI_example_Xerxes GEM5_BINARY=gem5.opt ./scripts/build_xerxes.sh
GEM5_BINARY=gem5.opt ./scripts/build_xerxes_debug.sh
```

gem5 的 SCons 构建会通过 `ccache` masquerade wrapper 加速；wrapper 目录位于 `${XDG_CACHE_HOME:-$HOME/.cache}/xerxes-gem5-ccache-wrappers`，不会写入源码树。

脚本默认设置 `M5_IGNORE_STYLE=1`，并且每次调用 SCons 都传入 `--ignore-style`，避免 gem5 在 SCons 读取阶段交互式提示安装 `pre-commit` / `commit-msg` hooks，保证自动化构建不会停在 `Press enter to continue`。

单独构建 gem5 时可直接运行：

```bash
cd CXL_BETTER_SIM/Xerxes
./scripts/build_gem5_xerxes.sh X86_MESI_Two_Level_Xerxes
./scripts/build_gem5_xerxes_opt.sh X86_MESI_Two_Level_Xerxes
./scripts/build_gem5_xerxes_debug.sh X86_MESI_Two_Level_Xerxes
```

也可以构建 MI 后端：

```bash
./scripts/build_gem5_xerxes.sh X86_MI_example_Xerxes
```

这几个脚本的关系是：

- `build_gem5_xerxes.sh`：通用入口，默认构建 `gem5.opt`，也可以通过第二个参数或环境变量 `GEM5_BINARY` 改成 `gem5.debug`
- `build_gem5_xerxes_opt.sh`：固定构建 `gem5.opt`
- `build_gem5_xerxes_debug.sh`：固定构建 `gem5.debug`

脚本会执行：

1. 初始化 `third_party/gem5` 子模块；
2. 按 `gem5/MANIFEST.txt` 将 overlay 文件复制到完整 gem5 树；
3. 创建 `third_party/gem5/ext/xerxes -> ../../..`，让 gem5 能包含 standalone Xerxes 头文件；
4. 运行 SCons 构建 `gem5.opt`；
5. 由 SCons 自动生成 `build/<TARGET>/compile_commands.json`。

## 编译产物怎么使用

编译完成后的 gem5 二进制位于：

```text
third_party/gem5/build/<TARGET>/<BINARY>
```

常用默认值是：

```text
third_party/gem5/build/X86_MESI_Two_Level_Xerxes/gem5.opt
third_party/gem5/build/X86_MESI_Two_Level_Xerxes/gem5.debug
```

`gem5.opt` 适合正常实验运行，`gem5.debug` 适合调试断言、backtrace 和更详细的问题定位。它们都是标准 gem5 可执行文件，只是内部多了 Xerxes wrapper、coherence interface、Xerxes Ruby 协议和示例配置。

运行时从 `third_party/gem5` 目录调用该二进制，并传入 overlay 提供的配置脚本。例如：

```bash
cd CXL_BETTER_SIM/Xerxes/third_party/gem5
./build/X86_MESI_Two_Level_Xerxes/gem5.opt \
  configs/example/xerxes_mesi_two_level_xerxes.py
```

MI 示例目标类似：

```bash
cd CXL_BETTER_SIM/Xerxes/third_party/gem5
./build/X86_MI_example_Xerxes/gem5.opt \
  configs/example/xerxes_mi_example_xerxes.py
```

具体配置参数以后应以对应 `configs/example/xerxes_*.py` 的 argparse/脚本内容为准。一般工作流是：先用 `./scripts/build_xerxes.sh` 或 `scripts/build_gem5_xerxes.sh` 生成二进制和 `compile_commands.json`，再从 `third_party/gem5` 运行 `build/<TARGET>/gem5.*`，仿真输出默认进入 gem5 的 `m5out/` 目录，或由 gem5 参数指定输出目录。

## 并行度与内存

默认不再直接使用全部 CPU 核心。脚本会读取 `/proc/meminfo` 的 `MemAvailable`，并按下面规则自动选择并行度。`scripts/build_xerxes.sh` / `scripts/build_xerxes_debug.sh` 用 `XERXES_JOB_MEM_MB` 控制 CMake 构建预算，`scripts/build_gem5_xerxes.sh` 用 `GEM5_JOB_MEM_MB` 控制 gem5 SCons 构建预算：

```text
PARALLEL = min(CPU 核数, MemAvailable / GEM5_JOB_MEM_MB)
```

`GEM5_JOB_MEM_MB` 默认是 `2200`，表示按每个并发编译任务约 2.2 GiB 内存预算估算，避免小内存机器上 `scons -j $(nproc)` 把内存打满。需要手动控制时可以覆盖：

```bash
PARALLEL=4 ./scripts/build_gem5_xerxes.sh X86_MESI_Two_Level_Xerxes
GEM5_JOB_MEM_MB=3000 ./scripts/build_gem5_xerxes.sh X86_MESI_Two_Level_Xerxes
XERXES_JOB_MEM_MB=1500 ./scripts/build_xerxes.sh
```

## clangd

不要手写 `compile_commands.json`。构建完成后，在编辑器中让 clangd 使用：

```text
CXL_BETTER_SIM/Xerxes/third_party/gem5/build/X86_MESI_Two_Level_Xerxes/compile_commands.json
```

如果编辑器从 `third_party/gem5` 打开，一般会自动发现对应 build 目录中的编译数据库。若没有自动发现，可在编辑器 clangd 参数中设置：

```text
--compile-commands-dir=/root/code/cxlkv/CXL_BETTER_SIM/Xerxes/third_party/gem5/build/X86_MESI_Two_Level_Xerxes
```

注意：SCons 生成的 `compile_commands.json` 面向 full gem5 工作树，也就是 `third_party/gem5/src/...`。如果直接编辑 Xerxes overlay 源文件，例如：

```text
CXL_BETTER_SIM/Xerxes/gem5/src/mem/coh_interface.cc
```

clangd 无法把这个路径和编译数据库中的 `third_party/gem5/src/mem/coh_interface.cc` 自动对应起来，所以即使 gem5 已经编译成功，overlay 文件仍可能报找不到 `params/*.hh`、`base/*.hh`、`mem/*.hh` 等错误。

为此，`gem5/.clangd` 给 overlay 目录补充了 full gem5 的 include 路径、生成头文件目录和必要宏。它只影响编辑体验，不参与 SCons 构建，也不会手动修改 `compile_commands.json`。如果要获得和真实编译完全一致的 clangd 结果，也可以直接打开/编辑 overlay 已应用后的文件：

```text
CXL_BETTER_SIM/Xerxes/third_party/gem5/src/mem/coh_interface.cc
```

但长期维护时仍应把源码改回 `CXL_BETTER_SIM/Xerxes/gem5/` overlay，再重新运行 build/overlay 脚本。

## 注意

- `third_party/gem5` 是官方 gem5 子模块；Xerxes 集成改动通过 overlay 脚本应用，不直接提交到官方 gem5 子模块。
- 应用 overlay 后，`third_party/gem5` 工作区会显示为 dirty，这是预期现象。
- 若要清理 overlay 结果，可在 `third_party/gem5` 中执行 `git reset --hard && git clean -fdx`，然后重新运行构建脚本。
