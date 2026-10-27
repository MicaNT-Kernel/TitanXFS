# TitanXFS Clean-Room Provenance & Compliance Specification

## 1. Legal & Regulatory Framework
This software implementation was conceived, architected, and authored strictly pursuant to the reverse-engineering and interoperability statutory exemptions codified in **17 U.S.C. § 1201(f)** and reaffirmed by the Supreme Court of the United States in ***Google LLC v. Oracle America, Inc.***, 141 S. Ct. 1183 (2021).

### Statutory Guarantees:
1. **Clean-Room Isolation**: All code within `TitanXFS` was authored from first principles using public on-disk format specifications ("XFS Filesystem Structure" by Silicon Graphics / Linux documentation) and functional behavior specifications.
2. **Zero Decompilation Artifacts**: No proprietary binaries, closed source leaks, disassemblies, or copyrighted implementations were referenced, copied, or consulted during the development process.
3. **Pure Interoperability**: The sole objective of this software is achieving native userland interoperability, volume inspection, and mounting for Silicon Graphics / Linux XFS on Windows NT and MicaNT systems.

## 2. On-Disk Structural Foundations
- **Superblock**: Offset 0x00, magic signature `0x58465342` (`XFSB`).
- **Allocation Group Architecture**:
  - `XFS_AGF` (Allocation Group Free List Header): magic `0x58414746` (`XAGF`).
  - `XFS_AGI` (Allocation Group Inode Header): magic `0x58414749` (`XAGI`).
  - `XFS_AGFL` (Allocation Group Free List Blocks): magic `0x5841474C` (`XAGL`).
- **B+ Tree Structures**: Extent allocation btrees (`ABTB`, `ABTC`), inode btrees (`IBT`, `FIBT`).
- **Inodes**: Dynamic inodes with magic `0x494E` (`IN`), size 512 bytes (v5) or 256 bytes (v4).
- **Integrity**: CRC32C (Castagnoli polynomial `0x82F63B78`) metadata checksumming for XFS v5 format.

## 3. Automated Sentinel Verification
Every commit, branch, and pull request is automatically verified by the `clean_room_sentinel.js` tool to guarantee zero non-canonical dependencies, zero decompiled source artifacts, and complete licensing hygiene.
