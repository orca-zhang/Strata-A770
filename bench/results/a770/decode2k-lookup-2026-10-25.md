# A770 original 2K/128: suffix lookup isolation

A77016GB / PCIe3.0, Windows, driver32.0.101.9034, DPC++2025.3.2. Change only --suffix-draft3/0/3 on the frozen graph engine, requested expert count6000/actual6297 slots, FP16 KV, reserve1536MiB, context/prefill8192, min-p0.5, MTP attention1024, full248320-row vocabulary, prompt-cache0 and adapt-every1/swaps32/decay0.5. ALL arms explicitly use CLI spec6/mtp-max-t4, reproducing the earlier CLI-spec4 automatic verify6/MTP4 capacities. Runtime INFO confirms both capacities and the lookup setting in every arm. Precision, weight layout and the verifier capacity are unchanged.

The frozen suffix policy retains acceptance and measured round-cost estimates across requests and uses them to select windows. This is a possible source of changing offered counts, even when the separate MTP cost-window switch is off. Disabling lookup is a controlled diagnostic, not proof of a unique cause. This older frozen generator also lacks the newer output-budget clamp; lookup removal does not add that clamp.

Each fresh A1/B1/A2 process runs Short16 initialization, one warmup/case and three interleaved retained cycles. All24 original2048-input/full128-output requests match frozen references (3072 tokens). Every warmup and low observation is retained. Expert residency and the suffix lookup policy continue adapting across requests; prompt-cache0 does not reset them.

| Arm | Lookup | Case | Decode median tok/s | Full retained range |
| --- | ---: | --- | ---: | ---: |
| A1 | 3 | Code2K | 30.860 | 30.147–30.936 |
| A1 | 3 | Prose2K | 30.152 | 29.988–30.724 |
| B1 | 0 | Code2K | 30.821 | 30.605–30.824 |
| B1 | 0 | Prose2K | 30.426 | 30.389–30.441 |
| A2 | 3 | Code2K | 30.937 | 30.801–30.956 |
| A2 | 3 | Prose2K | 30.146 | 29.228–30.517 |

| Case | Paired throughput change | Adjacent baseline time drift |
| --- | ---: | ---: |
| Code2K | -0.252% | -0.248% |
| Prose2K | +0.920% | +0.019% |

Both cases must gain>=3% with<=3% absolute baseline time drift. Screen passed: **False**. A positive screen still needs independent confirmation; stable40 tok/s is not established by this cohort. No production change or general quality claim.

A predeclared separate read-only Sysman process samples once per second, with at least one sample during every decode. All per-request frequency/throttle observations, accepted/offered counts, startup free VRAM and timing ranges are retained in the compact data. Monitoring overhead is not separately quantified, and sampling gaps do not rule out short changes or establish a paging cause.

[Data](decode2k-lookup-2026-10-25.json). Offline check:

```powershell
python bench/results/a770/check_decode2k_lookup_20261025.py
```

The optional --local flag audits frozen local artifacts. Run it under the shared lock on this measurement machine to avoid hashing contention.
