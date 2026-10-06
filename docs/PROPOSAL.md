# TGVC project proposal

## Working title

**TGVC: Tensor Graph to Vulkan Compiler**

## Problem statement

Deploying tensor computations to Vulkan compute requires several error-prone steps: interpreting a graph, lowering tensor operations into executable kernels, selecting a schedule for the target device, and managing intermediate memory. A small compiler can make these steps reproducible, but it must demonstrate both numerical correctness and measurable execution benefits without claiming to be a production-scale machine-learning compiler.

This project will implement and evaluate a deliberately bounded compiler for static-shape `float32` tensor graphs. The core path will support a canonical graph representation, a custom MLIR-based intermediate representation, lowering to SPIR-V, and execution through a minimal Vulkan runtime. The evaluation will focus on correctness, optimization effects, and hardware-aware scheduling and memory planning.

## Scope and boundaries

### In scope

- Static-shape `float32` graphs.
- A documented core operator set: Constant, Add, Mul, ReLU, MatMul, Reshape, and Transpose.
- Canonical JSON input, with a restricted ONNX importer where practical.
- MLIR-based graph representation and lowering.
- SPIR-V generation and Vulkan compute execution.
- Reference execution, differential testing, optimization experiments, scheduling, and memory planning.

### Out of scope

- General production-model compatibility.
- Dynamic shapes, arbitrary data types, or general quantized-model support.
- Claims of superiority over production GPU or ML compilers.
- Proprietary hardware details, data, formats, or implementation material.

## Research questions

### RQ1 — Correctness

**Can TGVC compile the supported static-shape tensor graphs to Vulkan while preserving the reference result within a documented tolerance?**

- **Independent variable:** Graph operator composition and supported shape class.
- **Dependent variables:** Numerical error, pass/fail correctness rate, and failure-diagnostic quality.
- **Measurement method:** Run the same graph through the host reference interpreter and Vulkan backend; compare named outputs element-by-element using the stated tolerance policy.
- **Evidence:** Differential test matrix covering individual operators, composed graphs, boundary shapes, and valid/invalid inputs.

### RQ2 — Optimization

**How much do canonicalization and operation fusion reduce execution work compared with the unoptimized pipeline?**

- **Independent variable:** Optimization configuration: baseline, canonicalization only, fusion only, and the combined pipeline.
- **Dependent variables:** Dispatch count, intermediate memory, compile time, and steady-state runtime.
- **Measurement method:** Compile and execute the same workload under each configuration with fixed inputs, warmups, repetitions, device, and compiler settings.
- **Evidence:** Ablation tables containing correctness, node count, dispatch count, compile time, runtime, and peak intermediate memory.

### RQ3 — Hardware-aware scheduling and memory planning

**Can a target-profile-guided schedule and static memory plan improve performance or memory use relative to a fixed naive schedule?**

- **Independent variable:** Scheduling and allocation policy: naive fixed schedule, heuristic target-aware schedule, and planned arena reuse.
- **Dependent variables:** Runtime, dispatch count, peak intermediate memory, and schedule-selection accuracy against measured candidates.
- **Measurement method:** Evaluate representative workloads across available Vulkan devices or target profiles, recording device limits, schedule choice, raw timings, and planned memory ranges.
- **Evidence:** Per-workload and per-target comparisons, including counterexamples and cases where the heuristic provides no benefit.

## Evaluation plan

All experiments will use versioned workload fixtures, deterministic seeds where randomness is needed, and machine-readable result files. Correctness will be checked before performance measurements. Timing will separate compile/setup from execution where the platform permits, and each report will include hardware, driver, API, compiler, workload, and configuration metadata.

The minimum result set will report:

| Metric | Purpose |
|---|---|
| Correctness error and pass rate | Establish semantic validity against the reference |
| Steady-state runtime | Measure execution performance after warmup |
| Compile time | Measure non-runtime compiler cost |
| Dispatch count | Quantify launch and synchronization work |
| Peak intermediate memory | Quantify memory-planning effectiveness |

## Research-question-to-experiment traceability

| Question | Experiment | Baseline | Main outputs |
|---|---|---|---|
| RQ1 | Differential correctness matrix | Host reference interpreter | Error, pass rate, failing artifact |
| RQ2 | Optimization ablation | Unoptimized lowering | Runtime, compile time, dispatches, memory |
| RQ3 | Schedule and memory-plan comparison | Naive schedule and no reuse | Runtime, dispatches, peak memory, ranking evidence |

## Engineering deliverables versus research claims

### Engineering deliverables

- A reproducible build and test setup.
- Canonical graph parser and verifier.
- Reference interpreter for the supported operators.
- MLIR dialect and lowering pipeline.
- SPIR-V and Vulkan execution path.
- Inspection, benchmarking, diagnostics, and result schemas.
- Versioned workload fixtures and automated tests.

### Research claims

- TGVC preserves the reference results for the tested supported scope.
- The measured optimization configurations reduce selected costs on the evaluated workloads, if the data supports that conclusion.
- Target-aware scheduling or memory planning improves the stated metrics under the tested device profiles, if the data supports that conclusion.

Claims will be limited to the workloads, operators, devices, toolchain versions, and measurement procedures actually evaluated. Negative or neutral results will be retained and discussed rather than hidden.

## Definition of success

The project succeeds when the implementation has a reproducible end-to-end path for the bounded scope, the differential matrix demonstrates correctness, and each research question is answered by archived measurements traceable to a command and raw result. The final report will distinguish measured facts from interpretation and future work.
