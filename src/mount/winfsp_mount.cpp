#include "titanxfs/xfs_engine.hpp"
#include "titanxfs/winfsp_bridge.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

int main(int argc, char* argv[]) {
    if (argc < 2 || (argc >= 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h" || std::string(argv[1]) == "/?"))) {
        std::cout << "TitanXFS WinFsp Mount Driver Bridge v1.0.0\n";
        std::cout << "Usage: titanxfs_mount <disk_image_or_device> <drive_letter:>\n";
        std::cout << "Example: titanxfs_mount C:\\storage\\rhel_backup.img X:\n";
        return 0;
    }
    if (argc < 3) {
        std::cerr << "[-] Error: Missing drive letter.\n";
        return 1;
    }

    std::string devPath = argv[1];
    std::string driveLetterStr = argv[2];
    std::wstring wDrive(driveLetterStr.begin(), driveLetterStr.end());

    std::cout << "[+] Opening XFS volume: " << devPath << "...\n";
    auto engine = std::make_shared<titanxfs::XfsEngine>();
    if (!engine->Open(devPath, true)) {
        std::cerr << "[-] Error: Failed to open XFS filesystem at " << devPath << "\n";
        return 1;
    }

    std::cout << "[+] XFS Volume Label: " << engine->VolumeLabel() << "\n";
    std::cout << "[+] Initializing WinFsp User-Mode Filesystem Bridge...\n";

#ifdef _WIN32
    auto& fsp = titanxfs::WinFspApi::Get();
    if (!fsp.isAvailable()) {
        std::cerr << "[-] Error: WinFsp runtime library (winfsp-x64.dll) is not installed or available on this system.\n";
        return 2;
    }

    titanxfs::WinFspMountSession session(engine, wDrive);
    if (!session.Start()) {
        std::cerr << "[-] Error: Failed to mount XFS volume to " << driveLetterStr << "\n";
        return 3;
    }

    std::cout << "[+] Volume successfully mounted to " << driveLetterStr << "\\. Press Ctrl+C to unmount...\n";
    while (session.IsMounted()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
#else
    std::cout << "WinFsp mounting is supported on Windows NT only.\n";
#endif
    return 0;
}
