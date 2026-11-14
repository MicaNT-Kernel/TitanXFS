#include "titanxfs/xfs_engine.hpp"

#ifdef _WIN32
#include "titanxfs/winfsp_bridge.hpp"
#include <csignal>
#include <atomic>
#include <iostream>
#include <thread>
#include <chrono>

using namespace titanxfs;

namespace {
std::atomic<bool> g_Running{true};
WinFspMountSession g_Session;

void SigHandler(int) {
    std::cout << "\n[TitanXFS Mount] Stop signal received. Unmounting volume cleanly...\n";
    g_Running = false;
    g_Session.unmount();
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "========================================================================\n";
        std::cout << "    TitanXFS Filesystem Mount Driver (WinFsp Userland Bridge)           \n";
        std::cout << "========================================================================\n\n";
        std::cout << "Usage: titanxfs_mount <device_or_image_path> <drive_letter:> [options]\n\n";
        std::cout << "Options:\n";
        std::cout << "  --label <name>         Set volume label (default: XFS Volume or disk label)\n";
        std::cout << "  --readonly             Mount volume in read-only mode\n\n";
        std::cout << "Examples:\n";
        std::cout << "  titanxfs_mount rhel_backup.img X:\n";
        std::cout << "  titanxfs_mount \\\\.\\PhysicalDrive3 M: --label BigData\n\n";
        return 0;
    }

    std::string devicePath = argv[1];
    std::string driveLetter = argv[2];
    std::wstring volumeLabel;
    bool labelSpecified = false;
    bool readOnly = false;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--readonly") {
            readOnly = true;
        } else if (arg == "--label" && i + 1 < argc) {
            std::string lStr = argv[++i];
            volumeLabel = std::wstring(lStr.begin(), lStr.end());
            labelSpecified = true;
        }
    }

    if (driveLetter.size() == 1) {
        driveLetter += ":";
    } else if (driveLetter.size() > 2 && driveLetter[1] == ':') {
        driveLetter = driveLetter.substr(0, 2);
    }

    auto& fsp = WinFspApi::Get();
    if (!fsp.isAvailable()) {
        std::cerr << "[-] Error: WinFsp dynamic library (winfsp-x64.dll) was not found on this system.\n";
        std::cerr << "    Please install WinFsp (https://winfsp.dev) to mount filesystems.\n";
        return 1;
    }

    auto engine = std::make_shared<XfsEngine>();
    if (!engine->Open(devicePath, readOnly)) {
        std::cerr << "[-] Error: Failed to open XFS volume at " << devicePath << "\n";
        return 1;
    }

    if (!labelSpecified) {
        std::string diskLabel = engine->VolumeLabel();
        if (!diskLabel.empty()) {
            volumeLabel = std::wstring(diskLabel.begin(), diskLabel.end());
        } else {
            volumeLabel = L"XFS Volume";
        }
    }

    std::signal(SIGINT, SigHandler);
    std::signal(SIGTERM, SigHandler);

    if (!g_Session.mount(engine, driveLetter, volumeLabel)) {
        std::cerr << "[-] Error: Failed to mount XFS volume to " << driveLetter << "\n";
        return 1;
    }

    std::cout << "[+] Press Ctrl+C to unmount and exit.\n";
    while (g_Running && g_Session.IsMounted()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}
#else
int main() {
    std::cout << "TitanXFS WinFsp mounting is supported on Windows NT only.\n";
    return 1;
}
#endif
