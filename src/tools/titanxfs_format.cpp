#include "titanxfs/xfs_engine.hpp"
#include <iostream>
#include <string>
#include <cstdlib>

void PrintUsage() {
    std::cout << "TitanXFS Format Utility v1.0.0 (Clean-Room SGI XFS Engine for Windows NT)\n";
    std::cout << "Usage: titanxfs_format <target_device_or_file> [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --size <MB>     Volume size in Megabytes (default: 64 MB)\n";
    std::cout << "  --label <name>  Filesystem volume label (default: TitanXFS)\n";
    std::cout << "  --help          Display this help message\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    std::string target = argv[1];
    if (target == "--help" || target == "-h" || target == "/?") {
        PrintUsage();
        return 0;
    }

    uint64_t sizeMb = 64;
    std::string label = "TitanXFS";

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--size" && i + 1 < argc) {
            sizeMb = std::strtoull(argv[++i], nullptr, 10);
            if (sizeMb < 16) sizeMb = 16;
        } else if (arg == "--label" && i + 1 < argc) {
            label = argv[++i];
        } else if (arg.rfind("--", 0) != 0) {
            // Positional fallback
            if (i == 2 && std::isdigit(static_cast<unsigned char>(arg[0]))) {
                sizeMb = std::strtoull(arg.c_str(), nullptr, 10);
                if (sizeMb < 16) sizeMb = 16;
            } else if (i == 3 || (i == 2 && !std::isdigit(static_cast<unsigned char>(arg[0])))) {
                label = arg;
            }
        }
    }

    std::cout << "[+] Formatting target: " << target << "\n";
    std::cout << "[+] Target size: " << sizeMb << " MB\n";
    std::cout << "[+] Volume label: " << label << "\n";

    uint64_t sizeBytes = sizeMb * 1024ULL * 1024ULL;
    if (!titanxfs::XfsEngine::FormatVolume(target, sizeBytes, label)) {
        std::cerr << "[-] Error: Failed to format XFS volume on target: " << target << "\n";
        return 1;
    }

    std::cout << "[+] XFS filesystem successfully initialized!\n";
    std::cout << "[+] Superblock magic: 0x58465342 (XFS v5)\n";
    std::cout << "[+] Inode size: 512 bytes (v3 dynamic dinode)\n";
    return 0;
}
