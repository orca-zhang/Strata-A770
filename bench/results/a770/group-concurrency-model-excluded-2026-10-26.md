# A770 group concurrency: excluded model attempt

The active performance screen is both Code2K and Prose2K at least **1%** faster, with absolute baseline time drift at most **3%**. Correctness and independent confirmation requirements remain unchanged. The original 3% protocols and measurements are preserved.

The [operator screen](group-concurrency-2026-10-26.md) selected eight group rows only for pure VRAM T1/T2. It did not select any mapped-host shape.

The isolated model attempt in `build-sycl-a770-group-concurrency-model-20261026` did not apply that candidate. In its frozen `verify.cpp`, routing collection is initialized from the frozen runtime settings (line 608), while the candidate requires `!collect_routing_` (line 1436). Those settings enable routing collection, so the candidate condition is false in every arm. Neither A1 nor B1 logged a `GROUP_ROWS8` dispatch witness. The source's routing-collection path combines resident and pinned-mirror experts; dropping the guard would extend the candidate beyond the pure-VRAM operator selection.

A1 completed all eight original 2048-input/full128-output requests and matched the original BF16 outputs. B1 reached READY with 6297 slots but completed no 2K request. After identifying the runner and its exact child executable, only that child was deliberately stopped during B1's first 2K request. The runner exited with an error and cleaned up its monitor and GPU lock. This was an intentional cancellation of an inert experiment, not an unexplained engine failure.

The frozen runner/source, protocol, logs, object, executable and `partial.json` remain intact. There is no completed A/B/A result. All timings from this attempt are excluded from gain or regression decisions. No production EXE/configuration changed, and stable40tok/s remains unmet.

The [HC Q8 independent confirmation](hc-q8-model-1pct-confirm-2026-10-26.md) remains a separate completed result: the original cohort passes the updated1% screen, but its independent repeat does not; HC Q8 is not accepted.
