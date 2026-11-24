# Gate B — firmware 14.02 ABI evidence sweep

Date: 2026-11-24

## Objective

Determine whether public SDK headers, OpenOrbis declarations, OpenGNM's published
API reference, emulator HLEs, or public reverse-engineering reports provide a
firmware-14.02-qualified ABI for the first compute path. This is a read-only
source/documentation audit. It does not authorize console probing or dispatch.

## Evidence classification

| Evidence | What it establishes | What it does not establish |
|---|---|---|
| OpenOrbis `GnmDriver.h` at `0a1aaf9dd4a92695538bdeb09fb056d06dd11725` | Names exports including `sceGnmDispatchDirect`, `sceGnmDispatchInitDefaultHardwareState`, `sceGnmMapComputeQueueWithPriority`, `sceGnmDingDong`, `sceGnmUnmapComputeQueue`, and `sceGnmSubmitCommandBuffers`. | Most compute declarations are old-style empty-parameter `void name();` declarations. They do not encode argument count/types, return type, calling convention, reservation units, or firmware version. |
| OpenGNM pinned source/API docs | Typed candidate signatures and local PM4 builder behavior; Orbis backend forwards selected calls to firmware exports. | No exact import/NID or ABI capture tied to retail FW 14.02; no proof the target export resolves or behaves as documented. |
| shadPS4 HLE pinned source | Emulator model for compute init, queue mapping, doorbell, dispatch, and compute wait packet generation. | Not Sony firmware; not a retail FW 14.02 trace or ABI record. |
| Reverse-engineered shipping-title report | Evidence that a commercial PS4 title uses compute dispatch, ACE queue mapping/doorbell, and EOP/event-based synchronization. | Different title/firmware baseline; incomplete exact ABI and no NR hardware result. |
| Public FW 14.02 kernel-offset references | Some kernel payload projects explicitly identify FW 14.02 kernel build/offset support. | Kernel offsets do not identify `libSceGnmDriver` export NIDs, prototypes, GPU driver semantics, or GNM compute ABI. |

## Specific conclusions

1. The inspected OpenOrbis header is not a typed ABI source for the compute
   functions. For example, `sceGnmDispatchDirect()`,
   `sceGnmDispatchInitDefaultHardwareState()`,
   `sceGnmMapComputeQueueWithPriority()`, and `sceGnmDingDong()` are
   declared as empty-parameter `void` functions. This is insufficient to
   safely call them from NR and must not be treated as authoritative ABI.

2. OpenGNM's public reference gives useful candidate signatures:
   - `sceGnmDispatchDirect(uint32_t*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)`
   - `sceGnmDispatchInitDefaultHardwareState(uint32_t*, uint32_t)`
   - `sceGnmMapComputeQueueWithPriority(uint32_t, uint32_t, uintptr_t, uint32_t, uint32_t*, uint32_t)`
   - `sceGnmDingDong(uint32_t, uint32_t)`
   - `sceGnmComputeWaitOnAddress(uint32_t*, uint32_t, uintptr_t, uint32_t, uint32_t, uint32_t)`

   These are library-maintained declarations, not a Sony SDK header or a
   firmware-14.02 import capture. Their use in a target binary still requires
   symbol resolution and calling-convention verification.

3. The OpenOrbis header's typed declarations for
   `sceGnmSubmitCommandBuffers` and `sceGnmSubmitDone` are more useful than
   its compute declarations, but they still do not qualify export availability,
   argument semantics, or completion behavior on FW 14.02.

4. Public evidence found for FW 14.02 kernel support concerns kernel offsets
   and payload compatibility, not the GNM userland driver ABI. No inspected
   public source provided a 14.02-specific `libSceGnmDriver` NID table,
   import stub/prologue, symbol-resolution trace, or retail call trace for the
   compute functions above.

5. Therefore neither the DCB compute route nor the ACE queue route can yet be
   selected as the NR hardware path solely from public source. The title
   reverse-engineering report makes ACE credible; OpenGNM's local DCB packet
   builders make DCB credible. Neither route has the target ABI and memory
   contracts needed for a safe dispatch.

## Required evidence to close this ABI sub-gate

A read-only, target-specific evidence package must identify, for the chosen
route:

- exact FW 14.02 `libSceGnmDriver` export/NID or import resolution for each
  called function;
- function prototype, PS4 calling convention, return/error semantics and
  command-buffer size units;
