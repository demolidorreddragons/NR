# PS4 mapping stability and x86 code-publication audit

**Date:** 2026-11-22  
**Repository:** `demolidorreddragons/NR`  
**Branch:** `feature/gcn8-numeric-kernels`  
**Scope:** public Orbis declarations, public PS4 reverse-engineering notes, AMD64 architectural requirements  
**Disposition:** no PS4 mapping lease or NR-safe cross-modifying-code protocol established; platform adapter remains blocked

## Executive decision

This audit closes two important misconceptions but does not close either implementation gate:

1. `sceKernelVirtualQuery` and `sceKernelQueryMemoryProtection` are metadata queries, not mapping leases. Their successful return does not establish that the mapping cannot be unmapped, remapped, or reprotected immediately after the query. The exact OpenOrbis v0.5.4 declarations expose no handle, reference, lock, pin, or release operation associated with these queries.
2. AMD64 cache coherence for ordinary self-modifying code does **not** mean that an arbitrary thread can safely patch another thread's active instruction stream and then simply resume it. AMD's cross-modifying-code guidance requires a producer/consumer synchronization protocol; after the producer changes the bytes and signals the consumer, the consumer must execute a serializing instruction such as CPUID or MFENCE before proceeding to the modified code. A global suspend/resume wrapper has not been shown to provide that consumer-side serialization.

Consequently:
- Do not treat query-then-read as a stable source window.
- Do not add a guessed `__builtin___clear_cache`, `mfence`, `cpuid`, `clflush`, or cache API to the PRX as a substitute for a platform contract.
- Do not assume `scePthreadSuspendAll()/scePthreadResumeAll()` alone provides a complete cross-modifying-code protocol.
- Keep the transaction core host-only and the live hook unchanged.

## 1. Evidence: public Orbis declarations

The exact OpenOrbis v0.5.4 `include/orbis/libkernel.h` declares:

```c
int32_t sceKernelQueryMemoryProtection(void *, void **, void **, int32_t *);
int32_t sceKernelVirtualQuery(const void *, int32_t, OrbisKernelVirtualQueryInfo *, size_t);
int32_t sceKernelMprotect(const void *, size_t, int);
int32_t sceKernelMunmap(void *, size_t);
```

These signatures return query data or a status code, but do not expose a mapping object, lease token, pin count, lock handle, or paired acquire/release API. The declarations contain no contract comments promising lifetime stability after the call.

The same pinned header is already identified in the preceding thread-suspension audit; its blob SHA-1 is `33be396ac17076541acadafca04c8683c2383a5f`.

**Interpretation:** this is sufficient to identify query/protection APIs and their C signatures. It is not sufficient to claim mapping lifetime or race-free access.

## 2. Independent PS4 reverse-engineering notes: useful boundary, not retail-kernel proof

The Paleblood/Byrgenwerth runtime-call reference describes the observed purpose and argument use of `sceKernelQueryMemoryProtection` and `sceKernelVirtualQuery`, and separately records the behavior implemented by that project's clean-room runtime. It explicitly warns that public standards and manuals do not prove every detail of the PS4 implementation.

The notes report:
- `sceKernelQueryMemoryProtection` as returning the protection for the mapping containing an address, with optional start/end outputs;
- `sceKernelVirtualQuery` as returning mapping metadata in `OrbisKernelVirtualQueryInfo`;
- their own emulator runtime answers from its internal mapping records;
- their runtime's mapping records are not a claim about a retail Orbis mapping lease.

This is useful corroboration that these APIs are used as queries, but it is not a retail 14.02 kernel implementation or a mapping-lifetime guarantee. The same reference notes that even its own implementation has explicit limitations around which mappings it tracks. Do not import emulator behavior into NR as a PS4 fact.

## 3. Mapping stability disposition

A query can establish a point-in-time observation. It cannot, by itself, make the following sequence atomic:

1. query address and mapping bounds/protection;
2. validate the proposed instruction span;
3. read bytes for decoding;
4. allocate/generate the trampoline;
5. change protection and write/verify the target.

Between any two steps, another actor with sufficient authority could alter the mapping or permissions. Thread suspension might reduce user-thread races if its contract were proven, but it would not establish that kernel/loader operations, process management, or other privileged actors cannot change the mapping.

The current target is code owned by a title/loader, not a mapping that NR itself created and exclusively owns. No public Orbis API contract was found that pins such an arbitrary existing executable mapping for the entire transaction.

**Gate result: BLOCKED.** A PS4 adapter must not decode or patch an arbitrary target on the strength of a query alone. A future design would need either:
- a documented/validated mapping-lifetime mechanism that applies to the target mapping; or
- a stronger ownership model in which NR controls the target mapping and can prove its lifetime and protection transitions; or
- a title/loader cooperation contract that prevents concurrent unmap/reprotect for the whole operation.

If none exists for the selected target, the NR-owned detour path is unsupported and must remain disabled.

## 4. AMD64 cross-modifying-code requirements

Source: AMD64 Architecture Programmer's Manual, Volume 2: System Programming, revision 3.38 (November 2021), §7.6.1, pages 201–202.

