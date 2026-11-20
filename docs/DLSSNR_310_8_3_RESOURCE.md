# DLSS-NR 310.8.3 model resource — recovered and verified

**Status:** source archives recovered from the user's Library and verified in a clean working directory. Proprietary DLL and model payload are not committed to this repository.

## Input archive provenance

| Input | Bytes | SHA-256 |
|---|---:|---|
| `PS4-Neural-Engine-source-0.9.0.zip` | 97,187 | `101ef376e4e3e3d948adf6153012bf6482f81586b57f7ebf7d2672237b2c1340` |
| `PS4-Neural-Engine-DLSSNR-model-310.8.3.zip` | 104,025,862 | `36c005a3c51437eb55f09d201473371080b71d873567d865b18fe2a4dd92077a` |
| `FrameRateX - Reshade + DLSS 5 310.8.3 Files.zip` | 167,741,729 | `b51278d5db51b0002f02c81c097da6921c27f99783be51cf23030a28851c449c` |

All three archive hashes match the expected project inputs. They were recovered from the personal Library; no public redistribution is implied.

## DLL and resource verification

Extracted file: `streamline/nvngx_dlssnr.dll`

- DLL bytes: 165,840,496
- DLL SHA-256: `f95feb54137ea11979f9b4ec4f00afd84b5c98a5624d3388fbf6a87714a39fcc`
- PE resource: type ID `10`, name `WEIGHTS_HT`, language `1033`
- Observed PE file offset in this DLL: `0x114a160` (provenance only; the extractor resolves the PE resource tree and does not hardcode this offset)
- Resource bytes: 147,695,410
- Resource SHA-256: `836f445d06ecd2e59bb9f17b84b91c143396fd76ccda1c9dc7fe81d5edd548f4`

The resource was re-extracted with the supplied source tool:

```sh
python3 tools/extract_pe_resource.py nvngx_dlssnr.dll WEIGHTS_HT 1033 weights_ht.bin
python3 tools/inspect_nr_weights.py weights_ht.bin --output inventory.json
python3 tools/export_opendlss_model.py nvngx_dlssnr.dll ./model-export
```

The independent inventory output is byte-identical to the inventory JSON in the source archive (SHA-256 `006e93cb977b5f26cedf0267c3c9beb71cb40cbf8658bfd2ded3b7df1cf630ea`).

## Model directory verification

The archived model package contains `manifest.json` and `model/stages/s00.bin`.

- Tensor records: 153 (152 `layer` records and one `blend_scale`)
- Blocks: 71, numbered 0 through 70
- Concatenated packed stage: 147,683,778 bytes
- `s00.bin` SHA-256: `37a293b5b781b9351a8b4b5fbeb2ff68fbd8a84891378950812ff3b99d161116`
- `manifest.json` SHA-256: `1610780bbc4d40b4f98f25e146063dd2f3ef05d0e6e60af548ae4d4566b43d02`

The export was reproduced from the supplied DLL using `export_opendlss_model.py`. Both the generated manifest and `s00.bin` are byte-for-byte identical to the archived model package. A separate slice audit verified all 153 tensor slices are contiguous, non-overlapping, in-bounds, and account for every byte in the stage.

The C++20 source archive built on the host with GCC 14.2; CTest passed 3/3. The resource-aware Python inspector tests also passed with `PNE_NR_WEIGHTS_RESOURCE` pointing to the re-extracted resource. The exporter round-trip passed for all 153 records.

## Version compatibility finding

The extracted `WEIGHTS_HT` resource hash exactly matches the published DLSS-NR 310.8.0 reference resource hash. Therefore, for this supplied 310.8.3 DLL, **the embedded model-weight resource is byte-identical to the 310.8.0 reference payload**.

This does not prove that every part of the 310.8.3 DLL, graph implementation, preprocessing, conditioning, or runtime ABI is identical to 310.8.0. The reference OpenDLSS-NR repository describes the graph as the 310.8.0 71-block network. The resource parity removes the weight-payload mismatch, but graph/runtime semantic parity still requires independent comparison.

## Remaining semantic limits

The inventory deliberately reports:

- `payloads_decoded=false`
- `dimensions_are_semantic_shapes=false`

Each resource record has a bounded byte payload and a one-dimensional envelope length. The 153 names and record boundaries are verified, but semantic matrix dimensions and per-record offsets still have to be reconciled against the reference `FusedLayout` / graph code. The manifest slices alone do not decode the packed tensors.

