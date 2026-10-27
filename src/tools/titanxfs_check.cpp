#include "titanxfs/xfs_engine.hpp"
#include <iostream>
#include <iomanip>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "TitanXFS Integrity & Bit-Rot Scrubber v1.0.0\n";
        std::cout << "Usage: titanxfs_check <disk_image_or_drive_letter>\n";
        return 1;
    }

    std::string path = argv[1];
    titanxfs::XfsEngine engine;
    if (!engine.Open(path, true)) {
        std::cerr << "[-] Error: Failed to open XFS filesystem at: " << path << "\n";
        return 1;
    }

    std::cout << "=================================================\n";
    std::cout << "            TitanXFS Volume Inspection           \n";
    std::cout << "=================================================\n";
    std::cout << "Volume Path:        " << path << "\n";
    std::cout << "Volume Label:       " << engine.VolumeLabel() << "\n";
    std::cout << "Filesystem Magic:   0x" << std::hex << titanxfs::Be32ToCpu(engine.SuperBlock().sb_magicnum) << std::dec << "\n";
    std::cout << "Block Size:         " << engine.BlockSize() << " bytes\n";
    std::cout << "Total Blocks:       " << engine.TotalBlocks() << "\n";
    std::cout << "Free Blocks:        " << titanxfs::Be64ToCpu(engine.SuperBlock().sb_fdblocks) << "\n";
    std::cout << "Allocation Groups:  " << engine.AgCount() << "\n";
    std::cout << "AG Size (Blocks):   " << engine.AgBlocks() << "\n";
    std::cout << "Root Inode:         " << engine.RootInode() << "\n";
    std::cout << "Inode Size:         " << engine.InodeSize() << " bytes\n";

    std::cout << "\n--- Performing Integrity & Bit-Rot Scrub ---\n";
    titanxfs::XfsScrubReport report{};
    if (!engine.CheckAndScrub(report)) {
        std::cerr << "[-] Error: Scrubber encountered an unrecoverable failure!\n";
        return 1;
    }

    std::cout << "Blocks Scanned:     " << report.scannedBlocks << "\n";
    std::cout << "Inodes Scanned:     " << report.scannedInodes << "\n";
    std::cout << "Errors Detected:    " << report.errorsDetected << "\n";

    if (report.isClean && report.errorsDetected == 0) {
        std::cout << "[+] STATUS: VOLUME IS HEALTHY AND CONSISTENT\n";
        return 0;
    } else {
        std::cout << "[-] STATUS: CORRUPTION OR INCONSISTENCIES DETECTED!\n";
        return 2;
    }
}