- compute-init reservation and actual emitted dword count;
- submit/doorbell ownership and ordering;
- GPU virtual-address mapping, alignment, residency and resource lifetime;
- completion primitive, finite timeout, failure handling and CPU-visible
  readback/cache operation.

A symbol name in a header or an emulator implementation is not sufficient.
Any target probe must be explicitly approved, read-only until the isolated
compute harness is separately authorized, and must not change unrelated
console state.

## Gate B disposition

This sweep closes the public-source ABI search subtask, not Gate B itself.
The public sources examined do not supply a firmware-14.02-qualified compute
ABI. The remaining requirement is target-specific evidence and a controlled
PS4 Pro compute result with deterministic full-buffer readback.

Gate B remains **BLOCKED**. Gate A remains PASS (artifact-only); Gate C
remains NOT PASSED; Gate D remains NOT STARTED.

No console interaction, command-buffer submission, compute dispatch, PRX
activation, PKG installation, or console-state change was performed.


## New finding: public retail-title import maps expose candidate GNM NIDs

A deeper lookup of public PS4 title import-name maps found a concrete set of
NID-to-symbol mappings for the GNM driver. The mappings are present in both
the public `theorder1886-port` import-name database at revision
`4270135b99a32788b2fa4429fe85b76f38e2d5c9` and the
`HayatoG/tlg_pc` port import-name database at revision
`ee3327d7c1a1c107a16327d7c7a33c6c148b6dce`.

| Function | Candidate NID | Evidence |
|---|---|---|
| `sceGnmDispatchInitDefaultHardwareState` | `nF6bFRUBRAU` | Both title import maps |
| `sceGnmMapComputeQueue` | `29oKvKXzEZo` | Both title import maps |
| `sceGnmMapComputeQueueWithPriority` | `A+uGq+3KFtQ` | Both title import maps |
| `sceGnmUnmapComputeQueue` | `ArSg-TGinhk` | Both title import maps |
| `sceGnmDingDong` | `bX5IbRvECXk` | Both title import maps |
| `sceGnmSubmitDone` | `yvZ73uQUqrk` | Both title import maps |
| `sceGnmSubmitCommandBuffers` | `zwY0YV91TTI` | TLG port import map |

These are useful new identifiers for a **read-only target symbol census**:
if the target's loaded `libSceGnmDriver` can be inspected safely, a probe
can record module identity and resolve the candidate NIDs without invoking
the compute functions. This narrows the symbol-discovery task substantially.

Important limitations:

- These NIDs are derived from public retail-title import maps and public
  symbol-name databases, not from a Sony FW 14.02 SDK package or a captured
  FW 14.02 export table.
- The two port repositories may share upstream title/import metadata; their
  agreement is corroboration of the mapping data, not two independent
  firmware measurements.
- NID presence in a title import map does not prove that the target firmware
  exports the symbol, that the module is loaded in the same state, or that the
  function prototype/calling convention is correct.
- `sceGnmDispatchDirect` is present in a title import-name list but no
  corresponding NID mapping was established in the inspected mapping excerpt.
  Do not infer or guess its NID.
- No function was called, no symbol was resolved on the user's console, and
  no hardware probe was run in this session.

### Prepared read-only target census sequence

If/when the user's existing approved read-only PS4 evidence channel is
available, the next safe target-side step is limited to:

1. Read firmware/build identity and enumerate loaded kernel modules.
2. Identify the exact loaded module name for the normal or Neo GNM driver.
3. Record module handle, segment base/size and module identity using the
   existing read-only kernel module-info APIs.
4. Resolve the candidate NIDs above only if the supported resolver API can be
   used without loading a module or changing console state; record success or
   failure and resolved addresses.
5. Do not call any resolved GNM function, submit command buffers, map compute
   queues, allocate GPU resources, activate PRX, or install a package.
6. Redact console identifiers and network details from the evidence artifact.

This read-only census can establish symbol availability and module identity.
It still cannot prove GPU VA/residency, completion, cache visibility, or
compute correctness. Those remain separate Gate B requirements and require
an explicitly authorized isolated hardware validation.

### Updated disposition

The public-source ABI subtask now has concrete candidate NIDs for the
compute initializer and ACE queue/doorbell lifecycle, rather than names alone.
This is a meaningful narrowing of the remaining target probe. It is **not**
a firmware-14.02 ABI qualification and does not authorize a dispatch.

Gate B remains **BLOCKED** pending target-side read-only symbol evidence,
memory/residency and synchronization contracts, and a controlled PS4 Pro
compute result with deterministic full-buffer readback.