The model directory uses one packed stage `s00` with 153 manifest slices. This is compatible with the OpenDLSS-NR model-directory ABI (stage ID, relative file, packed length, SHA-256, tensor slices), but it is not a PS4-ready model and does not mean the 71-block graph is integrated into the Neural Rendering PS4 runtime.

## Licensing and handling

The NVIDIA DLL and extracted weights remain proprietary. Do not commit `nvngx_dlssnr.dll`, `weights_ht.bin`, or `s00.bin` to the public repository. Keep only hashes, inventory metadata, extraction code and reproducible test instructions here. The OpenDLSS-NR layout formulas are MIT-licensed and attributed in `THIRD_PARTY_NOTICES.md`.


## Semantic tensor-layout reconciliation against the 71-block reference graph

A second validation pass now checks every manifest tensor's **expected byte length** against the actual layout formulas in the pinned OpenDLSS-NR reference graph `9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1` (310.8.0), using `FusedLayout`, `upsampleFusedLayout`, the 512-channel split blocks, the eight 1024-channel ViT blocks, and the pre/post heads.

Reproduction command when the model directory is available:

```sh
python3 tools/verify_dlssnr_tensor_layout.py /path/to/DLSS-NR-model-directory
python3 tests/test_dlssnr_layout_formulas.py
```

Verified results:

- Exact tensor-name set: 153/153.
- Expected byte length: 153/153 match the reference graph layout formulas.
- Block IDs: all 71, from 0 through 70.
- Stage slices: contiguous, non-overlapping, no gaps, all 147,683,778 bytes accounted for.
- Eight 2-byte records `block31.layer3.layer` through `block38.layer3.layer` are present but unread by the current reference graph.
- The separate 2-byte `block70.layer0.blend_scale` record is consumed by the demo composition path, not by the graph's neural block schedule.

The host-only formula and graph-export regression suite passed in GitHub Actions runs `44683109821` and `44683103461` (also run `44683126942` on the PR). The full pinned manifest/stage audit passed locally against the recovered Library files. This reconciles all packed tensor record sizes and names with the 310.8.0 graph; it does not decode the E4M3/FP16 payload values or prove that the 310.8.3 DLL's non-weight graph/runtime code is identical.


## Machine-readable 71-block layout export

The reference graph's tensor-layout description can be exported without the proprietary payload:

```sh
python3 tools/export_dlssnr_graph_layout.py dlssnr-310.8.0-graph-layout.json
```

The JSON describes all 71 block IDs, input/output channel widths, block family (fused, split-512, ViT, upsample, pre/post), 11 stage transitions, and all 153 expected tensor records with packed byte lengths and consumer classification (`graph`, `demo`, or `unused_by_current_graph`). It pins the OpenDLSS-NR graph source commit and both resource/stage hashes. Host tests validate the exported block/record/transition counts. This is a machine-readable reference layout; it is not yet the PS4 runtime's executable inference graph.


## Whole-DLL identity and PE metadata (310.8.3)

The recovered FrameRateX archive contains `streamline/nvngx_dlssnr.dll`. The complete DLL was extracted from the original ZIP and hashed:

- DLL SHA-256: `f95feb54137ea11979f9b4ec4f00afd84b5c98a5624d3388fbf6a87714a39fcc`
- PE format: PE32+, x86-64 DLL; image base `0x180000000`; entry RVA `0x7cec0`.
- Version resource: FileVersion and ProductVersion `310.8.3.0`; FileDescription `NVIDIA DLSSNR - DVS PRODUCTION`; OriginalFilename `CL 38718415`.
- Sections: `.text` size `0xAADFC`; `.rdata` `0x3361E`; `.data` `0x1062800`; `.rsrc` `0x8CDAC98`.
- Export table: 55 named exports. Imports include VERSION.dll, ADVAPI32.dll, USER32.dll and KERNEL32.dll.
- Embedded `WEIGHTS_HT` resource: SHA-256 `836f445d06ecd2e59bb9f17b84b91c143396fd76ccda1c9dc7fe81d5edd548f4`, byte-identical to the 310.8.0 reference payload.

These facts authenticate the exact 310.8.3 DLL used for resource extraction and establish that the weight resource matches the 310.8.0 reference. They do **not** establish code-section equivalence, identical exported function behavior, preprocessing parity, or runtime ABI parity between the two versions. No 310.8.0 DLL binary is currently available for a direct PE/code comparison.
