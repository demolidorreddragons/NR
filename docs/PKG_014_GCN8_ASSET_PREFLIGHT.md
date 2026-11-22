# PKG 014 — GFX8 asset integrity preflight

Date: 2026-11-22
Branch: feature/gcn8-numeric-kernels
Release candidate: Full/Base (gde), TITLE_ID PNEU00001, APP_VER 01.09, PKG 014.

## Technical change

PKG 014 extends the PKG 013 process-local preflight with six compiled GFX8 shader code objects. At launch, the app computes SHA-256 over each file in /app0/nr_gcn8_ and compares it with the pinned digest compiled into the preflight. A missing or altered file fails closed before GNM module lookup. The existing five GNM exports are then resolved by DLSYM only.

This is a real package-content and runtime-integrity check. It is not a compute backend: none of the binaries is submitted to the GPU, no GNM export is called, no command buffer is submitted, and no Neural Rendering PRX is activated.

## Binary provenance

Source workflow: GCN8 shader compiler and GEMM binary, run 45062384927, source commit a1070555f3465b929e47bd1a1109e96f7d92bcb3.
Source artifact: nr-gcn8-compute-shaders-gfx8, artifact ID 14770655379, ZIP size 26,065 bytes, ZIP digest sha256:c329001734e6f72801067884589420dfa7a94773973a8deaad724286f4781da0.

| GFX8 code object | Bytes | SHA-256 |
|---|---:|---|
| compute_smoke_add.gcn.bin | 88 | bd6deda8f6fc7cc04f78b441864a0b57d63f259f8d048d6ac09747f0cc6988e5 |
| e4m3_decode.gcn.bin | 856 | 08ed8e40dbab45038312f3d4db49a2dc49f209e7b393bc2c497a4b42ccb70c60 |
| e4m3_encode.gcn.bin | 540 | 86b8c0a49fc46d513a27f344b1515cc7ab38353878215b29238d27d74e4d0142 |
| gemm_fp16_accum.gcn.bin | 364 | 5bf7129019cab1e40a67b572dba41167c3e5346bfe3360ded939f84894008048 |
| gemm_fp32.gcn.bin | 292 | ca0325130b9b77294a3fd7c32957d076f90741584901570a2c1be5521a099412 |
| gemm_fp32_wave64_tiled.gcn.bin | 948 | 5dd8cba2710502d5c1f930e4e0ddbe779a56b168900f1040c0e95b28c0f3f8bc |

The base64 source files are kept under assets/gcn8/. tools/prepare_gcn8_pkg_assets.py decodes them and checks exact byte counts and SHA-256 before packaging. tools/verify_gcn8_assets.py is the independent post-extraction verifier used by package QA. Files are deliberately flattened into the app0 root because the pinned GP4 generator only builds directory nodes declared in the GP4 tree; file paths alone do not create arbitrary parent directories.

## Safety and limitations

- PRX activation: false.
- GNM calls: none.
- Compute dispatch/readback: not run.
- Persistent system/user data writes: none.
- GNM ABI, GPU VA/V# mapping, InputUsage semantics, cache/coherency, synchronization and firmware behavior remain unresolved.
- Shader compilation and asset hashing do not establish runtime compatibility or neural inference.

## Release gate

The candidate is not installable until the current PRX/PKG workflow and its independent QA job pass, the GP4/SFO/payload extraction is reviewed, and the Windows bridge staging/read-back matches the final artifact. QA is PC-only; hardware status remains not tested.
