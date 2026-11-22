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

## PKG 014 — build, independent QA and PS4 staging completed

- Build workflow: 45072353238 (SUCCESS), source commit 8aacfe426fac164eca681aa87829a509045cbed1.
- Build artifact: ps4-neural-engine-build, artifact ID 14773063921, archive SHA-256 2b06d79b0ae4719df4d39884d3407ed691a5cd7c17fe7d657dbe6072df6ea616.
- Package: PS4-Neural-Engine-014.pkg, Full/Base (gde), APP_VER 01.09, TITLE_ID PNEU00001, 6,619,136 bytes, SHA-256 96c15b46e7579c5f728a131279e986af04a9e4683c9903221e418ca52a3842db.
- Internal build extraction verified all six root-level GFX8 code objects and their SHA-256 values; the build workflow reported the extracted executable and GFX8 payload complete.
- Independent automated QA job independent-pkg-qa: SUCCESS. QA artifact ID 14773838427, archive SHA-256 be7ed56fa6df0b73af97a1085c589991e62503a85ca7805f00f2bf8065b0f03c. QA report confirms Full/Base, SFO semantic fields, PRX hash and QA_STATUS=PASS_PC_ONLY_NOT_HARDWARE_TESTED. The independent extraction listing contains the SHA256 manifest and all six GFX8 assets.
- Host tests: workflow 45072205357 SUCCESS; test suite reports 10/10 and the six-asset decode/hash test reports GCN8_ASSETS=6_OF_6_PASS.
- PS4 staging workflow: 45072657869, job 137362873852, SUCCESS. The prior PKG 013 was preserved at /data/pkg/old/PS4-Neural-Engine-013.pkg (6,619,136 bytes, SHA-256 73c4c9a43002ae55fadec532a9e97217d23e1742d5f4db2bed8a60abdec6f7d1). The new package was uploaded to a temporary path, read back and verified, then promoted to /data/pkg/PS4-Neural-Engine-014.pkg. Final remote read-back: 6,619,136 bytes, SHA-256 96c15b46e7579c5f728a131279e986af04a9e4683c9903221e418ca52a3842db.
- The runner confirmed exactly one Neural Rendering PKG remains in /data/pkg/ (014). Existing installed title PNEU00001 was present. No installation, PRX activation, GNM call, plugins.ini change, or GoldHEN restart was performed.
- QA is automated and PC-only; no independent human code review or hardware/runtime validation was performed.

