# DLSS-NR 310.8.3 model/layout audit

Date: 2026-11-20

## Inputs recovered and hashed

- User source archive `PS4-Neural-Engine-source-0.9.0.zip`: SHA-256 `101ef376e4e3e3d948adf6153012bf6482f81586b57f7ebf7d2672237b2c1340`.
- User model archive `PS4-Neural-Engine-DLSSNR-model-310.8.3.zip`: SHA-256 `36c005a3c51437eb55f09d201473371080b71d873567d865b18fe2a4dd92077a`.
- Exported packed stage: 147,683,778 bytes; SHA-256 `37a293b5b781b9351a8b4b5fbeb2ff68fbd8a84891378950812ff3b99d161116`.
- Source inventory reports 153 records / 147,683,778 payload bytes, resource SHA-256 `836f445d06ecd2e59bb9f17b84b91c143396fd76ccda1c9dc7fe81d5edd548f4`, and `matches_reference_payload=true`.

The resource hash is the published reference payload hash used by the supplied 0.9.0 importer. The packed stage is not redistributed in this repository.

## Structural checks performed

- 71 distinct block IDs, numbered 0 through 70.
- 153 unique tensor records.
- Every tensor slice is in bounds.
- No overlapping slices.
- All 153 slices exactly cover the single packed stage: zero unreferenced bytes.
- Every tensor name, block/layer field, parameter field and byte length matches the source inventory.
- Stage size and SHA-256 match the exported manifest.

## Reference graph layout comparison

Compared all 153 tensor record byte lengths against the graph/layout formulas in OpenDLSS-NR commit `9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1`, specifically `src/nr_graph.cpp` (`fusedLayout`, `preFusedLayout`, `upsampleFusedLayout`, `postFusedLayout`, split-512 block layout and the ViT tensor layout).

Result: **153/153 record names and byte lengths match** the expected 71-block graph layout. The block-4 record includes the 16-byte trailing pad present in the observed 310.8.3 export. No record was silently dropped or reshaped.

Reference documentation:
- https://github.com/maanHimself/OpenDLSS-NR/blob/9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1/docs/network.md
- https://github.com/maanHimself/OpenDLSS-NR/blob/9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1/docs/weights.md
- https://github.com/maanHimself/OpenDLSS-NR/blob/9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1/src/nr_graph.cpp

## Reproduction

```sh
python3 tools/validate_model_manifest.py /path/to/model-dir/manifest.json \
  --inventory /path/to/dlssnr-310.8.3-inventory.json \
  --expected-blocks 71 --expected-tensors 153 \
  --require-full-coverage --check-reference-layout
```

## Scope and remaining gates

This is a byte-level model-container and reference-layout compatibility result. It does **not** prove:
- decoded tensor semantic shapes or numerical equivalence at the 75 reference boundaries;
- a PS4 GCN8 implementation of all required operators;
- GNM allocation, descriptor binding, dispatch, barriers/fences or readback;
- successful inference on PS4 Pro / firmware 14.02, temporal integration, or real-time performance.

The 0.9.0 source archive's host CMake/CTest suite passed 3/3 locally, but its existing `NeuralRenderer` test is a synthetic CNN x2 path, not the 71-block DLSS-NR graph. Keep `compute_available=false` until the real PS4 Pro buffer→compute→buffer smoke test succeeds.
