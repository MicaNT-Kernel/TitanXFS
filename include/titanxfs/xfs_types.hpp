#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>
#include <bit>

namespace titanxfs {

// ============================================================================
// Big-Endian Endianness Conversion Helpers (XFS is big-endian on disk)
// ============================================================================
inline uint16_t Be16ToCpu(uint16_t val) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return std::byteswap(val);
    }
    return val;
}

inline uint32_t Be32ToCpu(uint32_t val) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return std::byteswap(val);
    }
    return val;
}

inline uint64_t Be64ToCpu(uint64_t val) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return std::byteswap(val);
    }
    return val;
}

inline uint16_t CpuToBe16(uint16_t val) noexcept { return Be16ToCpu(val); }
inline uint32_t CpuToBe32(uint32_t val) noexcept { return Be32ToCpu(val); }
inline uint64_t CpuToBe64(uint64_t val) noexcept { return Be64ToCpu(val); }

inline uint32_t Le32ToCpu(uint32_t val) noexcept {
    if constexpr (std::endian::native == std::endian::big) {
        return std::byteswap(val);
    }
    return val;
}
inline uint32_t CpuToLe32(uint32_t val) noexcept { return Le32ToCpu(val); }

// ============================================================================
// Core Constants and Magic Numbers
// ============================================================================
constexpr uint32_t XFS_SB_MAGIC       = 0x58465342U; // "XFSB"
constexpr uint32_t XFS_AGF_MAGIC      = 0x58414746U; // "XAGF"
constexpr uint32_t XFS_AGI_MAGIC      = 0x58414749U; // "XAGI"
constexpr uint32_t XFS_AGFL_MAGIC     = 0x5841474CU; // "XAGL"
constexpr uint16_t XFS_DINODE_MAGIC   = 0x494EU;     // "IN"
constexpr uint32_t XFS_DIR2_BLOCK_MAGIC = 0x58443242U; // "XD2B"
constexpr uint32_t XFS_DIR3_BLOCK_MAGIC = 0x58444233U; // "XDB3"

// Inode formats (di_format)
constexpr uint8_t XFS_DINODE_FMT_DEV   = 0;
constexpr uint8_t XFS_DINODE_FMT_LOCAL = 1; // Immediate data / Shortform dir
constexpr uint8_t XFS_DINODE_FMT_EXTENTS = 2; // Extent list
constexpr uint8_t XFS_DINODE_FMT_BTREE = 3; // B+ tree root

// Inode file modes (POSIX octal)
constexpr uint16_t XFS_S_IFMT   = 0170000;
constexpr uint16_t XFS_S_IFREG  = 0100000;
constexpr uint16_t XFS_S_IFDIR  = 0040000;
constexpr uint16_t XFS_S_IFLNK  = 0120000;
constexpr uint16_t XFS_S_IFCHR  = 0020000;
constexpr uint16_t XFS_S_IFBLK  = 0060000;
constexpr uint16_t XFS_S_IFIFO  = 0010000;
constexpr uint16_t XFS_S_IFSOCK = 0140000;

#pragma pack(push, 1)

