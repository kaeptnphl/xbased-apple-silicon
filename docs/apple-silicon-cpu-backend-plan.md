# CPU backend decision plan for native Apple Silicon

## Status and scope

This is a proposed investigation and prototype plan, not an authorization to
implement or integrate a backend. The final CPU backend remains undecided.
The first proposed experiment uses standalone QEMU TCG with SoftMMU; FEX
remains an alternative with a closer fit to the existing CPU interface.

Target: an M1 MacBook Air with 8 GB RAM, native arm64 only, without Rosetta.
Preserve the SDL frontend where practical. Renderer selection, Xbox boot and
full emulator integration are outside the prototype scope. No performance,
game compatibility or boot claims follow from this plan.

## Evidence baseline

- Repository: `d492b5c9da26bc107a028fa63669f67ea3d267df`.
- FEX reference: `rfandango/FEX`,
  `52018bf46a8279b22bce738548ab3f40db467064`.
- QEMU reference: v9.2.0,
  `ae35f033b874c627d81d51070187fbf55f0bf1a7`.
- These are inspected references, not a selection of a current release for
  distribution. Recheck revisions and security maintenance before adoption.
- The repository does not currently pin or provide `external/fex`;
  `native/cmake/fex.cmake` expects a separately supplied fork checkout.
- The five isolated Darwin tests passed during the investigation. They do
  not validate CPU execution or independent 4 KiB guest-page protections on
  a 16 KiB host.
- Neither external CPU backend was built or executed during the investigation.

The current platform gate is in `native/CMakeLists.txt:29-60`: Apple arm64 is
explicitly rejected rather than sent down the Android/FEX path. The existing
`native/cpu/cpu_x86.cpp` emits x86-64 code and is not an ARM64 backend.

## 1. Establish the backend contract and prototype boundary

Specify the following contracts before implementing an adapter:

- Eight x86-32 GPRs, EIP, architectural EFLAGS including virtual IF, flat
  segments and FS/KPCR, x87/MMX/SSE and MXCSR.
- System-operation exits at the instruction EIP with its length; INT 0x3F
  ordinal handling; pre-instruction hooks and exactly-once hook bypass.
- One host vCPU thread multiplexing guest contexts; cross-thread exit requests
  distinct from IF/IRQL-controlled guest interrupt delivery.
- Precise guest fault state, restart from a modified frame, translation
  invalidation, guest-PC mapping, and a signal-safe sampling strategy.
- Defined atomic, unaligned, cross-page, timing and pause behavior.
- Explicit conversion of the FEX-shaped `NcCpuState` savestate representation.

Repository evidence at the baseline:

| Boundary | Relevant source |
|---|---|
| Public CPU interface | `native/cpu/cpu.h:18-204` |
| FEX setup, state and fault recovery | `native/cpu/cpu.cpp:43-72,191-213,378-402,514-613` |
| Existing non-FEX savestate conversion | `native/cpu/cpu_x86.cpp:1467-1520` |
| Core entry and fault routing | `native/core/core.c:234-263,280-292,397-495` |
| VM, aliases and DMA notifications | `native/core/vm.h:49-114` |
| Host-instruction-based MMIO handling | `native/core/mmio.c:1-7,73-155,479-540` |
| Kernel system operations and scheduling | `native/kernel/kernel.c:1653-1829`, `native/kernel/ps.c:397-406,532-536` |
| SEH and savestate callers | `native/kernel/seh.c:103-131`, `native/kernel/kstate.c:141-190` |
| HLE hooks and write tracking | `native/hle/glue.c:297-311`, `native/hle/dirty.c:444-510` |

FEX's guest-memory-base, system-operation, hook and cycle-counter APIs are
hakuX fork extensions, not assumed upstream interfaces. Its existing fault
recovery depends on concrete frames, SRA register mappings, dispatcher
addresses and the call/return shadow stack.

TCG alone emits host code from IR. Translating x86 also requires the decoder,
CPU state, helpers, translation-block management and execution loop. QEMU
system emulation is not a ready-made HLE CPU library. Linux-user/BSD-user
emulate OS processes and syscalls; the inspected build has no Darwin-user
path. Do not assume a stable QEMU embedding API or a small extractable
`libtcg`.

**Gate:** document a callable run/exit interface and ownership model. Running
an ordinary QEMU machine is not proof of embeddability.

## 2. Resolve the proposed dependency and license closure

Trace the exact sources, headers, generated files, linked objects and
dependencies needed by the chosen configuration. The repository declares
GPL-3.0-or-later with component-specific exceptions in `LICENSE`; `COPYING`
contains GPLv3. This does not replace a file-level compatibility review.

For QEMU, include the x86 decoder and helpers, ARM64 emitter, execution loop,
TB management, SoftMMU, QOM, utility/thread/RCU code, SoftFloat and the selected
GLib dependency chain. Do not infer the whole license from a permissive TCG
header. The inspected ARM64 emitter and atomic helpers include GPLv2-or-later
code, while the x86 translator and execution/TLB code include LGPLv2.1-or-later
code. SoftFloat has additional provenance and license conditions.

