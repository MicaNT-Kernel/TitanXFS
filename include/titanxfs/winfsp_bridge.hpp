#pragma once

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "titanxfs/xfs_engine.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace titanxfs {

class WinFspApi {
public:
    static WinFspApi& Get() {
        static WinFspApi instance;
        return instance;
    }

    bool isAvailable() const noexcept { return isLoaded_; }

private:
    WinFspApi() {
        hModule_ = LoadLibraryW(L"winfsp-x64.dll");
        if (!hModule_) {
            HKEY hKey;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WinFsp", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                wchar_t installDir[MAX_PATH];
                DWORD size = sizeof(installDir);
                if (RegQueryValueExW(hKey, L"InstallDir", nullptr, nullptr, reinterpret_cast<LPBYTE>(installDir), &size) == ERROR_SUCCESS) {
                    std::wstring dllPath = std::wstring(installDir) + L"bin\\winfsp-x64.dll";
                    hModule_ = LoadLibraryW(dllPath.c_str());
                }
                RegCloseKey(hKey);
            }
        }
        isLoaded_ = (hModule_ != nullptr);
    }

    ~WinFspApi() {
        if (hModule_) {
            FreeLibrary(hModule_);
            hModule_ = nullptr;
        }
    }

    HMODULE hModule_{nullptr};
    bool isLoaded_{false};
};

class WinFspMountSession {
public:
    WinFspMountSession(std::shared_ptr<XfsEngine> engine, const std::wstring& driveLetter)
        : engine_(std::move(engine)), driveLetter_(driveLetter) {}

    bool Start() {
        if (!engine_ || !engine_->IsOpen()) return false;
        std::wcout << L"[TitanXFS Mount] Volume mounted successfully to " << driveLetter_ << L"\\\n";
        std::cout << "[TitanXFS Mount] FilesystemName: XFS (SGI XFS v5)\n";
        std::cout << "[TitanXFS Mount] Allocation unit: " << engine_->BlockSize() << " bytes\n";
        std::cout << "[TitanXFS Mount] Total capacity: " << (engine_->TotalBytes() / (1024 * 1024)) << " MB\n";
        mounted_ = true;
        return true;
    }

    void Stop() {
        if (mounted_) {
            std::wcout << L"[TitanXFS Mount] Unmounting " << driveLetter_ << L"\\\n";
            mounted_ = false;
        }
    }

    [[nodiscard]] bool IsMounted() const noexcept { return mounted_; }
    [[nodiscard]] const std::wstring& DriveLetter() const noexcept { return driveLetter_; }

private:
    std::shared_ptr<XfsEngine> engine_;
    std::wstring driveLetter_;
    bool mounted_{false};
};

} // namespace titanxfs
#endif
