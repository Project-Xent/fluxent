# fluxent
Windows rendering backend for the `xent` UI framework.

Minimal, production-focused implementation using Direct2D and DirectComposition.

Quick start
- Requirements: Windows 10+, Visual Studio, C++20, `xmake`.
- Dependency: `xent-core` and `yoga`.
- Build: `xmake build` — run example: `xmake run hello_fluxent`.

License: BSD-3-Clause

Status
- Core rendering pipeline: implemented (Direct2D + DirectComposition).
- Controls: basic set implemented (CheckBox, Button, TextBox, Slider); more in progress.
- Animation: lightweight Animator system (frame-based); external Lottie deps removed.
- Plugin system: kept for future extensions; not required for core controls.

UI Style
- Visual language: Fluent Design System (v2).
- Principles: light-weight, motion-first, acrylic/blur where appropriate, Segoe Fluent icons for glyphs.

For usage and contribution, follow repository CONTRIBUTING.md and submit issues before large features.