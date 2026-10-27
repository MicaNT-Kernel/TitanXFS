#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include "titanxfs/compression.hpp"

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct PayloadFooter {
    uint64_t tocOffset{0};
    uint64_t payloadStart{0};
    char     magic[16]{0};
};
#pragma pack(pop)
static_assert(sizeof(PayloadFooter) == 32, "PayloadFooter must be 32 bytes");

struct PackEntry {
    std::string name;
    std::vector<uint8_t> data;
    uint64_t dataOffset{0};
    uint64_t compSize{0};
    uint64_t uncompSize{0};
    uint32_t isCompressed{0};
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: titanxfs_packager <target_setup_exe> <file1> [file2 ...]\n";
        return 1;
    }

    fs::path targetExePath(argv[1]);
    if (!fs::exists(targetExePath)) {
        std::cerr << "Target setup executable not found: " << targetExePath << "\n";
        return 1;
    }

    // 1. Truncate previous payload if exists
    uint64_t fileSize = fs::file_size(targetExePath);
    if (fileSize >= sizeof(PayloadFooter)) {
        std::ifstream in(targetExePath, std::ios::binary);
        in.seekg(fileSize - sizeof(PayloadFooter), std::ios::beg);
        PayloadFooter oldFooter{};
        in.read(reinterpret_cast<char*>(&oldFooter), sizeof(oldFooter));
        in.close();

        if (std::memcmp(oldFooter.magic, "TITANXFS_BUNDLE", 15) == 0 &&
            oldFooter.payloadStart < fileSize && oldFooter.payloadStart > 0) {
            std::cout << "[Packager] Found previous payload at offset " << oldFooter.payloadStart
                      << ". Truncating executable before repacking...\n";
            fs::resize_file(targetExePath, oldFooter.payloadStart);
            fileSize = oldFooter.payloadStart;
        }
    }

    uint64_t payloadStartOffset = fileSize;
    std::vector<PackEntry> entries;

    for (int i = 2; i < argc; ++i) {
        fs::path p(argv[i]);
        if (!fs::exists(p)) {
            std::cerr << "[Packager] Warning: Skipping missing file: " << p << "\n";
            continue;
        }

        std::ifstream fileIn(p, std::ios::binary);
        if (!fileIn.is_open()) {
            std::cerr << "[Packager] Warning: Cannot open: " << p << "\n";
            continue;
        }

        std::vector<uint8_t> raw((std::istreambuf_iterator<char>(fileIn)), std::istreambuf_iterator<char>());
        fileIn.close();

        PackEntry entry;
        entry.name = p.filename().string();
        entry.uncompSize = raw.size();

        // Compress unless image
        std::string ext = p.extension().string();
        for (auto& c : ext) c = static_cast<char>(tolower(c));

        if (ext == ".jpg" || ext == ".png" || ext == ".zip") {
            entry.isCompressed = 0;
            entry.data = std::move(raw);
            entry.compSize = entry.data.size();
        } else {
            entry.isCompressed = 1;
            titanxfs::CompressionEngine::CompressLZ4(raw, entry.data);
            entry.compSize = entry.data.size();
        }

        entries.push_back(std::move(entry));
    }

    if (entries.empty()) {
        std::cerr << "[Packager] Error: No files to package.\n";
        return 1;
    }

    std::ofstream out(targetExePath, std::ios::binary | std::ios::app);
    if (!out.is_open()) {
        std::cerr << "[Packager] Error: Failed to open target executable for appending: " << targetExePath << "\n";
        return 1;
    }

    std::cout << "[Packager] Packaging payload into \"" << targetExePath.filename().string()
              << "\" starting at offset " << payloadStartOffset << "...\n";

    uint64_t currentOffset = payloadStartOffset;
    for (auto& entry : entries) {
        entry.dataOffset = currentOffset;
        out.write(reinterpret_cast<const char*>(entry.data.data()), entry.data.size());
        currentOffset += entry.data.size();

        std::cout << "  + Packed: " << entry.name << " (" << (entry.uncompSize / 1024)
                  << " KB -> " << (entry.compSize / 1024) << " KB"
                  << (entry.isCompressed ? " LZ4)" : " uncompressed)") << "\n";
    }

    uint64_t tocOffset = currentOffset;

    // Table of contents
    uint32_t entryCount = static_cast<uint32_t>(entries.size());
    out.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));
    for (const auto& entry : entries) {
        uint32_t nameLen = static_cast<uint32_t>(entry.name.size());
        out.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
        out.write(entry.name.data(), nameLen);
        out.write(reinterpret_cast<const char*>(&entry.dataOffset), sizeof(entry.dataOffset));
        out.write(reinterpret_cast<const char*>(&entry.compSize), sizeof(entry.compSize));
        out.write(reinterpret_cast<const char*>(&entry.uncompSize), sizeof(entry.uncompSize));
        out.write(reinterpret_cast<const char*>(&entry.isCompressed), sizeof(entry.isCompressed));
    }

    // 32-byte Footer
    PayloadFooter footer{};
    footer.tocOffset = tocOffset;
    footer.payloadStart = payloadStartOffset;
    std::memcpy(footer.magic, "TITANXFS_BUNDLE\0", 16);
    out.write(reinterpret_cast<const char*>(&footer), sizeof(footer));

    out.flush();
    uint64_t finalSize = out.tellp();
    out.close();

    std::cout << "[Packager Success] Complete! Standalone installer size: " << (finalSize / 1024)
              << " KB (Payload: " << ((finalSize - payloadStartOffset) / 1024) << " KB)\n";
    return 0;
}