// ============================================================================
// XFS Superblock (xfs_sb_t) - Stored in Big-Endian at offset 0 of each AG
// ============================================================================
struct XfsSuperBlock {
    uint32_t sb_magicnum;           // 0x00: 0x58465342 ("XFSB")
    uint32_t sb_blocksize;          // 0x04: Block size in bytes (e.g. 4096)
    uint64_t sb_dblocks;            // 0x08: Total data blocks
    uint64_t sb_rblocks;            // 0x10: Realtime data blocks
    uint64_t sb_rextents;           // 0x18: Realtime extents
    uint8_t  sb_uuid[16];           // 0x20: Filesystem UUID
    uint64_t sb_logstart;           // 0x30: Start block of log
    uint64_t sb_rootino;            // 0x38: Root directory inode number
    uint64_t sb_rbmino;             // 0x40: Realtime bitmap inode
    uint64_t sb_rsumino;            // 0x48: Realtime summary inode
    uint32_t sb_rextsize;           // 0x50: Realtime extent size
    uint32_t sb_agblocks;           // 0x54: Size of an AG in blocks
    uint32_t sb_agcount;            // 0x58: Number of AGs
    uint32_t sb_rbmblocks;          // 0x5C: Realtime bitmap blocks
    uint32_t sb_logblocks;          // 0x60: Log size in blocks
    uint16_t sb_versionnum;         // 0x64: Version / flags
    uint16_t sb_sectsize;           // 0x66: Volume sector size
    uint16_t sb_inodesize;          // 0x68: Inode size in bytes (256 or 512)
    uint16_t sb_inopblock;          // 0x6A: Inodes per block
    char     sb_fname[12];          // 0x6C: Filesystem name / label
    uint8_t  sb_blocklog;           // 0x78: log2(sb_blocksize)
    uint8_t  sb_sectlog;            // 0x79: log2(sb_sectsize)
    uint8_t  sb_inodelog;           // 0x7A: log2(sb_inodesize)
    uint8_t  sb_inopblog;           // 0x7B: log2(sb_inopblock)
    uint8_t  sb_agblklog;           // 0x7C: log2(sb_agblocks)
    uint8_t  sb_rextslog;           // 0x7D: log2(sb_rextents)
    uint8_t  sb_inprogress;         // 0x7E: mkfs in progress flag
    uint8_t  sb_imax_pct;           // 0x7F: Max % of space for inodes
    uint64_t sb_icount;             // 0x80: Total allocated inodes
    uint64_t sb_ifree;              // 0x88: Free inodes count
    uint64_t sb_fdblocks;           // 0x90: Free data blocks count
    uint64_t sb_frextents;          // 0x98: Free realtime extents
    uint64_t sb_uquotino;           // 0xA0: User quota inode
    uint64_t sb_gquotino;           // 0xA8: Group quota inode
    uint16_t sb_qflags;             // 0xB0: Quota flags
    uint8_t  sb_flags;              // 0xB2: Misc flags
    uint8_t  sb_shared_vn;          // 0xB3: Shared version number
    uint32_t sb_inoalignmt;         // 0xB4: Inode chunk alignment
    uint32_t sb_unit;               // 0xB8: Stripe or chunk unit
    uint32_t sb_width;              // 0xBC: Stripe width
    uint8_t  sb_dirblklog;          // 0xC0: Directory block log2
    uint8_t  sb_logsectlog;         // 0xC1: Log sector log2
    uint16_t sb_logsectsize;        // 0xC2: Log sector size
    uint32_t sb_logsunit;           // 0xC4: Log stripe unit
    uint32_t sb_features2;          // 0xC8: Additional features
    uint32_t sb_bad_features2;      // 0xCC: Mask of bad features

    // XFS v5 fields (versionnum has XFS_SB_VERSION_5)
    uint32_t sb_features_compat;    // 0xD0: Read/write compat features
    uint32_t sb_features_ro_compat; // 0xD4: Read-only compat features
    uint32_t sb_features_incompat;  // 0xD8: Read/write incompat features
    uint32_t sb_features_log_incompat;//0xDC: Log incompat features
    uint32_t sb_crc;                // 0xE0: CRC32C checksum of superblock
    uint32_t sb_spino_align;        // 0xE4: Sparse inode alignment
    uint64_t sb_pquotino;           // 0xE8: Project quota inode
    uint64_t sb_lsn;                // 0xF0: Log sequence number
    uint8_t  sb_meta_uuid[16];      // 0xF8: Metadata UUID
};
static_assert(sizeof(XfsSuperBlock) == 0x108, "XfsSuperBlock core header size check");

