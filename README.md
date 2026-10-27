# TitanXFS — High-Performance Clean-Room XFS Storage Engine for Windows NT & MicaNT

[![TitanXFS CI](https://github.com/MicaNT-Kernel/TitanXFS/actions/workflows/ci.yml/badge.svg)](https://github.com/MicaNT-Kernel/TitanXFS/actions/workflows/ci.yml)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Standard: Clean-Room 17 U.S.C. § 1201(f)](https://img.shields.io/badge/Clean--Room-17%20U.S.C.%20§%201201(f)-green.svg)](CLEAN_ROOM_PROVENANCE.md)

![TitanXFS Hero Banner](assets/titanxfs_hero.jpg)

**TitanXFS** is an independent, clean-room ISO C++23 Silicon Graphics XFS storage engine, WinFsp user-mode filesystem driver, and background management daemon for Windows NT, MicaNT, and POSIX platforms.

Designed strictly under **17 U.S.C. § 1201(f)** and ***Google v. Oracle*** interoperability standards, TitanXFS delivers native XFS v5 volume formatting, userland File Explorer mounting via WinFsp, B+ tree extent resolution, Allocation Group (AG) telemetry, and background consistency scrubbing without third-party kernel drivers or blue screen (BSOD) risks.

---

### Key Capabilities

- **Red Hat Enterprise Linux (RHEL) & CentOS Compatibility**: Seamlessly mount, browse, read, and verify disks formatted with RHEL, Rocky Linux, AlmaLinux, CentOS, or SGI IRIX directly in Windows.
- **WinFsp User-Mode Driver Bridge (`titanxfs_mount`)**: Mount any XFS disk image, raw partition, or VHD to a native Windows drive letter (e.g. `X:\`) with full integration into Windows File Explorer, PowerShell, Command Prompt, and WSL. Reports `FileSystemName = L"XFS"`.
- **1-Click Turnkey Installer (`titanxfs_setup.exe`)**: Self-contained installer embedding all binaries, high-resolution hero artwork, and signed WinFsp runtime MSI using LZ4 PE overlay compression.
- **Windows NT Background Daemon (`titanxfs_svc.exe`)**: Autonomous background service supporting boot-time auto-mounts, Named Pipe IPC (`\\.\pipe\titanxfs_ipc`), and scheduled 24-hour integrity scrubbing.
- **CLI Utilities**:
  - `titanxfs_format.exe`: Format disk images or physical drives to standard XFS v5.
  - `titanxfs_check.exe`: Filesystem scrubber, superblock validator, and bit-rot diagnostic tool.
- **Unified Titan Storage Hub Integration**: Full management and telemetry support in [Titan Storage Hub](https://github.com/MicaNT-Kernel/TitanStorageHub).

---

### Architecture & On-Disk Geometry

| Component | Offset / Magic | Purpose |
|---|---|---|
| **Superblock (`xfs_sb_t`)** | Offset `0x00` (`0x58465342` `"XFSB"`) | 64-bit volume geometry, root inode pointer, block size, UUID, and feature flags |
| **Allocation Group Free (`XFS_AGF`)** | Sector 1 (`0x58414746` `"XAGF"`) | AG free block counts, longest extents, and free space btree roots (`ABTB`, `ABTC`) |
| **Allocation Group Inode (`XFS_AGI`)** | Sector 2 (`0x58414749` `"XAGI"`) | AG allocated inode counters and inode btree roots (`IBT`, `FIBT`) |
| **Allocation Group Free List (`XFS_AGFL`)** | Sector 3 (`0x5841474C` `"XAGL"`) | Dynamic free block cache for btree block splits and merges |
| **Dynamic Inode (`xfs_dinode_t`)** | `0x494E` (`"IN"`) | 512-byte v3 dinode with B+ tree data fork, extent records, and metadata |

---

### Building from Source

```powershell
# Clone repository
git clone https://github.com/MicaNT-Kernel/TitanXFS.git
cd TitanXFS

# Configure CMake with Clang & Ninja
cmake -B build -G "Ninja" -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release

# Build all targets (-j2)
cmake --build build -j2

# Run tests
ctest --test-dir build --output-on-failure
.\build\titanxfs_tests.exe
```

---

### License & Compliance

Licensed under the **Apache License, Version 2.0**. All reverse-engineering and format structures adhere strictly to statutory interoperability protections under **17 U.S.C. § 1201(f)** and ***Google LLC v. Oracle America, Inc.***.
