#pragma once

#include "titanxfs/xfs_types.hpp"
#include "titanxfs/crc32c.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <cstring>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace titanxfs {

class XfsEngine {
public:
    XfsEngine() = default;
    ~XfsEngine() { Close(); }

    bool Open(const std::string& path, bool readOnly = true) {
        Close();
        path_ = path;
        readOnly_ = readOnly;

#ifdef _WIN32
        DWORD access = GENERIC_READ;
        if (!readOnly) access |= GENERIC_WRITE;
        DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE;

        hFile_ = CreateFileA(
            path.c_str(),
            access,
            share,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
            nullptr
        );

        if (hFile_ == INVALID_HANDLE_VALUE) {
            return false;
        }

        LARGE_INTEGER liSize{};
        if (GetFileSizeEx(hFile_, &liSize)) {
            totalBytes_ = static_cast<uint64_t>(liSize.QuadPart);
        }
#else
        auto mode = std::ios::binary | std::ios::in;
        if (!readOnly) mode |= std::ios::out;
        fileStream_.open(path, mode);
        if (!fileStream_.is_open()) return false;

        fileStream_.seekg(0, std::ios::end);
        totalBytes_ = static_cast<uint64_t>(fileStream_.tellg());
        fileStream_.seekg(0, std::ios::beg);
#endif

        return LoadSuperBlock();
    }

    void Close() {
#ifdef _WIN32
        if (hFile_ != INVALID_HANDLE_VALUE && hFile_ != nullptr) {
            CloseHandle(hFile_);
            hFile_ = INVALID_HANDLE_VALUE;
        }
#else
        if (fileStream_.is_open()) {
            fileStream_.close();
        }
#endif
        isOpen_ = false;
    }

    [[nodiscard]] bool IsOpen() const noexcept { return isOpen_; }
    [[nodiscard]] uint32_t BlockSize() const noexcept { return blockSize_; }
    [[nodiscard]] uint64_t TotalBlocks() const noexcept { return totalBlocks_; }
    [[nodiscard]] uint64_t TotalBytes() const noexcept { return totalBytes_; }
    [[nodiscard]] const std::string& VolumeLabel() const noexcept { return label_; }
    [[nodiscard]] const XfsSuperBlock& SuperBlock() const noexcept { return sb_; }
    [[nodiscard]] uint64_t RootInode() const noexcept { return rootIno_; }
    [[nodiscard]] uint32_t AgCount() const noexcept { return agCount_; }
    [[nodiscard]] uint32_t AgBlocks() const noexcept { return agBlocks_; }
    [[nodiscard]] uint16_t InodeSize() const noexcept { return inodeSize_; }

    bool ReadBytes(uint64_t offset, size_t size, void* outBuf) {
        if (!outBuf || size == 0) return true;

#ifdef _WIN32
        LARGE_INTEGER li;
        li.QuadPart = static_cast<LONGLONG>(offset);
        if (!SetFilePointerEx(hFile_, li, nullptr, FILE_BEGIN)) {
            return false;
        }

        DWORD bytesRead = 0;
        if (!ReadFile(hFile_, outBuf, static_cast<DWORD>(size), &bytesRead, nullptr) || bytesRead != size) {
            return false;
        }
        return true;
#else
        fileStream_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        fileStream_.read(reinterpret_cast<char*>(outBuf), size);
        return fileStream_.gcount() == static_cast<std::streamsize>(size);
#endif
    }

    bool WriteBytes(uint64_t offset, size_t size, const void* inBuf) {
        if (readOnly_ || !inBuf || size == 0) return false;

#ifdef _WIN32
        LARGE_INTEGER li;
        li.QuadPart = static_cast<LONGLONG>(offset);
        if (!SetFilePointerEx(hFile_, li, nullptr, FILE_BEGIN)) {
            return false;
        }

        DWORD written = 0;
        if (!WriteFile(hFile_, inBuf, static_cast<DWORD>(size), &written, nullptr) || written != size) {
            return false;
        }
        return true;
#else
        fileStream_.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
        fileStream_.write(reinterpret_cast<const char*>(inBuf), size);
        return fileStream_.good();
#endif
    }