// ============================================================================
// Allocation Group Free Space Block (xfs_agf_t) - Sector 1 of each AG
// ============================================================================
struct XfsAgf {
    uint32_t agf_magicnum;          // 0x58414746 ("XAGF")
    uint32_t agf_versionnum;        // Version number (1)
    uint32_t agf_seqno;             // AG sequence number
    uint32_t agf_length;            // AG size in blocks
    uint32_t agf_roots[2];          // Btree roots: [0] bno, [1] cnt
    uint32_t agf_spare0;            // Spare
    uint32_t agf_levels[2];         // Btree levels: [0] bno, [1] cnt
    uint32_t agf_spare1;            // Spare
    uint32_t agf_flfirst;           // First free list block
    uint32_t agf_fllast;            // Last free list block
    uint32_t agf_flcount;           // Free list count
    uint32_t agf_freeblks;          // Free data blocks in AG
    uint32_t agf_longest;           // Longest free extent
    uint32_t agf_btreeblks;         // Blocks used by free space btrees
    uint8_t  agf_uuid[16];          // Filesystem UUID (v5)
    uint32_t agf_rmap_root;         // RMAP root (v5)
    uint32_t agf_refcnt_root;       // Refcount root (v5)
    uint32_t agf_rmap_level;        // RMAP level (v5)
    uint32_t agf_refcnt_level;      // Refcount level (v5)
    uint64_t agf_lsn;               // LSN (v5)
    uint32_t agf_crc;               // CRC32C (v5)
};

// ============================================================================
// Allocation Group Inode Block (xfs_agi_t) - Sector 2 of each AG
// ============================================================================
struct XfsAgi {
    uint32_t agi_magicnum;          // 0x58414749 ("XAGI")
    uint32_t agi_versionnum;        // Version (1)
    uint32_t agi_seqno;             // AG sequence number
    uint32_t agi_length;            // AG size in blocks
    uint32_t agi_count;             // Total allocated inodes in AG
    uint32_t agi_root;              // Inode btree root block
    uint32_t agi_level;             // Inode btree level
    uint32_t agi_freecount;         // Free inodes in AG
    uint32_t agi_newino;            // Most recently allocated inode
    uint32_t agi_dirino;            // Inode used for directory
    uint32_t agi_unlinked[64];      // Unlinked inode hash table
    uint8_t  agi_uuid[16];          // Filesystem UUID (v5)
    uint32_t agi_crc;               // CRC32C (v5)
    uint32_t agi_pad32;             // Padding
    uint64_t agi_lsn;               // LSN (v5)
    uint32_t agi_free_root;         // Free inode btree root (v5)
    uint32_t agi_free_level;        // Free inode btree level (v5)
};

// ============================================================================
// On-Disk Inode (xfs_dinode_t) - Stored in Big-Endian (256 or 512 bytes)
// ============================================================================
struct XfsDinode {
    uint16_t di_magic;              // 0x00: 0x494E ("IN")
    uint16_t di_mode;               // 0x02: File mode (POSIX)
    uint8_t  di_version;            // 0x04: Version (1, 2, or 3 for v5)
    uint8_t  di_format;             // 0x05: Format (LOCAL=1, EXTENTS=2, BTREE=3)
    uint16_t di_onlink;             // 0x06: Old link count
    uint32_t di_uid;                // 0x08: User ID
    uint32_t di_gid;                // 0x0C: Group ID
    uint32_t di_nlink;              // 0x10: Link count
    uint16_t di_projid_lo;          // 0x14: Project ID (low)
    uint16_t di_projid_hi;          // 0x16: Project ID (high)
    uint8_t  di_pad[6];             // 0x18: Padding
    uint16_t di_flushiter;          // 0x1E: Flush iteration
    uint32_t di_atime_sec;          // 0x20: Access time sec
    uint32_t di_atime_nsec;         // 0x24: Access time nsec
    uint32_t di_mtime_sec;          // 0x28: Modify time sec
    uint32_t di_mtime_nsec;         // 0x2C: Modify time nsec
    uint32_t di_ctime_sec;          // 0x30: Change time sec
    uint32_t di_ctime_nsec;         // 0x34: Change time nsec
    uint64_t di_size;               // 0x38: File size in bytes
    uint64_t di_nblocks;            // 0x40: Blocks allocated
    uint32_t di_extsize;            // 0x48: Extent size
    uint32_t di_nextents;           // 0x4C: Number of data extents
    uint16_t di_anextents;          // 0x50: Number of attr extents
    uint8_t  di_forkoff;            // 0x52: Attr fork offset
    int8_t   di_aformat;            // 0x53: Attr format
    uint32_t di_dmevmask;           // 0x54: DMIG event mask
    uint16_t di_dmstate;            // 0x58: DMIG state
    uint16_t di_flags;              // 0x5A: Inode flags
    uint32_t di_gen;                // 0x5C: Generation count

