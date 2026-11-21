# Internal cross-review — GCN8 arena contract and V# codec
Date: 2026-11-21
Reviewed branch: `feature/gcn8-numeric-kernels`
Review classification: **REVISÃO INTERNA — NÃO INDEPENDENTE**. The same assistant context contributed to implementation and review; this is not an independent audit or QA sign-off.

## Scope and method
Static source review of:
- `platform/ps4/gcn8/host/arena_contract.{h,cpp}`
- `tests/test_gcn8_arena_contract.cpp`
- `platform/ps4/gcn8/host/vsharp_codec.{h,cpp}`
- `tests/test_gcn8_vsharp_codec.cpp`
- CMake test registration
Supplementary execution evidence: strict local compile/run of arena arithmetic/allocation tests and GitHub Actions host workflow.

## Findings
### F-01 — Arena-relative alignment is not backing-base alignment
Severity: **MEDIUM / integration constraint**.
The allocator aligns integer offsets relative to zero. This guarantees the eventual absolute address is aligned only if the backing arena base already satisfies the same alignment. The implementation does not know or validate a backing base address. This is acceptable for a host-only offset contract, but callers must enforce base alignment when integrating with a real allocation.
Disposition: documented explicitly in the arena evidence report. Do not treat this as proof of GNM alignment requirements.

### F-02 — Public Arena state is caller-mutable
Severity: **LOW / API contract constraint**.
The `Arena` struct exposes capacity and cursor, so callers can construct inconsistent states. Current allocation rejects a cursor beyond capacity through range validation and does not mutate the cursor on failure. The test suite includes a corrupted-cursor case.
Disposition: retained as a plain host contract; callers must initialize via `arena_init` and avoid unsynchronized mutation.

### F-03 — GPU VA and cache semantics remain unproven
Severity: **BLOCKER for hardware integration; not a defect in host codec**.
The V# codec accepts a numeric 44-bit, 4-byte-aligned address and packs fields. It does not establish that an address is a valid GPU VA, that a given memory type/cache field is correct, or that the descriptor is legal for the PS4 firmware/runtime.
Disposition: explicitly outside scope. Hardware dispatch remains blocked pending primary GNM API evidence, independent review and approved reversible procedure.

## Positive checks
- Checked addition/multiplication clear output on overflow and avoid unsigned wrap.
- Arena allocation validates power-of-two alignment, computes aligned offset with checked addition, validates the half-open range, and commits cursor only after success.
- Range overlap uses checked end calculations; adjacent intervals are non-overlapping.
- V# codec zeroes output before field validation, checks field widths and address range/alignment, and packs explicit masks/shifts without C++ bitfields.
- CMake/CTest registers both host tests.
- Workflow `44865213327` completed successfully; CTest reported 7/7 passed including the arena and V# codec tests.

## Decision
**PASS — host-only contract/code review, with F-01 documented and F-02 constrained.**
**NOT APPROVED** as independent review, GPU runtime approval, PS4 hardware validation, or permission to deploy/activate/dispatch anything on the console.
