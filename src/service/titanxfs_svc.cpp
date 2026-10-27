#include "titanxfs/xfs_engine.hpp"
#include "titanxfs/winfsp_bridge.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstring>
#include <memory>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsvc.h>
#endif

namespace titanxfs {

constexpr uint64_t XFS_IPC_MAGIC = 0x58465349504331ULL; // "XFSIPC1"
constexpr uint32_t IpcCmdPing          = 1;
constexpr uint32_t IpcCmdVolumeMount   = 2;
constexpr uint32_t IpcCmdVolumeUnmount = 3;
constexpr uint32_t IpcCmdVolumeScrub   = 4;
constexpr uint32_t IpcCmdGetScrubReport= 5;
constexpr uint32_t IpcCmdServiceStatus = 6;
constexpr uint32_t IpcCmdListMounts    = 7;

struct MountedVolumeEntry {
    std::string devicePath;
    std::string driveLetter;
    std::string volumeLabel;
    std::shared_ptr<XfsEngine> engine;
#ifdef _WIN32
    std::shared_ptr<WinFspMountSession> session;
#endif
};

class TitanXfsDaemon {
public:
    static TitanXfsDaemon& Get() {
        static TitanXfsDaemon instance;
        return instance;
    }

    bool Start() {
        if (running_) return true;
        running_ = true;

        std::cout << "========================================================================\n";
        std::cout << "        TitanXFS Windows NT Subsystem Service Daemon                    \n";
        std::cout << "========================================================================\n";
        std::cout << "[TitanXFS Daemon] Initializing XFS filesystem subsystem...\n";

#ifdef _WIN32
        auto& fsp = WinFspApi::Get();
        if (fsp.isAvailable()) {
            std::cout << "[TitanXFS Daemon] WinFsp dynamic engine loaded successfully.\n";
        } else {
            std::cerr << "[TitanXFS Daemon] Warning: WinFsp dynamic library not found. Userland drive letter mounting will be disabled.\n";
        }
#endif

        ipcThread_ = std::thread(&TitanXfsDaemon::IpcLoop, this);
        scrubThread_ = std::thread(&TitanXfsDaemon::ScrubLoop, this);

        std::cout << "[TitanXFS Daemon] Service successfully started. Listening on Named Pipe: \\\\.\\pipe\\titanxfs_ipc\n";
        return true;
    }

    void Stop() {
        if (!running_) return;
        running_ = false;

        std::lock_guard<std::mutex> lock(mountMutex_);
        for (auto& v : mounts_) {
#ifdef _WIN32
            if (v.session) v.session->Stop();
#endif
            if (v.engine) v.engine->Close();
        }
        mounts_.clear();

        if (ipcThread_.joinable()) ipcThread_.detach();
        if (scrubThread_.joinable()) scrubThread_.detach();
    }

private:
    TitanXfsDaemon() = default;
    ~TitanXfsDaemon() { Stop(); }

