# VM Manager

> A modern, cross-platform virtual-machine manager. Feature parity with
> [virt-manager](https://github.com/virt-manager/virt-manager), the fluid
> single-download experience of the Raspberry Pi Imager, and native builds for
> **macOS, Windows and Linux**.

VM Manager is a native desktop application built with **Qt 6 / Qt Quick (QML)**
and **C++**, talking to hypervisors through **libvirt** — locally and over
SSH/TLS to remote hosts. It targets two audiences at once:

- **Homelab & desktop power users** who want to spin up, import and snapshot
  VMs on their own machine with zero libvirt knowledge.
- **Sysadmins** managing fleets of VMs on remote KVM/QEMU hosts who need
  multi-host connections, bulk actions and robustness.

---

## Why another VM manager?

| | virt-manager | UTM | RPi Imager | **VM Manager** |
|---|:---:|:---:|:---:|:---:|
| Cross-platform (Mac/Win/Linux) | Linux only | Mac/iOS only | ✅ | ✅ |
| Modern, fluid UI | ✗ (GTK, dated) | ✅ | ✅ | ✅ (QML) |
| libvirt (local **+** remote) | ✅ | ✗ | — | ✅ |
| Cross-format image import | partial | partial | ✗ | ✅ (ova/vmdk/vhdx/vdi/qcow2/raw) |
| Snapshots **+ scheduler** | manual | manual | ✗ | ✅ |
| Templates & cloning | ✅ | ✅ | ✗ | ✅ |
| Curated downloadable-OS gallery | ✗ | ✅ | ✅ | ✅ |
| Single-file installer per OS | ✗ | ✅ | ✅ | ✅ |

See [`ROADMAP.md`](ROADMAP.md) for the full feature comparison and phased plan,
and [`ARCHITECTURE.md`](ARCHITECTURE.md) for how it fits together.

---

## Feature overview (target for v1)

- **Dashboard** — every VM across every connected host in one gallery, with live
  status and CPU / memory / disk / network sparklines.
- **VM lifecycle** — start, shut down, force-off, pause, resume, reboot, delete,
  with clear, reversible affordances.
- **Create-VM wizard** — guided *simple* mode (pick an OS from a curated gallery,
  click go) and *advanced* mode (full control of CPU/RAM/disk/NIC/firmware).
- **Cross-format image import** — drop in an `.ova`, `.vmdk`, `.vhdx`, `.vdi`,
  `.vhd`, `.qcow2`, `.raw` (and more) and it is converted and registered for you.
- **Integrated console** — graphical (VNC/SPICE) and serial consoles embedded in
  the app; no external viewer required.
- **Storage & networking** — manage storage pools/volumes and virtual
  networks/bridges.
- **Snapshots & scheduler** — take/restore/delete snapshots and schedule
  automatic ones with retention policies.
- **Templates & cloning** — turn any VM into a reusable template; full and
  linked clones.
- **Multi-host** — connect to many local and remote libvirt endpoints at once
  over SSH/TLS.

Status of each item is tracked in [`ROADMAP.md`](ROADMAP.md).

---

## Building

Full per-platform instructions live in [`BUILDING.md`](BUILDING.md). The short
version:

```bash
# Configure (UI runs against the mock backend if libvirt is not found)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/vm-manager            # or open the .app / .exe
```

Requirements: **Qt 6.5+**, **CMake 3.21+**, a C++20 compiler. `libvirt` and
`qemu-img` are optional at build time — when absent, VM Manager launches against
a built-in **mock backend** so you can develop and demo the UI on any machine.

---

## Project layout

```
vm-manager/
├── CMakeLists.txt          # top-level build
├── cmake/                  # packaging & helper modules
├── src/
│   ├── main.cpp
│   ├── core/               # C++ core: backends, models, controllers
│   │   ├── Backend.h           # IHypervisorBackend + shared data types
│   │   ├── MockBackend.*       # in-memory fake for UI dev & tests
│   │   ├── LibvirtBackend.*    # real libvirt client (compiled if available)
│   │   ├── AppController.*     # QML-facing facade
│   │   ├── VmListModel.*       # QAbstractListModel of VMs
│   │   ├── ConnectionModel.*   # hosts / connections
│   │   └── ImageImporter.*     # qemu-img + OVA conversion
│   └── ui/qml/             # Qt Quick UI
│       ├── Main.qml
│       ├── Theme.qml           # design tokens (colors, spacing, type)
│       ├── components/         # reusable widgets
│       └── views/              # screens
├── .github/workflows/      # cross-platform CI
└── docs, ROADMAP, ARCHITECTURE, BUILDING, CONTRIBUTING
```

---

## Design principles

1. **The UI never blocks.** All hypervisor calls run off the GUI thread; the
   interface stays fluid even against a slow remote host.
2. **The UI is decoupled from libvirt.** Everything speaks to
   `IHypervisorBackend`, so the app runs against a mock on machines without a
   hypervisor and stays testable.
3. **Progressive disclosure.** Simple by default (RPi-Imager-style), with an
   *Advanced* path that never hides power from experts (virt-manager-style).
4. **Native, small, fast.** One self-contained installer per OS, no runtime to
   install.

---

## License

[MIT](LICENSE).
