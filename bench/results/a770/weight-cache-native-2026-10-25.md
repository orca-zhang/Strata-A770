# A770 weight cache hint: native instruction audit

A770 16GB / PCIe 3.0, driver 32.0.101.9034, DPC++ 2025.3.2. This audits the `gu_pair_wg128` IQ2_S GU kernel from the [completed cache-hint screen](weight-cache-hint-2026-10-25-v2.md), using its exact frozen runtime environment.

The baseline and candidate instructions through the unique end-of-thread instruction are identical. The only ASM difference is two immediate values after that instruction; the sole trailing label has no references. Complete ASM files therefore differ, while the executed prefix and its memory-load instructions match. Prefix SHA256: `e875d6bd7ebfee53847d4597f550db3531a306648052a134f6415d2125c144ef`.

The original audit checker failed because ocloc also emits an `Intel_Symbol_Table_Void_Program` ASM. A fresh postprocessing audit selects exactly one GU ASM and preserves the failed checker and all original extraction/disassembly artifacts. No recompilation or new GPU timing was needed.

This particular hint did not change the GU execution instructions. This does not establish that other cache/repack approaches cannot help; down and other kernels were not audited. There is no new whole-model speed result or production change.

[Compact evidence](weight-cache-native-2026-10-25.json). Check without a GPU:

```powershell
python bench/results/a770/check_weight_cache_native_20261025.py
python bench/results/a770/check_weight_cache_native_20261025.py --local
```