    void IpcLoop() {
#ifdef _WIN32
        const wchar_t* pipeName = L"\\\\.\\pipe\\titanxfs_ipc";

        while (running_) {
            HANDLE hPipe = CreateNamedPipeW(
                pipeName,
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                65536, 65536, 0, nullptr
            );

            if (hPipe == INVALID_HANDLE_VALUE) {
                Sleep(500);
                continue;
            }

            if (ConnectNamedPipe(hPipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED)) {
                HandleClient(hPipe);
            }

            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
        }
#endif
    }

#ifdef _WIN32
    void HandleClient(HANDLE hPipe) {
#pragma pack(push, 1)
        struct ReqHeader {
            uint64_t magic;
            uint32_t cmd;
            uint32_t payloadLen;
        };
        struct RespHdr {
            uint64_t magic;
            uint32_t status;
            uint32_t payloadLen;
        };
#pragma pack(pop)

        ReqHeader hdr{};
        DWORD readBytes = 0;
        if (!ReadFile(hPipe, &hdr, sizeof(hdr), &readBytes, nullptr) || readBytes != sizeof(hdr)) {
            return;
        }

        if (hdr.magic != XFS_IPC_MAGIC) {
            return;
        }

        std::string requestPayload;
        if (hdr.payloadLen > 0 && hdr.payloadLen < 1048576) {
            requestPayload.resize(hdr.payloadLen);
            DWORD total = 0;
            while (total < hdr.payloadLen) {
                DWORD chunk = 0;
                if (!ReadFile(hPipe, requestPayload.data() + total, hdr.payloadLen - total, &chunk, nullptr) || chunk == 0) {
                    break;
                }
                total += chunk;
            }
        }

        std::string respPayload;
        uint32_t status = 0;

        switch (hdr.cmd) {
            case IpcCmdPing:
                respPayload = "PONG";
                break;

            case IpcCmdServiceStatus: {
                std::lock_guard<std::mutex> lock(mountMutex_);
                std::ostringstream ss;
                ss << "{\"service\":\"TitanXFS\",\"status\":\"running\",\"mountCount\":" << mounts_.size() << "}";
                respPayload = ss.str();
                break;
            }

            case IpcCmdListMounts: {
                std::lock_guard<std::mutex> lock(mountMutex_);
                std::ostringstream ss;
                ss << "[";
                for (size_t i = 0; i < mounts_.size(); ++i) {
                    if (i > 0) ss << ",";
                    ss << "{\"drive\":\"" << mounts_[i].driveLetter << "\",\"device\":\"" << mounts_[i].devicePath
                       << "\",\"label\":\"" << mounts_[i].volumeLabel << "\",\"totalBytes\":" << mounts_[i].engine->TotalBytes() << "}";
                }
                ss << "]";
                respPayload = ss.str();
                break;
            }

            case IpcCmdVolumeMount: {
                std::istringstream iss(requestPayload);
                std::string devPath, driveLetter;
                iss >> devPath >> driveLetter;

                if (devPath.empty() || driveLetter.empty()) {
                    status = 1;
                    respPayload = "Invalid arguments. Usage: <devPath> <driveLetter>";
                    break;
                }

                std::lock_guard<std::mutex> lock(mountMutex_);
                bool exists = false;
                for (const auto& m : mounts_) {
                    if (m.driveLetter == driveLetter) { exists = true; break; }
                }
                if (exists) {
                    status = 2;
                    respPayload = "Drive letter already in use.";
                    break;
                }

                auto eng = std::make_shared<XfsEngine>();
                if (!eng->Open(devPath, true)) {
                    status = 3;
                    respPayload = "Failed to open XFS volume.";
                    break;
                }

                std::wstring wDrive(driveLetter.begin(), driveLetter.end());
                auto sess = std::make_shared<WinFspMountSession>(eng, wDrive);
                if (!sess->Start()) {
                    status = 4;
                    respPayload = "WinFsp mount failed.";
                    break;
                }

                MountedVolumeEntry ent;
                ent.devicePath = devPath;
                ent.driveLetter = driveLetter;
                ent.volumeLabel = eng->VolumeLabel();
                ent.engine = eng;
                ent.session = sess;
                mounts_.push_back(std::move(ent));

                respPayload = "{\"status\":\"mounted\",\"drive\":\"" + driveLetter + "\",\"label\":\"" + eng->VolumeLabel() + "\"}";
                break;
            }

            case IpcCmdVolumeUnmount: {
                std::string drive = requestPayload;
                while (!drive.empty() && (drive.back() == ' ' || drive.back() == '\r' || drive.back() == '\n')) drive.pop_back();

                std::lock_guard<std::mutex> lock(mountMutex_);
                auto it = std::find_if(mounts_.begin(), mounts_.end(), [&](const MountedVolumeEntry& e) {
                    return e.driveLetter == drive;
                });

                if (it != mounts_.end()) {
                    if (it->session) it->session->Stop();
                    if (it->engine) it->engine->Close();
                    mounts_.erase(it);
                    respPayload = "{\"status\":\"unmounted\",\"drive\":\"" + drive + "\"}";
                } else {
                    status = 1;
                    respPayload = "Mount not found.";
                }
                break;
            }

            case IpcCmdVolumeScrub: {
                std::lock_guard<std::mutex> lock(mountMutex_);
                if (mounts_.empty()) {
                    respPayload = "No mounted XFS volumes to scrub.";
                } else {
                    XfsScrubReport rep{};
                    mounts_.front().engine->CheckAndScrub(rep);
                    std::ostringstream ss;
                    ss << "{\"clean\":" << (rep.isClean ? "true" : "false") << ",\"scannedBlocks\":"
                       << rep.scannedBlocks << ",\"errors\":" << rep.errorsDetected << "}";
                    respPayload = ss.str();
                }
                break;
            }

            default:
                status = 404;
                respPayload = "Unknown command";
                break;
        }

        RespHdr resp{XFS_IPC_MAGIC, status, static_cast<uint32_t>(respPayload.size())};
        DWORD written = 0;
        WriteFile(hPipe, &resp, sizeof(resp), &written, nullptr);
        if (!respPayload.empty()) {
            WriteFile(hPipe, respPayload.data(), static_cast<DWORD>(respPayload.size()), &written, nullptr);
        }
    }
#endif

