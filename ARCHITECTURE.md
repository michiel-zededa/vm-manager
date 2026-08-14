# Architecture

VM Manager is a native Qt 6 desktop app. It is deliberately split into a thin,
fluid **QML front end** and a **C++ core** that hides all hypervisor detail
behind one interface.

```
┌─────────────────────────────────────────────────────────────┐
│                        QML / Qt Quick UI                      │
│   Main.qml · views/* · components/* · Theme.qml (tokens)      │
│                                                               │
│  Talks ONLY to QObject facades exposed from C++:              │
│  AppController · VmListModel · ConnectionModel · tasks        │
└───────────────▲───────────────────────────────────────────────┘
                │  Q_PROPERTY / Q_INVOKABLE / signals (thread-safe)
┌───────────────┴───────────────────────────────────────────────┐
│                          C++ core                              │
│                                                                │
│   AppController  ── owns ──▶  ConnectionManager                │
│        │                          │                            │
│        │                          ▼                            │
│   VmListModel  ◀── observes ── IHypervisorBackend  (interface) │
│                                   ▲         ▲                  │
│                        ┌──────────┘         └──────────┐       │
│                 LibvirtBackend            MockBackend           │
│              (real, libvirt C API)   (in-memory fake data)     │
│                                                                │
│   ImageImporter (qemu-img / OVF)   SnapshotScheduler           │
└────────────────────────────────────────────────────────────────┘
                │                                   │
        libvirt (local socket / SSH / TLS)     qemu-img binary
                │
     ┌──────────┴───────────┬──────────────────┐
   QEMU/KVM (Linux)   QEMU+HVF (macOS)   QEMU/Hyper-V (Windows)
```

## Key decisions

### 1. libvirt as the single backend protocol
virt-manager *is* a libvirt client, and libvirt already runs on all three
platforms and can drive local QEMU as well as connect to remote hosts over
`qemu+ssh://` and `qemu+tls://`. Standardising on it gives us the widest feature
parity for the least platform-specific code. Native fast paths (Apple
Virtualization.framework, Windows WHPX) are a **later** phase behind the same
interface — see `ROADMAP.md`.

### 2. `IHypervisorBackend` — the seam
Every operation the UI can trigger (list domains, start/stop, snapshot, define,
import, list pools/networks…) is a method on `IHypervisorBackend`
(`src/core/Backend.h`). Two implementations ship:

- **`LibvirtBackend`** — the real client, compiled only when libvirt is found
  (`HAVE_LIBVIRT`). Wraps `virConnect*`, `virDomain*`, `virStorage*`,
  `virNetwork*`.
- **`MockBackend`** — realistic in-memory data. Lets the whole UI run, be
  demoed, and be UI-tested on a machine with no hypervisor (e.g. a fresh Mac).
  It is also the default target in CI's UI smoke test.

Selection is automatic at runtime: real backend if libvirt is available and a
connection succeeds, otherwise the mock (overridable with `VMM_BACKEND=mock`).

### 3. The GUI thread never does I/O
libvirt calls can block for seconds against a slow remote host. Every backend
call is dispatched to a worker (`QThreadPool` / `QtConcurrent`) and results are
delivered back to the GUI thread via queued signals. Models emit fine-grained
`dataChanged` so the QML list animates instead of flashing.

### 4. Models, not data dumps
`VmListModel` and `ConnectionModel` are `QAbstractListModel`s with named roles
consumed directly by QML `Repeater`/`ListView`. Live stats (CPU %, memory,
disk/net rates) are pushed as role updates on a timer, feeding the sparklines.

### 5. Image import pipeline
`ImageImporter` shells out to `qemu-img` for format conversion
(`vmdk/vhdx/vhd/vdi/raw → qcow2`) and parses **OVF/OVA** (a tar of an OVF
descriptor + disk images) to reconstruct CPU/RAM/NIC/disk and register the
result as a new domain. Conversions run as cancellable background tasks with
progress reported to the UI.

## Threading contract

- QML → C++: only via `Q_INVOKABLE` methods and properties on facade objects.
- C++ → QML: only via signals (auto/queued connections). Never touch QML objects
  from a worker thread.
- Backends are owned by `ConnectionManager`; each connection has its own worker
  so one slow host cannot stall another.

## Directory map

| Path | Responsibility |
|---|---|
| `src/core/Backend.h` | `IHypervisorBackend` + POD data types (`VmInfo`, `HostInfo`, `SnapshotInfo`, …) |
| `src/core/MockBackend.*` | In-memory fake backend |
| `src/core/LibvirtBackend.*` | Real libvirt client (guarded by `HAVE_LIBVIRT`) |
| `src/core/ConnectionManager.*` | Owns connections/backends, worker dispatch |
| `src/core/AppController.*` | Root QML facade; app-level actions & state |
| `src/core/VmListModel.*` | `QAbstractListModel` of VMs |
| `src/core/ConnectionModel.*` | `QAbstractListModel` of hosts |
| `src/core/ImageImporter.*` | qemu-img + OVF/OVA import |
| `src/ui/qml/Theme.qml` | Design tokens (color, type, spacing, motion) |
| `src/ui/qml/components/` | Reusable widgets (buttons, cards, badges…) |
| `src/ui/qml/views/` | Screens (Dashboard, Detail, Wizard, Import…) |
