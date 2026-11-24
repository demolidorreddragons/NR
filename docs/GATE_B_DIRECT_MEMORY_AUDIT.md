# Gate B follow-up: pinned OpenGNM direct-memory contract

Date: 2026-11-24  
Pinned source: PS4-OpenGNM/opengnm@4b295ca54c82c83acf308d1c646a2dfa9ae57350

## Findings

### 1. DirectMemory helper has two distinct address values

In include/gnm_helpers.h, GnmDirectMemory stores both:

- mapped: the CPU virtual pointer returned by sceKernelMapDirectMemory;
- directmemory: the offset returned by sceKernelAllocateDirectMemory.

In src/helpers.c, sceGnmDirectMemoryAllocate rounds size to the requested alignment, allocates using sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), size, alignment, memorytype, &directmemory), maps that offset using sceKernelMapDirectMemory(..., protection, 0, directmemory, alignment), and stores both results separately. Release unmaps the CPU mapping and releases the direct-memory offset.

This proves the helper's bookkeeping and kernel API call sequence at source level. It does not prove that the returned offset is a GNM GPU virtual address, establish the address translation expected by every GNM packet/descriptor, or establish residency/lifetime requirements for compute resources on retail firmware 14.02.

### 2. The helper is not yet a compute-resource allocator

The pinned production src/*.c sources only consume GnmDirectMemory.directmemory inside the helper itself (store and release). The helper's sole production allocation call site is VideoOut buffer allocation in src/helpers.c. There is no source path connecting this field to a compute shader address, descriptor table, command-buffer GPU address, output buffer, or completion label.

Therefore the existence of sceGnmDirectMemoryAllocate is not sufficient to claim that OpenGNM has a working general-purpose compute allocation/address contract.

### 3. Existing hardware smoke is a graphics submit/EOP test

Pinned tests/hardware_smoke.c independently calls the kernel direct-memory allocator and mapper with WC Garlic memory and CPU/GPU read/write protection. It places the DCB and a 64-bit EOP label in the same mapped allocation, passes the mapped CPU pointer (cast through uintptr_t) to the EOP builder, submits through sceGnmSubmitCommandBuffers, calls sceGnmSubmitDone, and polls the label.

The command stream initializes default graphics state and emits a zero-count draw. The smoke does not bind a compute shader, dispatch workgroups, write a compute output buffer, or compare compute output against a CPU reference. Its EOP polling loop is capped at 100,000,000 iterations, not a wall-clock deadline with a sleep/yield and recovery policy. The source also enters an infinite status-display hold after reporting its result.

OpenGNM's packaging/rewrite documentation reports a prior graphics hardware smoke result (green status screen and EOP label) but does not identify a firmware-14.02 compute test. Treat it only as graphics-path evidence; it cannot close the compute ABI, resource, completion, or readback requirements.

### 4. Address-space and coherency conclusions

Keep these concepts separate in all future work:

1. CPU virtual pointer returned by sceKernelMapDirectMemory;
2. direct-memory offset returned by sceKernelAllocateDirectMemory;
3. GPU address actually consumed by shader registers, descriptors, PM4 packets, or firmware submit;
4. firmware-managed residency/ownership and lifetime;
5. CPU/GPU cache visibility in each direction.

The pinned helper and graphics smoke show how OpenGNM/application code currently handles (1) and retains (2). They do not establish (3)-(5) for a compute dispatch on firmware 14.02. Do not substitute the direct-memory offset for a GPU address, or assume the CPU mapping is a valid GPU address in every command context, without firmware-qualified evidence.

## Automated source regression

tests/test_opengnm_direct_memory_contract.py checks the exact pinned helper bookkeeping, release path, absence of production consumers for the direct-memory offset outside the helper, and the graphics-only nature of the existing smoke. It intentionally does not assert that this source is hardware-correct.

## Gate B disposition

This audit closes a source-inspection subtask and removes an unsupported assumption: the OpenGNM helper's directmemory field is not wired into its compute resource path. Gate B remains BLOCKED, not passed, until the following are independently established for retail PS4 Pro firmware 14.02:

- typed ABI/export evidence for the exact compute bind/dispatch and submission path;
- GPU address translation and allocation/residency/lifetime for shader code, command buffers, descriptors, output, and completion storage;
- compute-engine ordering and a bounded completion/failure-recovery primitive;
- GPU-to-CPU visibility/readback and full-buffer deterministic comparison.

No console interaction, compute dispatch, PRX activation, PKG installation, or console-state change was performed. Gate A remains PASS (artifact-only); Gate C remains NOT PASSED; Gate D remains NOT STARTED.
