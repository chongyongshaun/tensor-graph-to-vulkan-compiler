# TGVC MVP scope and support matrix

**Status:** frozen for the MVP — matrix `mvp-1`, machine-readable table [tgvc-operators-v1.json](../schemas/tgvc-operators-v1.json).
**Frozen by:** TF-002 (milestone M0). **Change control:** [Changing the matrix](#17-changing-the-matrix).

This document is the authority for *what the MVP accepts*. It fixes the operator set, the shape
and data-type policies, the target and runtime assumptions, the rejection taxonomy and the
non-goals. It is consistent with the problem statement and research questions in
[PROPOSAL.md](PROPOSAL.md) and with the architecture baseline (TF-003).

Per-operator semantics and their negative tests are owned by the dialect work (TF-013, TF-014).
Shape inference and verifier behaviour are owned by TF-015, the canonical JSON frontend by TF-016,
the restricted ONNX frontend by TF-026 and the diagnostic code catalog by TF-028.

## 1. Core operator set

The MVP compiles exactly these seven operations. Each row is frozen as data in
`schemas/tgvc-operators-v1.json`; the guard suite [scope_matrix_test.cpp](../tests/scope/scope_matrix_test.cpp)
fails if this table and that file disagree.

<!-- operator-matrix:start -->
| Operator | Category | Arity | Shape rule | Rank | Attributes | Rejected when |
|---|---|---|---|---|---|---|
| `Constant` | `leaf` | 0→1 | `shape_from_attribute` | `1..4` | `value`, `shape` | shape attribute missing or not static-positive; literal element count differs from the shape product; literal not float32; rank outside 1..4; unexpected attribute |
| `Add` | `elementwise_binary` | 2→1 | `identical_shapes_only` | `1..4` | — | operand shapes differ in any dimension, including shapes that would need broadcasting; operand not float32; rank outside 1..4; unexpected attribute |
| `Mul` | `elementwise_binary` | 2→1 | `identical_shapes_only` | `1..4` | — | as `Add` |
| `ReLU` | `elementwise_unary` | 1→1 | `same_shape_as_input` | `1..4` | — | operand not float32; rank outside 1..4; unexpected attribute |
| `MatMul` | `contraction` | 2→1 | `inner_dimensions_equal` | `2` | — | either operand is not rank 2; inner dimensions differ; operand not float32; unexpected attribute |
| `Reshape` | `structural` | 1→1 | `element_count_preserved` | `1..4` | `shape` | result element count differs from input; more than one inferred dimension; a non-sentinel dimension is not static-positive; result rank outside 1..4; unexpected attribute |
| `Transpose` | `structural` | 1→1 | `complete_unique_permutation` | `1..4` | `perm` | permutation length differs from rank; a dimension index repeats; an index falls outside 0..rank-1; rank outside 1..4; unexpected attribute |
<!-- operator-matrix:end -->

## 2. Elementwise broadcasting policy

Broadcasting is **not** part of the MVP. `Add` and `Mul` require operands with identical shapes in
every dimension, so elementwise fusion and dispatch planning stay trivially legal. A graph that
needs a broadcast is rejected rather than silently expanded; broadcast support is a stretch
candidate (see [Guaranteed versus cuttable](#15-guaranteed-versus-cuttable)).

## 3. Shape and rank policy

| Rule | Frozen value |
|---|---|
| Shape kind | Static ranked tensors only; every dimension is a positive integer literal |
| Rank bounds | 1 to 4 inclusive; rank-0 scalars and rank 5 or higher are rejected |
| Dynamic dimensions | Rejected: no symbolic names, no runtime dimensions, no `null` |
| Zero-sized tensors | Rejected: every dimension is strictly positive |
| Per-tensor size | At most 16 777 216 elements (2^24, 64 MiB as float32) per tensor |
| Graph inputs and outputs | Declared with their static shapes; no shape inference across a dynamic boundary |

## 4. Data type policy

`float32` only, with no implicit promotion and no mixed-type arithmetic. `float16`, `bfloat16`,
`float64`, all integer widths, `bool` and quantized or blocked encodings are rejected before
lowering. A graph that mixes element types is rejected as a malformed graph rather than widened.

## 5. Numeric behaviour

- IEEE-754 binary32 arithmetic; no fast-math assumptions and no flush-to-zero assumption.
- NaN and infinite inputs are permitted values: the compiler preserves IEEE semantics rather than
  assuming finite inputs.
- Compiler passes must be numerically neutral: a transformation that changes results is a bug, not
  a tuning choice.
- The absolute and relative tolerance policy for comparisons is **not** frozen here. The policy is
  documented with the NumPy differential oracles (TF-025) and finalised with the correctness matrix
  (TF-072).

## 6. Frontends in scope

| Frontend | Tier | Owner | Notes |
|---|---|---|---|
| Canonical TGVC JSON | MUST | TF-016 | The guaranteed path; validated against `schemas/tgvc-graph-v1.json` |
| Restricted ONNX subset | SHOULD, cuttable | TF-026 | Normalises into the canonical path; never bypasses canonical validation |

Both frontends validate against this matrix. Neither may add an operation, a data type or a shape
class that is absent from the table above.

## 7. Compiler, IR and optimization scope

In scope: a custom TGVC MLIR dialect with verified types and attributes; a single legal pass
pipeline; constant folding, canonicalization, dead-code and common-subexpression elimination;
elementwise fusion; and MatMul with bias and ReLU fusion.

Out of scope for the MVP: autotuning, polyhedral or loop-nest optimization, layout-specific
intrinsics, arbitrary graph rewriting, and any transformation whose legality cannot be checked
statically.

## 8. Memory planning scope

In scope: static liveness analysis, arena planning with reusable offsets, a verified execution
plan, and legal aliasing between non-overlapping lifetimes.

Out of scope: allocation during execution, on-device allocator behaviour, spilling, and memory
plans that depend on data values.

## 9. Target, device and runtime scope

| Aspect | Frozen decision |
|---|---|
| Target | Vulkan compute through SPIR-V |
| Devices | One compute-capable device; the runtime does not enumerate or split work across devices |
| Queues | One compute queue |
| Dispatch | A deterministic, sequential dispatch schedule |
| Excluded | Graphics and ray-tracing pipelines, vendor or tensor-core extensions, multi-device execution, asynchronous overlap |
| Scheduling inputs | A portable, public target profile only; no proprietary hardware detail |

The concrete Vulkan API version, SPIR-V version and ABI are recorded in the architecture decision
records (TF-004) and must remain consistent with this table.

## 10. Tooling and observability

In scope: a CLI that compiles, runs, inspects and benchmarks a model; textual IR dumps;
optimization remarks; schedule traces; memory timelines; and machine-readable result files including
hardware, driver, compiler and workload metadata.

Out of scope: interactive profiling GUIs, vendor-specific capture tools, and any diagnostic that
depends on non-public hardware information.

## 11. Explicit non-goals

- Dynamic shapes, symbolic dimensions and runtime-determined shapes.
- Control flow: loops, conditionals, recursion.
- Training, gradients and automatic differentiation.
- Full ONNX compatibility; ONNX support is a restricted, cuttable subset.
- Graphics rendering, presentation and game-engine integration.
- Production driver or kernel integration.
- Multi-device execution and asynchronous queue overlap.
- Quantized, blocked or mixed-precision models.
- Cycle-accurate simulation of any hardware.
- Any representation of real Arm NGP or other proprietary accelerator details, including internal
  code, data, formats, target values, names, diagrams or screenshots.

## 12. Rejection taxonomy

Every unsupported model must fail with one distinguishable category, so a user can fix the model
without reading compiler source. The human-facing code catalog and messages are TF-028; this table
freezes the categories themselves.

| Category | Example input | First enforced by |
|---|---|---|
| `unsupported_operator` | a `Conv` node | TF-016 importer |
| `unsupported_dtype` | a `float16` tensor | TF-016 schema validation, TF-015 shape inference |
| `unsupported_rank` | a rank-5 tensor | TF-015 shape inference |
| `dynamic_dimension` | a dimension named `N` or left `null` | TF-016 importer, TF-026 ONNX filter |
| `unsupported_attribute` | an `Add` node carrying `axis` | TF-014 operator verifiers |
| `malformed_graph` | duplicate tensor name, missing input, elementwise shape mismatch, non-positive dimension | TF-016 schema validation, TF-015 verifiers |

No unsupported input may reach code generation; rejection happens in the frontend or the verifier
layer, never as a runtime failure on the device.

## 13. Machine-readable capability table

`schemas/tgvc-operators-v1.json` carries the same decisions as data so frontends, verifiers and
tests share one source of truth. It contains:

| Field | Meaning |
|---|---|
| `schema_version`, `matrix_version`, `status`, `frozen_by` | Identity and freeze state of the table |
| `dtypes` | Supported and rejected element types, promotion and mixed-type policy |
| `shape_policy` | Shape kind, rank bounds, dimension rules, per-tensor size cap |
| `elementwise_policy` | Broadcasting decision shared by elementwise operators |
| `operators` | One record per operator: category, arity, shape rule, rank rule, dtype rule, attributes, constraints, `rejected_when` |
| `rejection_taxonomy` | Category, example and enforcing ticket for each rejection class |
| `non_goals` | Machine-readable non-goals list |
| `target`, `frontends` | Target and runtime assumptions, frontend tiers and owners |
| `deferred_decisions`, `consumers` | What is deliberately not frozen here, and who owns it |
| `reproducible_check` | The exact command that verifies the matrix (see below) |

Consumers: TF-015 conformance tests, TF-016 canonical JSON importer, TF-024 reference interpreter,
TF-026 ONNX validator and TF-028 diagnostics.

## 14. Guaranteed versus cuttable

This list feeds the formal scope gates and cut lines owned by TF-005.

| Guaranteed for a valid submission | Cuttable if schedule slips | Stretch only |
|---|---|---|
| Seven-operator canonical JSON path | Restricted ONNX importer (TF-026) | Broadcasting support |
| Reference interpreter and differential correctness | Fusion breadth beyond elementwise and MatMul+bias+ReLU | Autotuning of schedule candidates |
| SPIR-V and Vulkan execution on one device | Arena reuse beyond a verified static plan | Multi-device or asynchronous execution |
| Canonicalization, folding, DCE and CSE | Extra schedule candidates | Additional frontends or data types |
| Static arena plan and execution plan package | Benchmark and inspection tooling niceties | Advanced research backends (TF-E15) |
| Correctness, runtime, compile-time, dispatch and memory metrics | — | — |

## 15. Deferred decisions and owners

| Deferred decision | Owner |
|---|---|
| Numeric tolerance policy | TF-025, finalised by TF-072 |
| Diagnostic code catalog and messages | TF-028 |
| Per-operator deep semantics and negative tests | TF-013, TF-014 |
| Shape inference and verifier enforcement | TF-015 |
| Canonical JSON graph schema | TF-016 |
| Restricted ONNX subset breadth | TF-026 |
| Formal minimum, target and stretch cut lines | TF-005 |
| Vulkan API, SPIR-V version and ABI | TF-004 |
| Source layout, MLIR integration and CI wiring | TF-006, TF-007, TF-009 |

## 16. Reproducible verification

<!-- markdownlint-disable-next-line MD013 -->
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j && ctest --test-dir build --output-on-failure
```

The guard suite checks that the table is frozen and versioned, that the seven operator records are
complete, that the mandated negative cases (unsupported operator, dynamic dimension, unsupported
rank, unsupported element type) are rejected, that the taxonomy, non-goals and deferred decisions
are intact, and that this document's operator table matches the JSON cell for cell.

Definition-of-done evidence for this change: the guard suite was added first and failed with
`cannot read .../schemas/tgvc-operators-v1.json`; it then failed only on the document-parity cases
until this file existed; every test passes afterwards. The suite has no dependency on compiler
internals, so it runs in the M0 baseline as well as in CI.

## 17. Changing the matrix

1. Record the reason in an architecture decision record (TF-004 pattern) — scope changes are not
   silent edits.
2. Update `schemas/tgvc-operators-v1.json` and bump `matrix_version` (and `schema_version` when the
   record shape changes).
3. Update this document; the guard suite fails until the two agree again.
4. Re-run the reproducible check and record the raw output with the change.
5. Note the change in the weekly scope log alongside any milestone impact (TF-005 owns the gates).