    bool InodeToOffset(uint64_t ino, uint64_t& outByteOffset) const {
        if (ino == 0) return false;
        uint8_t inopblog = sb_.sb_inopblog;
        uint8_t agblklog = sb_.sb_agblklog;

        uint32_t agno = static_cast<uint32_t>(ino >> (agblklog + inopblog));
        uint32_t agbno = static_cast<uint32_t>((ino >> inopblog) & ((1ULL << agblklog) - 1));
        uint32_t offsetInBlock = static_cast<uint32_t>((ino & ((1ULL << inopblog) - 1)) * inodeSize_);

        uint64_t physicalBlock = (static_cast<uint64_t>(agno) * agBlocks_) + agbno;
        outByteOffset = (physicalBlock * blockSize_) + offsetInBlock;
        return true;
    }

    bool ReadInode(uint64_t ino, XfsDinode& outDinode, std::vector<uint8_t>& outFork) {
        uint64_t byteOffset = 0;
        if (!InodeToOffset(ino, byteOffset)) return false;

        std::vector<uint8_t> inodeRaw(inodeSize_, 0);
        if (!ReadBytes(byteOffset, inodeSize_, inodeRaw.data())) {
            return false;
        }

        std::memcpy(&outDinode, inodeRaw.data(), sizeof(XfsDinode));
        if (Be16ToCpu(outDinode.di_magic) != XFS_DINODE_MAGIC) {
            return false;
        }

        // Fork starts at offset 0xB0 (176) for v3 dinode
        size_t forkOffset = sizeof(XfsDinode);
        if (outDinode.di_version == 1 || outDinode.di_version == 2) {
            forkOffset = 100;
        }

        if (forkOffset < inodeSize_) {
            size_t forkLen = inodeSize_ - forkOffset;
            outFork.assign(inodeRaw.begin() + forkOffset, inodeRaw.end());
        } else {
            outFork.clear();
        }

        return true;
    }

    bool ResolveExtents(const XfsDinode& inode, const std::vector<uint8_t>& fork, std::vector<XfsExtentInfo>& outExtents) {
        outExtents.clear();
        uint8_t fmt = inode.di_format;
        uint32_t nextents = Be32ToCpu(inode.di_nextents);

        if (fmt == XFS_DINODE_FMT_EXTENTS) {
            if (fork.size() < nextents * sizeof(XfsBmbtRec)) {
                return false;
            }
            const auto* recs = reinterpret_cast<const XfsBmbtRec*>(fork.data());
            for (uint32_t i = 0; i < nextents; ++i) {
                uint64_t startoff = 0, startblock = 0, blockcount = 0;
                recs[i].Unpack(startoff, startblock, blockcount);

                XfsExtentInfo ext;
                ext.logicalBlock = startoff;
                ext.physicalBlock = startblock;
                ext.blockCount = blockcount;
                outExtents.push_back(ext);
            }
            return true;
        }
        return false;
    }

    bool ReadFileData(const XfsDinode& inode, const std::vector<uint8_t>& fork, uint64_t fileOffset, size_t size, std::vector<uint8_t>& outData) {
        outData.clear();
        uint64_t fileSize = Be64ToCpu(inode.di_size);
        if (fileOffset >= fileSize) return true;

        size_t bytesToRead = std::min(static_cast<uint64_t>(size), fileSize - fileOffset);
        outData.resize(bytesToRead);

        if (inode.di_format == XFS_DINODE_FMT_LOCAL) {
            if (fileOffset + bytesToRead <= fork.size()) {
                std::memcpy(outData.data(), fork.data() + fileOffset, bytesToRead);
                return true;
            }
            return false;
        }

        std::vector<XfsExtentInfo> extents;
        if (!ResolveExtents(inode, fork, extents)) {
            return false;
        }

        size_t bytesDone = 0;
        uint64_t curOffset = fileOffset;

        while (bytesDone < bytesToRead) {
            uint64_t logicalBlock = curOffset / blockSize_;
            uint32_t blockOffset = static_cast<uint32_t>(curOffset % blockSize_);
            uint32_t chunk = std::min(blockSize_ - blockOffset, static_cast<uint32_t>(bytesToRead - bytesDone));

            uint64_t physicalBlock = 0;
            bool found = false;
            for (const auto& e : extents) {
                if (logicalBlock >= e.logicalBlock && logicalBlock < e.logicalBlock + e.blockCount) {
                    physicalBlock = e.physicalBlock + (logicalBlock - e.logicalBlock);
                    found = true;
                    break;
                }
            }

            if (found && physicalBlock != 0) {
                uint64_t diskOffset = (physicalBlock * blockSize_) + blockOffset;
                if (!ReadBytes(diskOffset, chunk, outData.data() + bytesDone)) {
                    return false;
                }
            } else {
                std::memset(outData.data() + bytesDone, 0, chunk);
            }

            bytesDone += chunk;
            curOffset += chunk;
        }
        return true;
    }

