# Third-party licenses

This project's own code is licensed **GPL-3.0-or-later**. Every third-party dependency
vendored into this repository (as a pinned git submodule under `libs/`) must be
GPLv3-compatible, and every one must have a row in the table below. When you add, remove,
or re-pin a dependency, update this file in the same change and verify the license by
reading the dependency's own `LICENSE`/`COPYING` file — do not guess.

Licenses below were verified by reading the `LICENSE`/`COPYING`/`README` files in each
cloned submodule at the pinned revision.

## Distribution note

Distributed builds combine this GPL-3.0-or-later code with **JUCE under its AGPL-3.0
side** (JUCE is dual AGPL-3.0/commercial; we use the AGPL side). A combined work that
includes AGPL-3.0 code is, as a whole, effectively bound by AGPL-3.0 network-use terms.
The Tracktion Engine and Rubber Band are likewise used under their GPL (open-source)
sides, not their commercial licenses.

## Dependencies

| Dependency | Version / SHA pinned | Upstream | License | GPLv3-compatibility |
| --- | --- | --- | --- | --- |
| Tracktion Engine | `2877b621f2fbee564d0696a616b86bf8ba8c8ab0` (develop, engine 3.2.0) | https://github.com/Tracktion/tracktion_engine | GPL-3.0-or-later / Commercial (dual) | Yes — used under the GPL-3.0 side; identical to project license. |
| JUCE | `7c89e11f6b7316c369f3d3f22227c60e816e738b` (JUCE 8.0.12), via the engine's `modules/juce` submodule | https://github.com/juce-framework/JUCE | AGPL-3.0 / Commercial (dual) | Yes — used under the AGPL-3.0 side. AGPL-3.0 is one-way compatible with GPL-3.0; the combined work is governed by AGPL-3.0 (see distribution note). |
| Rubber Band Library | `1d95888bec3ae0a17c0c4af791810d5a63f6bc35` (tag `v4.0.0`) | https://github.com/breakfastquay/rubberband | GPL-2.0-or-later / Commercial (dual) | Yes — used under the GPL "v2 or, at your option, any later version" side, which permits use under GPL-3.0. |
| Catch2 | `644821ce28cb25d7992a4d0375b1d83214392592` (tag `v3.9.1`) | https://github.com/catchorg/Catch2 | BSL-1.0 (Boost Software License) | Yes — permissive, GPLv3-compatible. Test-only dependency. |
| melatonin_test_helpers | `790f57bb766b3575ff40a98fb19c679a155578b5` (main) | https://github.com/sudara/melatonin_test_helpers | MIT | Yes — permissive, GPLv3-compatible. Test-only dependency. |
| melatonin_audio_sparklines | `379b719e3ef6b43f3e8f4d078cdf718962a4c8da` (main) | https://github.com/sudara/melatonin_audio_sparklines | MIT | Yes — permissive, GPLv3-compatible. Test-only dependency, required by melatonin_test_helpers. |
| moodycamel readerwriterqueue | `8b2176698e9bdaba653cdc20c32b54737a934b47` (tag `v1.0.7`) | https://github.com/cameron314/readerwriterqueue | BSD-2-Clause ("Simplified BSD"); embedded semaphore in `atomicops.h` is zlib | Yes — both BSD-2-Clause and zlib are permissive and GPLv3-compatible. |

### Notes

- **JUCE** is pinned transitively: it is the engine's own `modules/juce` gitlink at the
  engine SHA above, not a top-level submodule of this repo. Build against that JUCE only
  (see `CLAUDE.md`).
- **Rubber Band** source headers and `COPYING` carry the GPLv2 text, but the project
  `README.md` and per-file headers state "either version 2 of the License, or (at your
  option) any later version" — i.e. GPL-2.0-**or-later**, which is GPLv3-compatible.
- **readerwriterqueue**: most files are BSD-2-Clause; Jeff Preshing's semaphore
  implementation embedded in `atomicops.h` is under the zlib license. Both are permissive.
