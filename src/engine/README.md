# src/engine

Tracktion Engine glue: edit/project lifecycle, transport, rendering, plugin management,
import/export. **No GUI includes** — this code must compile into `daw_core` and be exercised
headlessly by `daw_tests`.

`src/engine/dsp/` (when it exists) is subject to the CI grep tripwire for RT violations —
see `docs/realtime-rules.md`.
