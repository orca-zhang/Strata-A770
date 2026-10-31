# A770 lossless native-weight allocation smoke

A77016GB / PCIe3.0, Windows, driver32.0.101.9034, DPC++2025.3.2. Same isolated frozen engine and matching ABI, default-off native arena0/1. Only300 original quantized native projection allocations are packed into one256-byte-aligned device arena. No weight representation, kernel, arithmetic or HC quantization changes. Views preserve the tensor count; one owner releases the arena.

Both arms read every native upload back AFTER all uploads and compare all1,546,488,320bytes to GGUF,300 matrices/arm. This catches a later view overlapping an earlier one. All600 matrix comparisons pass. Short16 initialization followed by originalCode2K/Prose2K twice, full128 output each; all1024 output tokens equal the historical reference. Fixed requestedcache6000/actual6297slots,reserve1536,FP16KV,fullvocabulary,context8192,verify6/MTP4/lookup3,min-p0.5,prompt-cache0. Smoke timings are preserved but are not a performance cohort.

READY free memory is1558MiB for A and1621MiB for B: **+63MiB in this A/B smoke**. A header-only estimate assuming2MiB allocation rounding predicted254MiB; that estimate is not realized. This is startup free memory, not peak request memory or independently confirmed physical allocation savings. The smoke does not establish the driver's allocation granularity.

The prepared cache6150 performance cohort requires>=240MiB measured free-memory gain before running. **The prerequisite fails**, so the larger-cache cohort is not launched and there is no throughput result or independent confirmation. The40tok/s goal remains unmet. Production executable/configuration is unchanged.

[Data](native-dense-arena-2026-10-25.json). Offline check:

```powershell
python bench/results/a770/check_native_dense_arena_20261025.py
```

Optional --local verifies the private frozen records; run it under the shared GPU/compiler lock on the measurement machine.
