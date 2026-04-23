# gem5 子模块使用说明

本仓库将完整 gem5 源码作为子模块放在：

```text
third_party/gem5
```

当前目录下原有的 `gem5/` 不是完整 gem5 树，而是 Xerxes 对 gem5 的 integration overlay。构建前需要把 overlay 应用到完整 gem5 子模块。

## 一键构建

```bash
cd CXL_BETTER_SIM/Xerxes
./scripts/build_gem5_xerxes.sh X86_MESI_Two_Level_Xerxes
```

也可以构建 MI 后端：

```bash
./scripts/build_gem5_xerxes.sh X86_MI_example_Xerxes
```

脚本会执行：

1. 初始化 `third_party/gem5` 子模块；
2. 按 `gem5/MANIFEST.txt` 将 overlay 文件复制到完整 gem5 树；
3. 创建 `third_party/gem5/ext/xerxes -> ../../..`，让 gem5 能包含 standalone Xerxes 头文件；
4. 运行 SCons 构建 `gem5.opt`；
5. 由 SCons 自动生成 `build/<TARGET>/compile_commands.json`。

## 并行度与内存

默认不再直接使用全部 CPU 核心。脚本会读取 `/proc/meminfo` 的 `MemAvailable`，并按下面规则自动选择 SCons 并行度：

```text
PARALLEL = min(CPU 核数, MemAvailable / GEM5_JOB_MEM_MB)
```

`GEM5_JOB_MEM_MB` 默认是 `2200`，表示按每个并发编译任务约 2.2 GiB 内存预算估算，避免小内存机器上 `scons -j $(nproc)` 把内存打满。需要手动控制时可以覆盖：

```bash
PARALLEL=4 ./scripts/build_gem5_xerxes.sh X86_MESI_Two_Level_Xerxes
GEM5_JOB_MEM_MB=3000 ./scripts/build_gem5_xerxes.sh X86_MESI_Two_Level_Xerxes
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

## 注意

- `third_party/gem5` 是官方 gem5 子模块；Xerxes 集成改动通过 overlay 脚本应用，不直接提交到官方 gem5 子模块。
- 应用 overlay 后，`third_party/gem5` 工作区会显示为 dirty，这是预期现象。
- 若要清理 overlay 结果，可在 `third_party/gem5` 中执行 `git reset --hard && git clean -fdx`，然后重新运行构建脚本。