    void ScrubLoop() {
        while (running_) {
            // Periodic background scrub check every 24 hours
            for (int i = 0; i < 86400 && running_; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            if (!running_) break;

            std::lock_guard<std::mutex> lock(mountMutex_);
            for (auto& m : mounts_) {
                if (m.engine && m.engine->IsOpen()) {
                    XfsScrubReport rep{};
                    m.engine->CheckAndScrub(rep);
                }
            }
        }
    }

    std::atomic<bool> running_{false};
    std::thread ipcThread_;
    std::thread scrubThread_;
    std::mutex mountMutex_;
    std::vector<MountedVolumeEntry> mounts_;
};

} // namespace titanxfs

#ifdef _WIN32
SERVICE_STATUS        g_ServiceStatus{};
SERVICE_STATUS_HANDLE g_StatusHandle{nullptr};

VOID WINAPI ServiceCtrlHandler(DWORD request) {
    switch (request) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
            titanxfs::TitanXfsDaemon::Get().Stop();
            g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
            SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
            break;
        default:
            break;
    }
}

VOID WINAPI ServiceMain(DWORD, LPWSTR*) {
    g_StatusHandle = RegisterServiceCtrlHandlerW(L"TitanXFS", ServiceCtrlHandler);
    if (!g_StatusHandle) return;

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    titanxfs::TitanXfsDaemon::Get().Start();

    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    while (g_ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
        Sleep(1000);
    }
}
#endif

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string cmd = argv[1];
#ifdef _WIN32
        if (cmd == "install") {
            wchar_t szPath[MAX_PATH];
            GetModuleFileNameW(nullptr, szPath, MAX_PATH);
            SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
            if (!scm) {
                std::cerr << "[-] Failed to open SCM. Run as Administrator.\n";
                return 1;
            }
            SC_HANDLE svc = CreateServiceW(
                scm, L"TitanXFS", L"TitanXFS Enterprise Linux Filesystem Service",
                SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
                szPath, nullptr, nullptr, nullptr, nullptr, nullptr
            );
            if (svc) {
                std::cout << "[+] TitanXFS Service registered successfully.\n";
                CloseServiceHandle(svc);
            } else {
                std::cerr << "[-] Failed to create service: " << GetLastError() << "\n";
            }
            CloseServiceHandle(scm);
            return 0;
        } else if (cmd == "uninstall") {
            SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
            if (!scm) return 1;
            SC_HANDLE svc = OpenServiceW(scm, L"TitanXFS", DELETE);
            if (svc) {
                DeleteService(svc);
                CloseServiceHandle(svc);
                std::cout << "[+] TitanXFS Service uninstalled.\n";
            }
            CloseServiceHandle(scm);
            return 0;
        }
#endif
        if (cmd == "run") {
            titanxfs::TitanXfsDaemon::Get().Start();
            std::cout << "[+] Press Ctrl+C to terminate debug daemon...\n";
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            return 0;
        }
    }

#ifdef _WIN32
    SERVICE_TABLE_ENTRYW ServiceTable[] = {
        {const_cast<LPWSTR>(L"TitanXFS"), ServiceMain},
        {nullptr, nullptr}
    };
    if (!StartServiceCtrlDispatcherW(ServiceTable)) {
        std::cout << "Usage: titanxfs_svc [install | uninstall | run]\n";
    }
#else
    std::cout << "Starting TitanXFS daemon in console mode...\n";
    titanxfs::TitanXfsDaemon::Get().Start();
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
#endif
    return 0;
}