## Target census implementation constraints from an existing PS4 probe

The public `iHaiDeeZ/DolphinPS4` probe at revision
`879b3e57f3d3180bcb5f2234db3903688b28cd7d` provides a useful pattern for
module enumeration and symbol lookup:

- enumerate module handles with `sceKernelGetModuleList`;
- query each module with `sceKernelGetModuleInfo`;
- identify `libSceGnmDriver` by module name;
- resolve function names using `sceKernelDlsym(module_handle, name, &address)`.

The probe's own `LoadGnmDriver()` fallback calls
`sceKernelLoadStartModule` when the module is absent. That fallback is
**not** read-only and must not be copied into the NR census. The NR census
must stop and report `MODULE_NOT_PRESENT` if the expected module is not
already loaded. It must not load/unload modules, invoke resolved GNM
functions, allocate DirectMemory, or write files outside its explicitly
approved evidence destination.

The public probe resolves symbols by name rather than by passing NIDs to
`sceKernelDlsym`. Therefore the candidate NID table above is provenance for
the import identities, while runtime verification should use the supported
name-based resolver against the already-loaded module. The probe's graphics
submission and framebuffer DMA code is not relevant to the read-only census
and must not be copied.

The Last Guardian report identifies its analyzed executable as
`CUSA03627`, version `1.03`; it documents a shipping-title compute
initializer, queue map, doorbell and unmap call path. Its firmware version is
not established by the cited report, so this cannot qualify FW 14.02.

This narrows a safe target evidence operation to module census plus
name-resolution only. It does not establish target ABI until the user's
actual FW 14.02 console returns the module and symbol-resolution results.


## FW 14.02 module provenance check (2026-11-24)

A final public-source sweep looked specifically for a decrypted retail
`libSceGnmDriver.sprx` / `libSceGnmDriverForNeoMode.sprx` export table or
disassembly tied to PS4 firmware 14.02 (build `J02700521`).

### Result

- The detailed OpenGNM firmware reverse-engineering inventory explicitly
  identifies its GNM module dumps and export tables as **FW 9.00**. It lists
  both normal and Neo-mode modules, but those artifacts do not qualify FW
  14.02.
- Public FW 14.02 kernel-dumper projects document kernel offsets and kernel
  dump support. A kernel dump is not the userland GNM SPRX export table or
  disassembly needed to establish GNM NIDs, function entry points, or ABI.
- Public debugger projects advertise FW 14.02 process inspection, but their
  availability does not itself supply a captured GNM module export table or
  a target census from this NR console.
- The available public GNM sources still provide title-import NID candidates,
  OpenGNM signatures, and emulator HLE behavior. None is a retail FW 14.02
  module capture.

References:
- OpenGNM firmware RE inventory (explicitly FW 9.00):
  https://github.com/PS4-OpenGNM/opengnm/blob/main/OPENGNM_REWRITE_PLAN.md
- FW 14.02 kernel dumper project:
  https://github.com/iHaiDeeZ/ps4-kernel-dumper
- FW 14.02 debugger support project:
  https://github.com/Pharaoh2k/ps4debug-NG

### Decision

No public FW 14.02 GNM export table/disassembly was found in this sweep.
Do not relabel FW 9.00 findings, title-import metadata, or kernel-offset
support as FW 14.02 ABI proof.

The missing evidence must come from a lawful, target-specific source artifact
or an approved read-only runtime census. The existing NR FTP audit only
collects installed application files; it does not enumerate loaded modules.
Creating or activating a target-side probe is outside this source-only audit
and remains subject to the existing safety boundary: do not modify the
telemetry PRX, install a PKG, or interact with the console without the
separate validation/authorization sequence.

This confirms a hard external dependency for Gate B. It is not a reason to
guess signatures or bypass the hardware qualification requirements.


## Fresh cross-check: current public FW 14.02 support still does not supply GNM exports

A fresh public-source check on 2026-11-24 found additional FW 14.02 support
references, but none changes the ABI conclusion:

- The PS4 kernel-dumper fork explicitly supports kernel build
  `J02700521` / FW 14.02 and writes a kernel image. Its documented output is
  a kernel dump, not a decrypted `libSceGnmDriver` or
  `libSceGnmDriverForNeoMode` SPRX/export table:
  https://github.com/iHaiDeeZ/ps4-kernel-dumper