The inspected `include/exec/memory.h` names GPL version 2 without an
"or later" grant. `system/memory.c` distinguishes GPLv2 legacy content from
post-2012-01-13 GPLv2-or-later contributions. These are concrete unresolved
compatibility concerns on the standard SoftMMU/MemoryRegion path when
combining it with xbased's GPL-3.0-or-later code. Require rights/history
clarification or a demonstrably compatible alternative closure before
embedding. Static versus dynamic linkage is not a resolution.

For FEX, verify FEXCore, CodeEmitter, FEXHeaderUtils, fmt, xxHash,
unordered_dense, range-v3, rpmalloc, Cephes and SoftFloat-3e, including
individual exceptions, provenance and generated material. The inspected
FEXCore/CodeEmitter MIT texts and permissive dependency notices are not a
completed closure review; Cephes provenance needs particular attention.

**Gate:** no combined integration or distribution until the selected closure
is cleared. This plan is not legal advice or legal approval.

## 3. Prove a standalone QEMU/SoftMMU run/exit prototype

This proposed experiment can be investigated independently of the
combined-project license review only as a separate QEMU-based program with
self-written synthetic guest code, without incorporating xbased source or
headers. Its own licenses still apply.

Use native arm64 host code, one x86-32 vCPU, synthetic RAM and no Xbox boot,
guest OS, PC device ecosystem, HVF, SDL or renderer. Start with 64 MiB RAM and
an explicitly bounded TCG cache, initially 32 MiB. Record resident memory and
cache usage separately from reserved address space.

Implement a narrow revision-specific internal adapter for run, typed exits,
state inspection/update, resume and interrupt requests. Normal system-mode
QEMU exception delivery is not automatically an HLE exit. Retain the required
QEMU lifecycle, TLS, RCU, locking and atomic fallback mechanisms; do not
silently stub them out to obtain a link.

Validate Darwin MAP_JIT allocation, write/execute transitions on every code
generation and patch path, instruction-cache invalidation, Apple ARM64 helper
ABI and fault handling. Test the chosen signing/runtime configuration.
Hardened Runtime requires consideration of `allow-jit`; it is not a blanket
entitlement requirement for every non-hardened executable. Do not blindly mix
the JIT callback allowlist mode with `pthread_jit_write_protect_np`.

Example synthetic NASM payload, with stack, FS and mappings set by the harness:

```asm
BITS 32
ORG 0x10000
mov eax, 0x7fffffff
add eax, 1
pushfd
pop edx
mov [0x20000], eax
mov ebx, [fs:0x1c]
faulting:
mov ecx, [0x40000]
mov [0xff000000], ecx
lock inc dword [0x22001]
db 0xcd, 0x3f
dw 0x1234
hlt
```

`0x40000` starts unmapped; `0xff000000` is synthetic MMIO. Add separate
payloads for CLI/STI, a guest loop, x87/SSE, code modification and page edges.

| Area | Acceptance criterion |
|---|---|
| Registers and flags | EAX is `0x80000000`; `(EDX & 0x8d5) == 0x894`; FS returns its sentinel. Defined flags, all guest registers and unaffected host ABI/FP state survive exits and resume. |
| Memory and permissions | Four adjacent 4 KiB guest pages associated with one 16 KiB host span can have independent permissions and noncontiguous backings. Remapping or granting access to one does not change the others. Aliases share data and physical dirty/noaccess state. |
| Fault and resume | The missing-page read reports the exact faulting EIP, address, direction and architectural state. Mapping the page and resuming returns the sentinel without replaying prior side effects. |
| MMIO | Address, width, value and direction match the expected access; each device side effect occurs exactly once across exits and resume. |
| Atomics and invalidation | Aligned, unaligned and cross-page LOCK operations and faults match explicit result/side-effect oracles. Code modification plus invalidation executes new code rather than a stale TB. Preserve QEMU's required serialized atomic fallback. |
| Interrupts and hooks | 10,000 sequentially acknowledged cross-thread requests cause matching block-boundary exits without lost or duplicate callbacks. IF=0 blocks guest IRQ delivery, not host exit. Hook bypass executes the instruction exactly once. Use a watchdog and record latency distributions. |
| Context and state | Two distinct GPR/flags/FS/x87/SSE contexts survive at least 1,000 switches. Export/import restores architectural state without serializing backend pointers or caches. Existing xbased savestate compatibility needs a separate later proof. |

**Gate:** the callable run/exit experiment must pass these correctness checks
before proposing integration. Record missing coverage and failures explicitly.

## 4. Decide memory ownership and integration architecture

SoftMMU can decouple guest permissions from host page granularity for translated
accesses. It does not repair direct host pointers and protection assumptions in
NcVm, HLE, DMA or the kernel.

Specify one authoritative mapping model: retain NcVm as the authority with an
explicit bridge, or propose its replacement explicitly. Do not maintain
uncoordinated QEMU and NcVm page tables.