    // Version 3 (v5 filesystems) fields
    uint32_t di_next_unlinked;      // 0x60: Next unlinked
    uint32_t di_crc;                // 0x64: CRC32C of inode
    uint64_t di_changecount;        // 0x68: Change count
    uint64_t di_lsn;                // 0x70: LSN
    uint64_t di_flags2;             // 0x78: Additional flags
    uint32_t di_cowextsize;         // 0x80: CoW extent size
    uint8_t  di_pad2[12];           // 0x84: Padding
    uint32_t di_crtime_sec;         // 0x90: Creation time sec
    uint32_t di_crtime_nsec;        // 0x94: Creation time nsec
    uint64_t di_ino;                // 0x98: Inode number
    uint8_t  di_uuid[16];           // 0xA0: Filesystem UUID
    // Data fork starts at offset 0xB0 (176 bytes) for v3 dinode
    // or offset 0x64 (100 bytes) for v2 dinode!
};
static_assert(sizeof(XfsDinode) == 0xB0, "XfsDinode v3 header size check");

// ============================================================================
// B-Tree Extent Record (xfs_bmbt_rec_t) - 128-bit packed extent descriptor
// ============================================================================
struct XfsBmbtRec {
    uint64_t l0; // Big-endian upper 64 bits
    uint64_t l1; // Big-endian lower 64 bits

    void Unpack(uint64_t& outStartoff, uint64_t& outStartblock, uint64_t& outBlockcount) const noexcept {
        uint64_t hi = Be64ToCpu(l0);
        uint64_t lo = Be64ToCpu(l1);
        // Bit 127: flag (unwritten)
        // Bits 126..73 (54 bits): startoff (logical block offset)
        outStartoff = (hi >> 9) & 0x003FFFFFFFFFFFFFULL;
        // Bits 72..21 (52 bits): startblock (physical block)
        outStartblock = ((hi & 0x1FFULL) << 43) | (lo >> 21);
        // Bits 20..0 (21 bits): blockcount
        outBlockcount = lo & 0x1FFFFFULL;
    }

    void Pack(uint64_t startoff, uint64_t startblock, uint64_t blockcount) noexcept {
        uint64_t hi = ((startoff & 0x003FFFFFFFFFFFFFULL) << 9) | ((startblock >> 43) & 0x1FFULL);
        uint64_t lo = ((startblock & 0x0007FFFFFFFFFFFFULL) << 21) | (blockcount & 0x1FFFFFULL);
        l0 = CpuToBe64(hi);
        l1 = CpuToBe64(lo);
    }
};
static_assert(sizeof(XfsBmbtRec) == 16, "XfsBmbtRec must be 16 bytes");

// ============================================================================
// Shortform Directory Header (xfs_dir2_sf_hdr_t)
// ============================================================================
struct XfsDir2SfHdr {
    uint8_t count;          // Count of entries
    uint8_t i8count;        // Count of 8-byte inode numbers (0 for 4-byte ino)
    uint8_t parent[4];      // Parent inode number (4 bytes when i8count==0)
};

// Shortform Directory Entry (4-byte inode)
struct XfsDir2SfEntry {
    uint8_t namelen;
    uint8_t offset[2];      // 2-byte tag offset
    // name follows: char name[namelen]
    // followed by: uint8_t inumber[4]
};

#pragma pack(pop)

struct XfsExtentInfo {
    uint64_t logicalBlock{0};
    uint64_t physicalBlock{0};
    uint64_t blockCount{0};
};

struct XfsDirEntryInfo {
    uint64_t inodeNumber{0};
    std::string name;
    uint8_t fileType{0};
};

struct XfsScrubReport {
    bool isClean{true};
    uint64_t totalBlocks{0};
    uint64_t freeBlocks{0};
    uint64_t totalInodes{0};
    uint64_t freeInodes{0};
    uint64_t scannedBlocks{0};
    uint64_t scannedInodes{0};
    uint32_t errorsDetected{0};
    std::string volumeLabel;
};

} // namespace titanxfs
