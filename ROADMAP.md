# Roadmap & feature comparison

Legend: ✅ done · 🟡 in progress · ⬜ planned · 💤 later phase

## Feature parity matrix (vs. virt-manager / UTM)

| Area | Feature | virt-manager | UTM | VM Manager target | Status |
|---|---|:---:|:---:|:---:|:---:|
| **Lifecycle** | Start / shutdown / force-off / pause / resume / reboot | ✅ | ✅ | v1 | 🟡 |
| | Delete VM (with disk cleanup choice) | ✅ | ✅ | v1 | 🟡 |
| | Autostart on host boot | ✅ | ✗ | v1 | ⬜ |
| **Dashboard** | Multi-host VM gallery | ✅ | partial | v1 | 🟡 |
| | Live CPU/RAM/disk/net stats + sparklines | ✅ | partial | v1 | 🟡 |
| **Create** | Guided *simple* wizard (OS gallery) | partial | ✅ | v1 | 🟡 |
| | *Advanced* wizard (full hardware control) | ✅ | ✅ | v1 | 🟡 |
| | Curated downloadable-OS gallery (RPi-Imager-style) | ✗ | ✅ | v1 | ⬜ |
| | Cloud-init / sysprep seeding | partial | partial | v1 | 💤 |
| **Import** | `.qcow2` / `.raw` register | ✅ | ✅ | v1 | 🟡 |
| | `.vmdk` (VMware) → qcow2 | ✗ | partial | v1 | ⬜ |
| | `.vhdx` / `.vhd` (Hyper-V) → qcow2 | ✗ | ✗ | v1 | ⬜ |
| | `.vdi` (VirtualBox) → qcow2 | ✗ | ✗ | v1 | ⬜ |
| | `.ova` / `.ovf` (appliance, multi-disk) | ✗ | partial | v1 | ⬜ |
| | `.qed` / `.vpc` / `.parallels` | ✗ | ✗ | v1.x | 💤 |
| **Console** | Embedded VNC | ✅ | ✅ | v1 | ⬜ |
| | Embedded SPICE (+ USB redirect, audio) | ✅ | ✗ | v1.x | 💤 |
| | Serial / text console | ✅ | ✅ | v1 | ⬜ |
| **Storage** | List/create/delete pools & volumes | ✅ | partial | v1 | ⬜ |
| | Attach/detach disks, resize | ✅ | ✅ | v1 | ⬜ |
| **Network** | List/create virtual networks & bridges | ✅ | partial | v1 | ⬜ |
| | Attach/detach NICs, MAC/model control | ✅ | ✅ | v1 | ⬜ |
| **Snapshots** | Take / restore / delete | ✅ | ✅ | v1 | ⬜ |
| | **Scheduler** with retention policy | ✗ | ✗ | v1 | ⬜ |
| | Disk-only vs. full (memory) snapshots | ✅ | partial | v1 | ⬜ |
| **Templates** | Mark VM as template | partial | ✅ | v1 | ⬜ |
| | Full clone | ✅ | ✅ | v1 | ⬜ |
| | Linked clone (backing file) | partial | ✅ | v1 | ⬜ |
| **Connections** | Local libvirt | ✅ | n/a | v1 | 🟡 |
| | Remote `qemu+ssh://` | ✅ | ✗ | v1 | ⬜ |
| | Remote `qemu+tls://` | ✅ | ✗ | v1.x | 💤 |
| **Native fast paths** | Apple Virtualization.framework (macOS) | ✗ | ✅ | later | 💤 |
| | Windows Hyper-V / WHPX | ✗ | ✗ | later | 💤 |

## Delivery phases

### Phase 0 — Foundation (this PR)
- ✅ Project scaffold: CMake, Qt 6 QML app, cross-platform CI matrix.
- ✅ `IHypervisorBackend` seam + `MockBackend` so the UI runs everywhere.
- ✅ Design system (`Theme.qml`) + component library.
- ✅ App shell, multi-host sidebar, VM dashboard gallery with live (mock) stats.
- ✅ Create-VM wizard flow (simple + advanced) wired to the backend interface.
- 🟡 `LibvirtBackend` skeleton behind `HAVE_LIBVIRT`.
- 🟡 `ImageImporter` design + qemu-img/OVF plumbing.

### Phase 1 — Real hypervisor + core management
- `LibvirtBackend` fully implemented (domains, define/undefine, lifecycle).
- Live stats from `virDomainGetInfo`/`virDomainGetStats`.
- Storage pool & network management.
- Remote `qemu+ssh://` connections.

### Phase 2 — Console & media
- Embedded VNC console, then serial console.
- Curated downloadable-OS gallery with checksum verification.

### Phase 3 — Import, snapshots, templates
- Full cross-format import (vmdk/vhdx/vdi/ova).
- Snapshots + scheduler with retention.
- Templates, full & linked clones.

### Phase 4 — Polish & native fast paths
- SPICE, USB redirect, cloud-init seeding.
- Apple Virtualization.framework and Windows WHPX backends behind the seam.
- Signed/notarised installers for all three OSes.

## Open questions (input welcome)
- **Downloadable-OS gallery source** — host a curated JSON manifest (like RPi
  Imager's `os_list`) or let users add their own catalog URLs? *(proposed: both — ship a default manifest, allow custom URLs.)*
- **Remote auth UX** — rely on the user's existing SSH agent/keys, or offer an
  in-app key manager? *(proposed: SSH agent first; key manager later.)*
- **Snapshot scheduler persistence** — store schedules in app config, or as
  libvirt metadata on the domain so they survive reinstalls? *(proposed: domain metadata.)*