Cover virtual-to-physical mappings, aliases, host physical RAM views, 4 KiB
dirty/noaccess state, read/write notifications, DMA fences, guest paging,
TLB invalidation, self-modifying code and MMIO callbacks. Distinguish guest
exceptions from unexpected host faults. Rounding host protection operations
to 16 KiB is not a solution for independent 4 KiB guest rights.

Provide a separate lock-free/signal-safe PC sampling design: QEMU's ordinary
TB lookup uses a mutex and cannot replace the existing signal sampler directly.
Avoid depending on decoding TCG-generated ARM64 instructions with the existing
FEX-oriented MMIO fault decoder.

**Gate:** no full CPU integration without a consistent mapping, fault and
synchronization design and a cleared license closure.

## 5. Measure on the target M1 and select the backend

Measure cold/warm execution, translation time, callback and fault costs,
interrupt latency, RSS, code-cache usage and sustained thermal behavior on
the M1 MacBook Air with 8 GB. Record host OS, compiler, revision, runtime/signing
configuration and test inputs. Do not infer Xbox FPS from synthetic throughput.

Compare the resulting integration and maintenance burden with a scoped FEX
Darwin port: JIT allocation/patching, Darwin signal-context recovery,
host-feature detection, helper ABI and the unresolved direct-mapping page model.
If QEMU licensing or integration gates fail, revisit FEX rather than treating
the prototype choice as a permanent backend commitment.

**Gate:** make the final backend decision only from correctness evidence,
license closure, memory architecture and M1 measurements. Xbox boot and
renderer integration require separate authorization and plans.

## Sources

All external source links refer to the inspected revisions, not moving branches.
Apple documentation describes platform requirements and must be rechecked for
the selected deployment target.

- [Repository baseline](https://github.com/kaeptnphl/xbased-apple-silicon/tree/d492b5c9da26bc107a028fa63669f67ea3d267df)
- [FEX fork APIs](https://github.com/rfandango/FEX/blob/52018bf46a8279b22bce738548ab3f40db467064/FEXCore/include/FEXCore/Core/Context.h#L177-L227)
- [FEX platform gate](https://github.com/rfandango/FEX/blob/52018bf46a8279b22bce738548ab3f40db467064/CMakeLists.txt#L55-L68)
- [FEX executable allocation](https://github.com/rfandango/FEX/blob/52018bf46a8279b22bce738548ab3f40db467064/FEXCore/include/FEXCore/Utils/AllocatorHooks.h#L110-L150)
- [FEX page-size assumptions](https://github.com/rfandango/FEX/blob/52018bf46a8279b22bce738548ab3f40db467064/FEXCore/include/FEXCore/Utils/TypeDefines.h)
- [FEX source and dependency build list](https://github.com/rfandango/FEX/blob/52018bf46a8279b22bce738548ab3f40db467064/FEXCore/Source/CMakeLists.txt)
- [QEMU TCG source composition](https://github.com/qemu/qemu/blob/ae35f033b874c627d81d51070187fbf55f0bf1a7/accel/tcg/meson.build)
- [QEMU system exception handling](https://github.com/qemu/qemu/blob/ae35f033b874c627d81d51070187fbf55f0bf1a7/accel/tcg/cpu-exec.c#L711-L775)
- [QEMU Darwin JIT allocation](https://github.com/qemu/qemu/blob/ae35f033b874c627d81d51070187fbf55f0bf1a7/tcg/region.c#L611-L721)
- [QEMU JIT write/execute transitions](https://github.com/qemu/qemu/blob/ae35f033b874c627d81d51070187fbf55f0bf1a7/include/qemu/osdep.h#L800-L816)
- [QEMU SoftMMU atomic fallback](https://github.com/qemu/qemu/blob/ae35f033b874c627d81d51070187fbf55f0bf1a7/accel/tcg/cputlb.c#L1793-L1900)
- [QEMU x86 page size](https://github.com/qemu/qemu/blob/ae35f033b874c627d81d51070187fbf55f0bf1a7/target/i386/cpu-param.h)
- [QEMU TB lookup locking](https://github.com/qemu/qemu/blob/ae35f033b874c627d81d51070187fbf55f0bf1a7/tcg/region.c#L237-L255)
- [QEMU license overview](https://github.com/qemu/qemu/blob/ae35f033b874c627d81d51070187fbf55f0bf1a7/LICENSE)
- [QEMU memory header license](https://github.com/qemu/qemu/blob/ae35f033b874c627d81d51070187fbf55f0bf1a7/include/exec/memory.h#L1-L11)
- [QEMU memory implementation license distinction](https://github.com/qemu/qemu/blob/ae35f033b874c627d81d51070187fbf55f0bf1a7/system/memory.c#L1-L14)
- [Apple JIT porting requirements](https://developer.apple.com/documentation/apple-silicon/porting-just-in-time-compilers-to-apple-silicon)
- [Apple allow-jit entitlement](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.cs.allow-jit)
- [Apple ARM64 ABI](https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms)
