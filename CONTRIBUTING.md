# Contributing

Thanks for helping build VM Manager! This is an early-stage project; the
architecture (see [`ARCHITECTURE.md`](ARCHITECTURE.md)) is deliberately set up so
you can contribute UI work without a hypervisor and backend work without a GUI.

## Getting started

1. Read [`BUILDING.md`](BUILDING.md) and get a build running against the **mock
   backend** (`VMM_BACKEND=mock`). No libvirt required.
2. Pick an item from [`ROADMAP.md`](ROADMAP.md) (⬜ = unclaimed).
3. Branch, build, test, open a PR.

## Ground rules

- **Never do I/O on the GUI thread.** Backend calls go through
  `ConnectionManager`'s worker dispatch and return via signals.
- **UI talks only to `IHypervisorBackend`.** No `#include <libvirt/*>` outside
  `LibvirtBackend.*`.
- **Every backend method must be implemented in both `MockBackend` and
  `LibvirtBackend`.** The mock keeps the UI runnable everywhere; CI enforces it.
- **Design tokens, not magic numbers.** Colors, spacing, radii, type and motion
  come from `Theme.qml`. If you need a new token, add it there.
- **Keep it accessible.** Focus order, keyboard operability, and sufficient
  contrast are review criteria, not afterthoughts.

## Commit / PR conventions

- Small, focused commits with imperative subjects (`Add VNC console widget`).
- PRs describe *what* and *why*, link the roadmap item, and include a screenshot
  or screen recording for any UI change.
- CI (build + tests on Linux/macOS/Windows) must be green.

## Code style

- C++20, 4-space indent, `clang-format` (config in repo root).
- QML: PascalCase for components, camelCase for properties; one component per
  file; keep view files declarative and push logic into C++.

## Releases

Releases are cut by pushing a `vX.Y.Z` tag; the release workflow builds and
attaches a packaged binary per OS. See `.github/workflows/release.yml`.
