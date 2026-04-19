# gem5 Xerxes Integration

This directory contains the gem5-Xerxes integration.

After applying this to a clean gem5 tree and copying the codes of standalone Xerxes into `ext/xerxes/`, the modified gem5 tree can be built and run directly.

## Manifest

The integration contains:

- `build_opts/`
  - build targets for the Xerxes-enabled gem5 binaries
- `configs/example/`
  - the three example entry points
- `configs/ruby/`
  - Python configuration glue for `MI_example_Xerxes` and `MESI_Two_Level_Xerxes`
- `src/mem/`
  - `XerxesWrapper`, `CohInterface`, and related C++ integration code
- `src/mem/ruby/protocol/`
  - the Xerxes-specific Ruby protocol forks
- selected upstream gem5 files with required integration changes
  - `configs/common/FileSystemConfig.py`
  - `src/mem/SConscript`
  - `src/mem/ruby/protocol/Kconfig`

## Repository Layout

The intended patching layout is:

- apply `Xerxes/gem5/` onto a clean gem5 source tree
- copy the standalone Xerxes repository contents into `gem5/ext/xerxes/`

## Build Targets

The maintained binaries are:

```bash
build/X86_MI_example_Xerxes/gem5.opt
build/X86_MESI_Two_Level_Xerxes/gem5.opt
```

Build from the gem5 root:

```bash
# MI backend and the wrapper-only example
scons defconfig build/X86_MI_example_Xerxes build_opts/X86_MI_example_Xerxes
scons -j8 build/X86_MI_example_Xerxes/gem5.opt --ignore-style

# MESI backend
scons defconfig build/X86_MESI_Two_Level_Xerxes build_opts/X86_MESI_Two_Level_Xerxes
scons -j8 build/X86_MESI_Two_Level_Xerxes/gem5.opt --ignore-style
```

## Example Scripts

This integration keeps three example scripts.

### 1. `configs/example/xerxes_wrapper_minimal.py`

Purpose:

- wrapper-only request/response smoke test
- no `CohInterface -> Ruby` path
- useful for checking the basic `XerxesWrapper` data path
- traffic-only public example

Run command:

```bash
cd gem5
build/X86_MI_example_Xerxes/gem5.opt \
  -d m5out/xerxes_no_coh_traffic \
  configs/example/xerxes_wrapper_minimal.py \
  --max-loads=8 \
  --progress-interval=4
```

### 2. `configs/example/xerxes_mi_example_xerxes.py`

Purpose:

- runs the owner-only `MI_example_Xerxes` backend
- drives a runtime invalidation from `XerxesWrapper`
- exercises the full path `XerxesWrapper -> CohInterface -> Ruby DMA -> completion`

Run command:

```bash
cd gem5
build/X86_MI_example_Xerxes/gem5.opt \
  -d m5out/xerxes_mi_runtime_inv \
  configs/example/xerxes_mi_example_xerxes.py \
  --abs-max-tick=2000000
```

Expected behavior:

- `XerxesWrapper` forwards a real INV packet into `CohInterface`
- `CohInterface` completes the invalidation request
- the Ruby stats show the invalidation/writeback path
- post-invalidation accesses refill the line instead of hitting the old copy

### 3. `configs/example/xerxes_mesi_two_level_xerxes.py`

Purpose:

- runs the `MESI_Two_Level_Xerxes` backend in two modes
- `i_path`: completion when the line is absent from the private hierarchy
- `runtime_inv`: dirty-owner invalidation through the full Xerxes path

I-path command:

```bash
cd gem5
build/X86_MESI_Two_Level_Xerxes/gem5.opt \
  -d m5out/xerxes_mesi_i_path \
  configs/example/xerxes_mesi_two_level_xerxes.py \
  --scenario i_path \
  --abs-max-tick=100000
```

Runtime invalidation command:

```bash
cd gem5
build/X86_MESI_Two_Level_Xerxes/gem5.opt \
  -d m5out/xerxes_mesi_runtime_inv \
  configs/example/xerxes_mesi_two_level_xerxes.py \
  --scenario runtime_inv \
  --abs-max-tick=2000000
```

## What Each Example Covers

- `xerxes_wrapper_minimal.py`
  - the wrapper can be instantiated and can serve basic memory traffic
- `xerxes_mi_example_xerxes.py`
  - a real Xerxes invalidation reaches Ruby and completes correctly
  - the invalidated line is no longer usable as a resident L1 hit
- `xerxes_mesi_two_level_xerxes.py --scenario i_path`
  - the directory handles an external invalidation in the `I + DMA_READ` case
- `xerxes_mesi_two_level_xerxes.py --scenario runtime_inv`
  - the directory/L2/L1 stack handles dirty-owner invalidation and completion

## Validation Status

The maintained example set has been re-checked with:

```bash
configs/example/xerxes_wrapper_minimal.py
configs/example/xerxes_mi_example_xerxes.py
configs/example/xerxes_mesi_two_level_xerxes.py --scenario i_path
configs/example/xerxes_mesi_two_level_xerxes.py --scenario runtime_inv
```

## Important Scope Notes

- TOML configuration used in standalone is intentionally not part of the current maintained gem5 integration surface
- this directory is designed so that `Xerxes/gem5/` can be patched onto gem5 and `Xerxes/` can be copied into `ext/xerxes/` without additional manual gem5-side edits