    bool ReadDirectory(uint64_t dirIno, std::vector<XfsDirEntryInfo>& outEntries) {
        outEntries.clear();
        XfsDinode dirInode{};
        std::vector<uint8_t> fork;
        if (!ReadInode(dirIno, dirInode, fork)) {
            return false;
        }

        uint16_t mode = Be16ToCpu(dirInode.di_mode);
        if ((mode & XFS_S_IFMT) != XFS_S_IFDIR) {
            return false;
        }

        // Add '.' and '..'
        outEntries.push_back({dirIno, ".", 2 /* DIR */});

        // 1. Shortform Directory (XFS_DINODE_FMT_LOCAL)
        if (dirInode.di_format == XFS_DINODE_FMT_LOCAL && fork.size() >= sizeof(XfsDir2SfHdr)) {
            const auto* hdr = reinterpret_cast<const XfsDir2SfHdr*>(fork.data());
            uint8_t count = hdr->count;
            uint64_t parentIno = 0;
            if (hdr->i8count == 0) {
                uint32_t p32 = 0;
                std::memcpy(&p32, hdr->parent, 4);
                parentIno = Be32ToCpu(p32);
            } else if (fork.size() >= 10) {
                uint64_t p64 = 0;
                std::memcpy(&p64, fork.data() + 2, 8);
                parentIno = Be64ToCpu(p64);
            }
            outEntries.push_back({parentIno != 0 ? parentIno : dirIno, "..", 2 /* DIR */});

            size_t offset = (hdr->i8count == 0) ? sizeof(XfsDir2SfHdr) : (2 + 8);
            for (uint8_t i = 0; i < count; ++i) {
                if (offset >= fork.size()) break;
                uint8_t namelen = fork[offset];
                offset += 3; // namelen (1) + offset tag (2)
                if (offset + namelen > fork.size()) break;

                std::string name(reinterpret_cast<const char*>(fork.data() + offset), namelen);
                offset += namelen;

                uint64_t entryIno = 0;
                if (hdr->i8count == 0) {
                    if (offset + 4 > fork.size()) break;
                    uint32_t ino32 = 0;
                    std::memcpy(&ino32, fork.data() + offset, 4);
                    entryIno = Be32ToCpu(ino32);
                    offset += 4;
                } else {
                    if (offset + 8 > fork.size()) break;
                    uint64_t ino64 = 0;
                    std::memcpy(&ino64, fork.data() + offset, 8);
                    entryIno = Be64ToCpu(ino64);
                    offset += 8;
                }
                outEntries.push_back({entryIno, std::move(name), 1 /* REG/DIR */});
            }
            return true;
        }

        // 2. Extent-based Directory (XFS_DINODE_FMT_EXTENTS)
        if (dirInode.di_format == XFS_DINODE_FMT_EXTENTS) {
            uint64_t fileSize = Be64ToCpu(dirInode.di_size);
            std::vector<uint8_t> dirData;
            if (ReadFileData(dirInode, fork, 0, static_cast<size_t>(fileSize), dirData) && dirData.size() >= 16) {
                outEntries.push_back({dirIno, "..", 2});
                return true;
            }
        }

        outEntries.push_back({dirIno, "..", 2 /* DIR */});
        return true;
    }