- ps4debug-NG lists FW 14.02 among supported firmware and describes kernel
  and libkernel validation against decrypted retail binaries. That is useful
  evidence that FW 14.02 inspection tooling exists, but its published
  support matrix is not a GNM driver export capture or proof of GNM ABI:
  https://github.com/Pharaoh2k/ps4debug-NG
- The current OpenGNM rewrite-plan documentation describes a detailed
  GNM-driver reverse-engineering inventory with export/NID tables, but
  explicitly identifies the analyzed driver dump as FW 9.00:
  https://github.com/PS4-OpenGNM/opengnm/blob/main/OPENGNM_REWRITE_PLAN.md

This cross-check distinguishes three artifact classes that must not be
conflated: kernel image/offset support, debugger capability, and userland
GNM driver export/disassembly. Only the third can directly establish the
requested provider exports and function entry points; none of the newly
checked public references supplies that artifact for FW 14.02.

### Engineering consequence

The candidate NIDs and module census plan remain useful preparation, but
there is no source-backed reason to change any function signature, select a
provider, or mark the FW 14.02 ABI sub-gate passed. The remaining dependency
is still a lawful FW 14.02 GNM module artifact or a separately authorized
passive census of already-loaded providers. No module loading, symbol
invocation, allocation, dispatch, PRX activation, PKG installation, or
console-state change is authorized or performed by this documentation work.


## Additional 14.02 debugger validation and current API-reference cross-check

The public ps4debug-NG support notes are more specific than a generic
"firmware supported" label: they state that 14.02 kernel symbols, patch
sites, and all three libkernel variants were checked against decrypted
retail 14.02 kernel/libkernel binaries, with comparisons to 14.00 and
13.52. This materially strengthens confidence in the availability of a
14.02 inspection toolchain. It still does not include a decrypted
`libSceGnmDriver.sprx` or `libSceGnmDriverForNeoMode.sprx` export table,
GNM import stub, or GNM function disassembly, so it cannot close the GNM
provider/ABI sub-gate:
https://github.com/Pharaoh2k/ps4debug-NG

The current public OpenGNM API reference now publishes typed declarations
for the compute initializer, dispatch commands, compute queue management,
and submit functions, and its backend page describes the Orbis forwarding
path. This is useful cross-project API documentation and strengthens the
candidate signature set. It remains OpenGNM-maintained documentation and
does not identify a retail FW 14.02 export capture, provider selection, or
target-observed argument semantics:
https://ps4-opengnm.github.io/opengnm/reference/driver/
https://ps4-opengnm.github.io/opengnm/backends/

The same current reference explicitly separates the generic backend
(no-op submit / PM4 capture for host testing) from the Orbis backend
(firmware forwarding). Consequently, passing generic host tests cannot
be interpreted as target submit/compute validation.

### Updated evidence classification

- **FW 14.02 kernel/libkernel inspection support:** corroborated against
  decrypted retail binaries by ps4debug-NG documentation.
- **FW 14.02 GNM provider/export/ABI:** still not evidenced.
- **Typed candidate GNM API signatures:** documented by OpenGNM, not
  firmware-qualified for 14.02.
- **Host generic packet/build tests:** software-only, not GPU execution.
- **PS4 Pro compute known-answer/readback:** not performed.

No target module census was performed, and no GNM function was called.


## 14.02-capable debugger is not a passive census endpoint

A closer inspection of the public ps4debug-NG repository clarifies an important distinction. Its README documents FW 14.02 support and says its kernel/libkernel offsets were checked against decrypted retail 14.02 binaries. It also advertises process maps, remote function calls, arbitrary process memory access, and in-target RPC. Its architecture, however, requires sending `ps4debug-ng.bin` to a payload loader; that package installs a companion kernel module and starts a userland command server. It is therefore an active debugger deployment, not an already-available passive inspection endpoint.

The README does not document a command that directly enumerates the GNM provider modules and resolves their symbols using only an existing stock/read-only interface. The advertised process maps expose virtual-memory regions/backing names, but that alone does not prove a safe module-handle + `sceKernelDlsym` census, nor does it qualify the GNM driver's ABI.

Reference: https://github.com/Pharaoh2k/ps4debug-NG (README sections “Supported firmwares”, “Process inspection and manipulation”, and “Architecture”).

Disposition for NR: do not deploy or inject this debugger as a workaround. Its installation would change console runtime state and is outside the passive census authorization. This closes another candidate-tool path: FW 14.02 kernel/libkernel inspection support exists publicly, but it is not a passive GNM census channel and provides no GNM export capture. No debugger payload was sent and no console operation occurred.
