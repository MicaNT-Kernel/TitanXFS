#include "titanxfs/xfs_engine.hpp"
#include "titanxfs/compression.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <vector>
#include <string>

namespace fs = std::filesystem;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[-] FAILED: " << msg << " (" << #cond << ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
            std::exit(1); \
        } else { \
            std::cout << "[+] PASSED: " << msg << "\n"; \
        } \
    } while (0)

void TestCrc32c() {
    std::cout << "\n=== TestCrc32c ===\n";
    const char* testStr = "123456789";
    uint32_t crc = titanxfs::Crc32c::Calculate(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(testStr), 9));
    TEST_ASSERT(crc == 0xE3069283U, "CRC32C standard test vector (123456789)");

    uint32_t emptyCrc = titanxfs::Crc32c::Calculate({});
    TEST_ASSERT(emptyCrc == 0x00000000U, "CRC32C empty vector");
}

void TestCompression() {
    std::cout << "\n=== TestCompression ===\n";
    std::string text = "TitanXFS Enterprise Storage Suite for Windows NT and MicaNT. Extreme speed and scalability!";
    std::vector<uint8_t> src;
    for (int i = 0; i < 50; ++i) src.insert(src.end(), text.begin(), text.end());

    std::vector<uint8_t> comp;
    size_t cSize = titanxfs::CompressionEngine::CompressLZ4(src, comp);
    TEST_ASSERT(cSize > 0 && cSize < src.size(), "LZ4 compressed size smaller than source");

    std::vector<uint8_t> decomp(src.size(), 0);
    bool ok = titanxfs::CompressionEngine::DecompressLZ4(comp, decomp);
    TEST_ASSERT(ok, "LZ4 decompression succeeded");
    TEST_ASSERT(decomp == src, "LZ4 decompressed matches original");
}

void TestBmbtExtentPacking() {
    std::cout << "\n=== TestBmbtExtentPacking ===\n";
    titanxfs::XfsBmbtRec rec{};
    uint64_t startoff = 100;
    uint64_t startblock = 50000;
    uint64_t blockcount = 16;

    rec.Pack(startoff, startblock, blockcount);

    uint64_t outOff = 0, outBlock = 0, outCount = 0;
    rec.Unpack(outOff, outBlock, outCount);

    TEST_ASSERT(outOff == startoff, "BMBT startoff unpacked matches");
    TEST_ASSERT(outBlock == startblock, "BMBT startblock unpacked matches");
    TEST_ASSERT(outCount == blockcount, "BMBT blockcount unpacked matches");
}

void TestXfsFormatAndMount() {
    std::cout << "\n=== TestXfsFormatAndMount ===\n";
    std::string imgPath = "test_xfs.img";
    if (fs::exists(imgPath)) fs::remove(imgPath);

    uint64_t sizeBytes = 64ULL * 1024ULL * 1024ULL;
    bool fmtOk = titanxfs::XfsEngine::FormatVolume(imgPath, sizeBytes, "TitanXfsTest");
    TEST_ASSERT(fmtOk, "FormatVolume succeeded");
    TEST_ASSERT(fs::exists(imgPath), "Image file created on disk");

    titanxfs::XfsEngine engine;
    bool openOk = engine.Open(imgPath, true);
    TEST_ASSERT(openOk, "Engine Open succeeded");
    TEST_ASSERT(engine.IsOpen(), "Engine reports open state");
    TEST_ASSERT(engine.BlockSize() == 4096, "Block size is 4096 bytes");
    TEST_ASSERT(engine.VolumeLabel() == "TitanXfsTest", "Volume label matches");
    TEST_ASSERT(titanxfs::Be32ToCpu(engine.SuperBlock().sb_magicnum) == titanxfs::XFS_SB_MAGIC, "Superblock magic is 0x58465342");
    TEST_ASSERT(engine.InodeSize() == 512, "Inode size is 512 bytes");
    TEST_ASSERT(engine.RootInode() == 128, "Root inode is Inode 128");

    // Read Root Inode
    titanxfs::XfsDinode rootDinode{};
    std::vector<uint8_t> fork;
    bool inoOk = engine.ReadInode(engine.RootInode(), rootDinode, fork);
    TEST_ASSERT(inoOk, "ReadInode 128 (Root) succeeded");
    uint16_t mode = titanxfs::Be16ToCpu(rootDinode.di_mode);
    TEST_ASSERT((mode & titanxfs::XFS_S_IFMT) == titanxfs::XFS_S_IFDIR, "Root inode is a directory");

    // Directory Traversal
    std::vector<titanxfs::XfsDirEntryInfo> dirEntries;
    bool dirOk = engine.ReadDirectory(engine.RootInode(), dirEntries);
    TEST_ASSERT(dirOk, "ReadDirectory succeeded on Root directory");
    TEST_ASSERT(dirEntries.size() >= 2, "Directory has at least '.' and '..'");

    bool foundDot = false, foundDotDot = false;
    for (const auto& ent : dirEntries) {
        if (ent.name == ".") foundDot = true;
        if (ent.name == "..") foundDotDot = true;
    }
    TEST_ASSERT(foundDot && foundDotDot, "Root directory contains '.' and '..'");

    // Path Resolution
    uint64_t resolvedIno = 0;
    bool resRoot = engine.ResolvePath("/", resolvedIno);
    TEST_ASSERT(resRoot && resolvedIno == engine.RootInode(), "ResolvePath '/' returns Root Inode 128");

    // Integrity Scrubber
    titanxfs::XfsScrubReport report{};
    bool scrubOk = engine.CheckAndScrub(report);
    TEST_ASSERT(scrubOk, "CheckAndScrub completed successfully");
    TEST_ASSERT(report.isClean, "Scrubber reports volume is clean");
    TEST_ASSERT(report.errorsDetected == 0, "Zero errors detected during scrub");

    engine.Close();
    if (fs::exists(imgPath)) fs::remove(imgPath);
}

int main() {
    std::cout << "=================================================\n";
    std::cout << "         TitanXFS Core Unit Test Battery         \n";
    std::cout << "=================================================\n";

    TestCrc32c();
    TestCompression();
    TestBmbtExtentPacking();
    TestXfsFormatAndMount();

    std::cout << "\n-------------------------------------------------\n";
    std::cout << " ALL UNIT TESTS COMPLETED SUCCESSFULLY (0 ERRORS)\n";
    std::cout << "-------------------------------------------------\n";
    return 0;
}