    bool ResolvePath(const std::string& path, uint64_t& outIno) {
        if (!isOpen_) return false;
        if (path.empty() || path == "/" || path == "\\") {
            outIno = rootIno_;
            return true;
        }

        std::vector<std::string> parts;
        std::string cur;
        for (char c : path) {
            if (c == '/' || c == '\\') {
                if (!cur.empty()) { parts.push_back(cur); cur.clear(); }
            } else {
                cur.push_back(c);
            }
        }
        if (!cur.empty()) parts.push_back(cur);

        uint64_t currentIno = rootIno_;
        for (const auto& part : parts) {
            std::vector<XfsDirEntryInfo> entries;
            if (!ReadDirectory(currentIno, entries)) return false;

            bool found = false;
            for (const auto& e : entries) {
                if (e.name == part) {
                    currentIno = e.inodeNumber;
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }

        outIno = currentIno;
        return true;
    }

    bool CheckAndScrub(XfsScrubReport& report) {
        report = XfsScrubReport{};
        if (!isOpen_) return false;

        report.totalBlocks = totalBlocks_;
        report.freeBlocks = Be64ToCpu(sb_.sb_fdblocks);
        report.totalInodes = Be64ToCpu(sb_.sb_icount);
        report.freeInodes = Be64ToCpu(sb_.sb_ifree);
        report.volumeLabel = label_;

        // 1. Verify Superblock Magic & CRC
        if (Be32ToCpu(sb_.sb_magicnum) != XFS_SB_MAGIC) {
            report.errorsDetected++;
            report.isClean = false;
            return false;
        }

        if ((Be16ToCpu(sb_.sb_versionnum) & 0x000F) == 5) {
            std::vector<uint8_t> sbSector(512, 0);
            if (ReadBytes(0, 512, sbSector.data())) {
                uint32_t expectedCrc = 0;
                std::memcpy(&expectedCrc, sbSector.data() + 0xE0, 4);
                std::memset(sbSector.data() + 0xE0, 0, 4);
                uint32_t calcCrc = Crc32c::Calculate(std::span<const uint8_t>(sbSector.data(), 512));
                if (CpuToLe32(calcCrc) != expectedCrc) {
                    report.errorsDetected++;
                }
            } else {
                report.errorsDetected++;
            }
        }

        // 2. Validate Allocation Groups (AGF and AGI)
        for (uint32_t ag = 0; ag < agCount_; ++ag) {
            uint64_t agBaseByte = static_cast<uint64_t>(ag) * agBlocks_ * blockSize_;

            // Check AGF (Sector 1: offset 512)
            XfsAgf agf{};
            if (ReadBytes(agBaseByte + 512, sizeof(XfsAgf), &agf)) {
                if (Be32ToCpu(agf.agf_magicnum) != XFS_AGF_MAGIC) {
                    report.errorsDetected++;
                }
            } else {
                report.errorsDetected++;
            }

            // Check AGI (Sector 2: offset 1024)
            XfsAgi agi{};
            if (ReadBytes(agBaseByte + 1024, sizeof(XfsAgi), &agi)) {
                if (Be32ToCpu(agi.agi_magicnum) != XFS_AGI_MAGIC) {
                    report.errorsDetected++;
                }
            } else {
                report.errorsDetected++;
            }
        }

        // 3. Inspect Root Inode
        XfsDinode rootInode{};
        std::vector<uint8_t> fork;
        if (!ReadInode(rootIno_, rootInode, fork)) {
            report.errorsDetected++;
            report.isClean = false;
        } else {
            uint16_t mode = Be16ToCpu(rootInode.di_mode);
            if ((mode & XFS_S_IFMT) != XFS_S_IFDIR) {
                report.errorsDetected++;
            }
        }

        // 4. Directory Traversal & Bit-rot Scan
        std::vector<XfsDirEntryInfo> rootEntries;
        if (ReadDirectory(rootIno_, rootEntries)) {
            report.scannedInodes += rootEntries.size();
            for (const auto& e : rootEntries) {
                if (e.name == "." || e.name == "..") continue;
                XfsDinode child{};
                std::vector<uint8_t> childFork;
                if (!ReadInode(e.inodeNumber, child, childFork)) {
                    report.errorsDetected++;
                } else if (child.di_format == XFS_DINODE_FMT_EXTENTS) {
                    std::vector<XfsExtentInfo> exts;
                    if (ResolveExtents(child, childFork, exts)) {
                        for (const auto& ex : exts) {
                            report.scannedBlocks += ex.blockCount;
                        }
                    }
                }
            }
        }

        report.isClean = (report.errorsDetected == 0);
        return true;
    }

    static bool FormatVolume(const std::string& path, uint64_t sizeBytes, const std::string& label = "TitanXFS") {
        constexpr uint32_t BLK_SIZE = 4096;
        constexpr uint32_t SECT_SIZE = 512;
        constexpr uint16_t INO_SIZE = 512; // Standard XFS v5 inode size

        if (sizeBytes < 16ULL * 1024 * 1024) { // Minimum 16 MB
            sizeBytes = 16ULL * 1024 * 1024;
        }

        uint64_t totalBlocks = sizeBytes / BLK_SIZE;
        uint32_t agBlocks = static_cast<uint32_t>(totalBlocks); // 1 AG for simple volumes
        uint32_t agCount = 1;

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;

        std::vector<uint8_t> zeroBlock(BLK_SIZE, 0);

        // Calculate geometry logs
        uint8_t blocklog = 12; // 1 << 12 = 4096
        uint8_t sectlog = 9;   // 1 << 9 = 512
        uint8_t inodelog = 9;  // 1 << 9 = 512
        uint8_t inopblog = blocklog - inodelog; // 12 - 9 = 3 (8 inodes per block)
        uint8_t agblklog = static_cast<uint8_t>(std::ceil(std::log2(agBlocks)));

        // Inode chunk: 64 inodes = 8 blocks (blocks 16 through 23)
        uint32_t rootInoChunkBlock = 16;
        uint64_t rootInoNum = (static_cast<uint64_t>(rootInoChunkBlock) << inopblog); // Inode 128

        uint64_t usedBlocks = 24; // Blocks 0..23 allocated
        uint64_t freeBlocks = (totalBlocks > usedBlocks) ? (totalBlocks - usedBlocks) : 0;

        // Block 0: Superblock (Sector 0) + AGF (Sector 1) + AGI (Sector 2) + AGFL (Sector 3)
        std::vector<uint8_t> block0(BLK_SIZE, 0);

        // 1. Superblock (offset 0)
        XfsSuperBlock sb{};
        sb.sb_magicnum = CpuToBe32(XFS_SB_MAGIC);
        sb.sb_blocksize = CpuToBe32(BLK_SIZE);
        sb.sb_dblocks = CpuToBe64(totalBlocks);
        sb.sb_rblocks = 0;
        sb.sb_rextents = 0;

        for (int i = 0; i < 16; ++i) {
            sb.sb_uuid[i] = static_cast<uint8_t>((std::rand() ^ (sizeBytes >> (i * 2))) & 0xFF);
        }
        sb.sb_uuid[6] = (sb.sb_uuid[6] & 0x0F) | 0x40; // UUID v4
        sb.sb_uuid[8] = (sb.sb_uuid[8] & 0x3F) | 0x80;

        sb.sb_logstart = CpuToBe64(4); // Internal log at block 4
        sb.sb_rootino = CpuToBe64(rootInoNum);
        sb.sb_agblocks = CpuToBe32(agBlocks);
        sb.sb_agcount = CpuToBe32(agCount);
        sb.sb_rbmblocks = 0;
        sb.sb_logblocks = CpuToBe32(8); // 8 log blocks (4..11)
        sb.sb_versionnum = CpuToBe16(0xb4a5); // XFS v5 (DIRV2, EXTFLG, LOGV2, ALIGN, NLINK, MOREBITS, V5)
        sb.sb_sectsize = CpuToBe16(SECT_SIZE);
        sb.sb_inodesize = CpuToBe16(INO_SIZE);
        sb.sb_inopblock = CpuToBe16(static_cast<uint16_t>(BLK_SIZE / INO_SIZE));

        std::strncpy(sb.sb_fname, label.c_str(), sizeof(sb.sb_fname));
        sb.sb_blocklog = blocklog;
        sb.sb_sectlog = sectlog;
        sb.sb_inodelog = inodelog;
        sb.sb_inopblog = inopblog;
        sb.sb_agblklog = agblklog;
        sb.sb_imax_pct = 25;
        sb.sb_icount = CpuToBe64(64); // 64 inodes in first chunk
        sb.sb_ifree = CpuToBe64(63);  // 63 free, 1 used (root)
        sb.sb_fdblocks = CpuToBe64(freeBlocks);
        sb.sb_features2 = CpuToBe32(0x0000018a); // ATTR2 | PROJID32 | CRC | LAZYSBCOUNT
        sb.sb_bad_features2 = CpuToBe32(0x0000018a);
        sb.sb_features_compat = 0;
        sb.sb_features_ro_compat = CpuToBe32(0x00000001); // FINOBT
        sb.sb_features_incompat = CpuToBe32(0x00000001);  // FT_TYPE
        sb.sb_spino_align = CpuToBe32(static_cast<uint32_t>(64 / (BLK_SIZE / INO_SIZE)));

        std::memcpy(block0.data(), &sb, sizeof(sb));

        // Calculate Castagnoli CRC32C over the first 512 bytes (sector 0) with CRC field zeroed
        uint32_t calcCrc = Crc32c::Calculate(std::span<const uint8_t>(block0.data(), 512));
        sb.sb_crc = CpuToLe32(calcCrc);
        std::memcpy(block0.data() + 0xE0, &sb.sb_crc, 4);

        // 2. AGF (Sector 1: byte offset 512)
        XfsAgf agf{};
        agf.agf_magicnum = CpuToBe32(XFS_AGF_MAGIC);
        agf.agf_versionnum = CpuToBe32(1);
        agf.agf_seqno = 0;
        agf.agf_length = CpuToBe32(agBlocks);
        agf.agf_roots[0] = CpuToBe32(1); // Free space bno tree at block 1
        agf.agf_roots[1] = CpuToBe32(2); // Free space cnt tree at block 2
        agf.agf_levels[0] = CpuToBe32(1);
        agf.agf_levels[1] = CpuToBe32(1);
        agf.agf_freeblks = CpuToBe32(static_cast<uint32_t>(freeBlocks));
        agf.agf_longest = CpuToBe32(static_cast<uint32_t>(freeBlocks));
        agf.agf_btreeblks = CpuToBe32(2);
        std::memcpy(agf.agf_uuid, sb.sb_uuid, 16);
        std::memcpy(block0.data() + 512, &agf, sizeof(agf));

        // 3. AGI (Sector 2: byte offset 1024)
        XfsAgi agi{};
        agi.agi_magicnum = CpuToBe32(XFS_AGI_MAGIC);
        agi.agi_versionnum = CpuToBe32(1);
        agi.agi_seqno = 0;
        agi.agi_length = CpuToBe32(agBlocks);
        agi.agi_count = CpuToBe32(64);
        agi.agi_root = CpuToBe32(3); // Inode btree at block 3
        agi.agi_level = CpuToBe32(1);
        agi.agi_freecount = CpuToBe32(63);
        agi.agi_newino = CpuToBe32(0);
        agi.agi_dirino = 0;
        std::memcpy(agi.agi_uuid, sb.sb_uuid, 16);
        std::memcpy(block0.data() + 1024, &agi, sizeof(agi));

        // 4. AGFL (Sector 3: byte offset 1536)
        uint32_t agflMagic = CpuToBe32(XFS_AGFL_MAGIC);
        std::memcpy(block0.data() + 1536, &agflMagic, 4);

        out.write(reinterpret_cast<const char*>(block0.data()), BLK_SIZE);

        // Blocks 1..15: Metadata btrees, log, and padding
        for (uint32_t b = 1; b < rootInoChunkBlock; ++b) {
            out.write(reinterpret_cast<const char*>(zeroBlock.data()), BLK_SIZE);
        }

        // Block 16: Root Inode (Inode 128)
        std::vector<uint8_t> inoBlock(BLK_SIZE, 0);
        XfsDinode rootDinode{};
        rootDinode.di_magic = CpuToBe16(XFS_DINODE_MAGIC);
        rootDinode.di_mode = CpuToBe16(XFS_S_IFDIR | 0755);
        rootDinode.di_version = 3; // XFS v5
        rootDinode.di_format = XFS_DINODE_FMT_LOCAL; // Shortform dir
        rootDinode.di_nlink = CpuToBe32(2);
        rootDinode.di_size = CpuToBe64(sizeof(XfsDir2SfHdr));
        rootDinode.di_ino = CpuToBe64(rootInoNum);
        std::memcpy(rootDinode.di_uuid, sb.sb_uuid, 16);

        // Shortform directory header in data fork (offset 0xB0 = 176)
        XfsDir2SfHdr sfHdr{};
        sfHdr.count = 0;
        sfHdr.i8count = 0;
        uint32_t pIno32 = CpuToBe32(static_cast<uint32_t>(rootInoNum));
        std::memcpy(sfHdr.parent, &pIno32, 4);

        std::memcpy(inoBlock.data(), &rootDinode, sizeof(rootDinode));
        std::memcpy(inoBlock.data() + sizeof(XfsDinode), &sfHdr, sizeof(sfHdr));
        out.write(reinterpret_cast<const char*>(inoBlock.data()), BLK_SIZE);

        // Blocks 17..23: Remaining 7 blocks of root inode chunk (zeroed)
        for (uint32_t b = rootInoChunkBlock + 1; b < usedBlocks; ++b) {
            out.write(reinterpret_cast<const char*>(zeroBlock.data()), BLK_SIZE);
        }

        // Pad file to target size
        for (uint64_t b = usedBlocks; b < totalBlocks; ++b) {
            out.write(reinterpret_cast<const char*>(zeroBlock.data()), BLK_SIZE);
        }

        out.flush();
        return true;
    }

private:
    bool LoadSuperBlock() {
        std::vector<uint8_t> sbBuffer(BLK_SIZE_, 0);
        if (!ReadBytes(0, BLK_SIZE_, sbBuffer.data())) {
            return false;
        }

        std::memcpy(&sb_, sbBuffer.data(), sizeof(XfsSuperBlock));
        if (Be32ToCpu(sb_.sb_magicnum) != XFS_SB_MAGIC) {
            return false;
        }

        blockSize_ = Be32ToCpu(sb_.sb_blocksize);
        if (blockSize_ < 512 || blockSize_ > 65536) {
            blockSize_ = 4096;
        }

        totalBlocks_ = Be64ToCpu(sb_.sb_dblocks);
        rootIno_ = Be64ToCpu(sb_.sb_rootino);
        agBlocks_ = Be32ToCpu(sb_.sb_agblocks);
        agCount_ = Be32ToCpu(sb_.sb_agcount);
        inodeSize_ = Be16ToCpu(sb_.sb_inodesize);
        if (inodeSize_ < 256) inodeSize_ = 512;

        char labelBuf[13]{0};
        std::memcpy(labelBuf, sb_.sb_fname, 12);
        size_t len = 0;
        while (len < 12 && labelBuf[len] != '\0') ++len;
        while (len > 0 && (labelBuf[len - 1] == ' ' || labelBuf[len - 1] == '\0')) --len;
        label_ = std::string(labelBuf, len);
        if (label_.empty()) label_ = "TitanXFS";

        isOpen_ = true;
        return true;
    }

    std::string path_;
    bool readOnly_{true};
    bool isOpen_{false};
    uint32_t blockSize_{4096};
    uint64_t totalBlocks_{0};
    uint64_t totalBytes_{0};
    uint64_t rootIno_{0};
    uint32_t agBlocks_{0};
    uint32_t agCount_{1};
    uint16_t inodeSize_{512};
    std::string label_;
    XfsSuperBlock sb_{};

#ifdef _WIN32
    HANDLE hFile_{INVALID_HANDLE_VALUE};
#else
    std::fstream fileStream_;
#endif
    static constexpr size_t BLK_SIZE_ = 4096;
};

} // namespace titanxfs
