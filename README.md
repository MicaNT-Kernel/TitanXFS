# TitanXFS — High-Performance Clean-Room XFS Storage Engine for Windows NT & MicaNT

[![TitanXFS CI](https://github.com/MicaNT-Kernel/TitanXFS/actions/workflows/ci.yml/badge.svg)](https://github.com/MicaNT-Kernel/TitanXFS/actions/workflows/ci.yml)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Standard: Clean-Room 17 U.S.C. § 1201(f)](https://img.shields.io/badge/Clean--Room-17%20U.S.C.%20§%201201(f)-green.svg)](CLEAN_ROOM_PROVENANCE.md)

![TitanXFS Hero Banner](assets/titanxfs_hero.jpg)

**TitanXFS** is an independent, clean-room ISO C++23 Silicon Graphics XFS storage engine, WinFsp user-mode filesystem driver, and background management daemon for Windows NT, MicaNT, and POSIX platforms.

Designed strictly under **17 U.S.C. § 1201(f)** and ***Google v. Oracle*** interoperability standards, TitanXFS delivers native XFS v5 volume formatting, userland File Explorer mounting via WinFsp, B+ tree extent resolution, Allocation Group (AG) telemetry, and background consistency scrubbing without third-party kernel drivers or blue screen (BSOD) risks.

---

### Key Capabilities

- **Red Hat Enterprise Linux (RHEL) & CentOS Compatibility**: Seamlessly mount, browse, read, and verify disks formatted with RHEL, Rocky Linux, AlmaLinux, CentOS, Fedora, or SGI IRIX directly in Windows.
- **WinFsp User-Mode Driver Bridge (`titanxfs_mount`)**: Mount any XFS disk image, raw partition, or physical drive to a native Windows drive letter (e.g. `X:\`) with full integration into Windows File Explorer, PowerShell, Command Prompt, and WSL. Reports `FileSystemName = L"XFS"`.
- **1-Click Turnkey Installer (`titanxfs_setup.exe`)**: Self-contained installer embedding all binaries, high-resolution hero artwork, and signed WinFsp runtime MSI using LZ4 PE overlay compression.
- **Windows NT Background Daemon (`titanxfs_svc.exe`)**: Autonomous background service supporting boot-time auto-mounts, Named Pipe IPC (`\\.\pipe\titanxfs_ipc`), and scheduled 24-hour integrity scrubbing.
- **CLI Utilities**:
  - `titanxfs_format.exe`: Format disk images or physical drives to standard XFS v5.
  - `titanxfs_check.exe`: Filesystem scrubber, superblock validator, and bit-rot diagnostic tool.
- **Unified Titan Storage Hub Integration**: Full management and telemetry support in [Titan Storage Hub](https://github.com/MicaNT-Kernel/TitanStorageHub).

---

## Prerequisites

To mount XFS volumes as Windows drive letters, **WinFsp** (Windows File System Proxy) is required.

1. **Install WinFsp**:
   - Download and install the latest release from the [WinFsp Official GitHub](https://github.com/winfsp/winfsp/releases), or run the bundled installer in `redist/winfsp-2.1.25156.msi`.
   - Alternatively, install via WinGet:
     ```powershell
     winget install WinFsp.WinFsp
     ```
2. **Administrator Privileges**:
   - Required only when accessing physical drives (e.g., `\\.\PhysicalDrive2`).
   - Mounting regular disk images (`.img`, `.raw`) does **not** require Administrator rights.

---

## Quick Start Guide

### Step 1: Format an XFS Volume

```powershell
# Format a new 512 MB XFS v5 disk image:
titanxfs_format.exe rhel_storage.img --size 512 --label "RhelData"

# Or format a physical partition/drive (Run as Administrator):
titanxfs_format.exe \\.\PhysicalDrive3 --label "Enterprise"
```

### Step 2: Mount to a Windows Drive Letter

```powershell
# Mount disk image to drive X:
titanxfs_mount.exe rhel_storage.img X:

# Mount with custom volume label in read-only mode:
titanxfs_mount.exe rhel_storage.img X: --label "RhelBackup" --readonly

# Mount a physical disk partition (Run as Administrator):
titanxfs_mount.exe \\.\PhysicalDrive3 M: --label "EnterpriseXFS"
```

Once mounted, `X:\` appears immediately in **Windows Explorer**, PowerShell, and Command Prompt. Press **`Ctrl+C`** in the console anytime to cleanly unmount.

---

### Command-Line Tools Reference

#### 1. File System Mounting Bridge (`titanxfs_mount.exe`)
```powershell
Usage: titanxfs_mount <device_or_image_path> <drive_letter:> [options]

Options:
  --label <name>         Set volume label in Windows Explorer (default: disk label)
  --readonly             Mount volume in read-only mode
```

#### 2. File System Formatter (`titanxfs_format.exe`)
```powershell
Usage: titanxfs_format <target_device_or_file> [options]

Options:
  --size <MB>            Specify size in MB when creating a new disk image
  --label <name>         Set volume label (max 12 characters)
  --agcount <count>      Number of Allocation Groups (default: 4)
```

#### 3. Consistency Checker & Scrubber (`titanxfs_check.exe`)
```powershell
titanxfs_check.exe rhel_storage.img
```

#### 4. Background Service Daemon (`titanxfs_svc.exe`)
```powershell
# Install as Windows NT service (Run as Administrator):
titanxfs_svc.exe --install

# Start service:
net start TitanXFS

# Run interactively:
titanxfs_svc.exe --run

# Uninstall:
titanxfs_svc.exe --uninstall
```

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
# Configure CMake with Clang & Ninja
cmake -B build -G "Ninja" -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release

# Build all targets (-j2)
cmake --build build -j2

# Run tests
ctest --test-dir build --output-on-failure
.\build\titanxfs_tests.exe
```

---

## Troubleshooting & FAQ

#### `[-] Error: WinFsp dynamic library (winfsp-x64.dll) was not found on this system.`
- WinFsp is not installed. Download and install WinFsp from [https://winfsp.dev](https://winfsp.dev) or run `winget install WinFsp.WinFsp`.

#### `[-] Error: Failed to open XFS volume at \\.\PhysicalDriveX`
- Windows requires Administrator rights to access raw block devices. Re-launch PowerShell or Command Prompt with **Run as administrator**.

#### `[-] Error: Failed to mount XFS volume to X:`
- The drive letter `X:` is already assigned or in use. Choose another letter (e.g. `M:`, `S:`, `L:`).

---

### License & Compliance

Licensed under the **Apache License, Version 2.0**. All reverse-engineering and format structures adhere strictly to statutory interoperability protections under **17 U.S.C. § 1201(f)** and ***Google LLC v. Oracle America, Inc.***.