The manual distinguishes:
- **Self-modifying code:** a thread writes its own pending instruction stream. AMD64 hardware maintains instruction/data cache coherence and detects certain in-flight instructions in that same physical instruction stream.
- **Cross-modifying code:** one thread writes the active instruction stream of another thread that may subsequently execute it.

For cross-modifying code, AMD describes:
- an asynchronous method with strict constraints, including a single naturally aligned quadword store and code-layout/timing assumptions; or
- a synchronous producer/consumer protocol. The consumer waits for a signal after the producer's writes, then executes a serializing instruction such as CPUID or MFENCE before entering the modified code. The manual specifically says a locked operation alone is insufficient for this consumer-side instruction-fetch synchronization.

The existing NR patch is a 14-byte absolute jump written as a multi-byte copy. It is not a single naturally aligned quadword store and does not satisfy the asynchronous modification conditions. Therefore only a proven synchronous protocol could qualify.

### Consequence for suspend/resume

Even if a future firmware investigation proves that `scePthreadSuspendAll()` stops all relevant user threads and `scePthreadResumeAll()` resumes them, that alone does not establish the AMD producer/consumer protocol. The current evidence does not show that each resumed target thread executes the required serializing instruction after resume and before it can fetch the patched range.

A thread may be suspended with an instruction pointer near or within the target region, or with speculative fetch state relevant to the old stream. A generic resume operation has no documented per-thread handoff callback in the public declaration. The PRX cannot simply assume that returning from the resume API causes every resumed thread to execute CPUID/MFENCE before continuing.

**Gate result: BLOCKED.** A candidate rendezvous must prove both:
1. no thread can fetch/execute the target while bytes are being modified; and
2. each consumer thread is released through a documented/validated synchronization sequence that prevents stale instruction fetch before it can reach the modified range.

A process-wide stop-the-world call with no documented consumer-side serialization is not enough evidence.

### What this does and does not say about cache APIs

AMD64's architectural cache-coherence rules are relevant to the CPU instruction stream. They do not prove:
- Sony's firmware wrapper behavior;
- that all PS4 CPU cores participate exactly as assumed by a userland PRX;
- the behavior of executable aliases or nonstandard mappings;
- that an OS-level code-publication API is unnecessary for every Orbis memory type;
- GPU cache coherency for GNM resources.

Therefore no cache-maintenance call or barrier sequence is being added to NR based on architecture text alone. Any eventual protocol must be consistent with AMD64 and also grounded in Orbis/firmware and mapping-type evidence.

## 5. Updated gate matrix

| Gate | Current evidence | Decision |
|---|---|---|
| Mapping metadata query names/signatures | OpenOrbis v0.5.4 header | Known declarations only |
| Mapping lease / pin for arbitrary title text | No public contract/API found | **BLOCKED** |
| `scePthreadSuspendAll` exact 14.02 semantics | Header declaration only; no documented postconditions | **BLOCKED** |
| Install-time exclusion during 14-byte patch | No proven rendezvous | **BLOCKED** |
| Consumer-side cross-modifying-code serialization | AMD64 requires synchronous consumer handoff; Orbis resume behavior unknown | **BLOCKED** |
| Removal-time quiescence | No proof all callbacks and generated-code execution have drained | **BLOCKED** |
| CPU instruction-cache coherence principles | AMD64 architecture manual | Architecture guidance only |
| Orbis code-publication contract for PRX mappings | No authoritative 14.02 contract found | **BLOCKED** |
| GNM/GPU cache coherency | Separate GNM resource/fence contract not established here | **BLOCKED** |

## 6. Required next research

1. Find firmware 14.02 libkernel export metadata and implementation evidence for `scePthreadSuspendAll` / `scePthreadResumeAll`; identify NIDs, module/library identity, ABI, thread scope, wait semantics, nesting and concurrency behavior.
2. Determine whether Orbis provides a documented per-thread resume rendezvous, suspend callback, signal/exception handoff, or other cooperative protocol that can guarantee the AMD consumer-side serializing step.
3. Determine whether the target title's loader or a title-specific adapter can guarantee text mapping lifetime for the entire transaction. Do not infer this from `sceKernelVirtualQuery`.
4. Identify authoritative Orbis executable mapping/publication semantics, including any required kernel cache operation and restrictions on aliases.
5. Keep removal as a distinct proof obligation: no new hook entry plus drain of every in-flight target/hook/stub/trampoline execution before reclaiming executable memory.

If the evidence cannot establish these properties, close the research item as unsupported rather than implementing an unsafe approximation.

## 7. Change boundary

This audit changes documentation only. No executable source, PRX Makefile, workflow, package, plugin registration, or console state is changed. No firmware API was called and no PS4 hardware test was performed.

## Sources

- OpenOrbis v0.5.4 libkernel declarations: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/include/orbis/libkernel.h
- Paleblood/Byrgenwerth runtime-call reference, with explicit distinction between PS4 evidence and clean-room runtime behavior: https://yharnam-hunters.github.io/byrgenwerth-site/runtime-reference/
- AMD64 Architecture Programmer's Manual, Volume 2, revision 3.38, §7.6.1 (Self-Modifying and Cross-Modifying Code): https://www.scs.stanford.edu/~zyedidia/docs/x86/amd-manual.pdf
