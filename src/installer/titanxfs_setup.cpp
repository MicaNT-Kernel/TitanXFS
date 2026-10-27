#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <gdiplus.h>

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>
#include <algorithm>
#include "titanxfs/compression.hpp"

namespace {

#pragma pack(push, 1)
struct PayloadFooter {
    uint64_t tocOffset{0};
    uint64_t payloadStart{0};
    char     magic[16]{0};
};
#pragma pack(pop)
static_assert(sizeof(PayloadFooter) == 32, "PayloadFooter must be 32 bytes");

struct EmbeddedFile {
    std::string name;
    uint64_t offset{0};
    uint64_t compSize{0};
    uint64_t uncompSize{0};
    uint32_t isCompressed{0};
};

bool ReadEmbeddedPayload(std::vector<EmbeddedFile>& outEntries) {
    wchar_t thisExe[MAX_PATH];
    if (GetModuleFileNameW(nullptr, thisExe, MAX_PATH) == 0) return false;

    std::ifstream file(thisExe, std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    uint64_t fileSize = static_cast<uint64_t>(file.tellg());
    if (fileSize < sizeof(PayloadFooter)) return false;

    file.seekg(fileSize - sizeof(PayloadFooter), std::ios::beg);
    PayloadFooter footer{};
    file.read(reinterpret_cast<char*>(&footer), sizeof(footer));

    if (std::memcmp(footer.magic, "TITANXFS_BUNDLE", 15) != 0) return false;
    if (footer.tocOffset >= fileSize || footer.payloadStart >= fileSize) return false;

    file.seekg(footer.tocOffset, std::ios::beg);
    uint32_t entryCount = 0;
    file.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));
    if (entryCount == 0 || entryCount > 256) return false;

    outEntries.clear();
    for (uint32_t i = 0; i < entryCount; ++i) {
        uint32_t nameLen = 0;
        file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        if (nameLen > 1024) return false;
        std::string name(nameLen, '\0');
        file.read(name.data(), nameLen);

        EmbeddedFile ent;
        ent.name = name;
        file.read(reinterpret_cast<char*>(&ent.offset), sizeof(ent.offset));
        file.read(reinterpret_cast<char*>(&ent.compSize), sizeof(ent.compSize));
        file.read(reinterpret_cast<char*>(&ent.uncompSize), sizeof(ent.uncompSize));
        file.read(reinterpret_cast<char*>(&ent.isCompressed), sizeof(ent.isCompressed));
        outEntries.push_back(std::move(ent));
    }
    return !outEntries.empty();
}

bool ExtractEmbeddedFile(const EmbeddedFile& ent, const std::wstring& outPath) {
    wchar_t thisExe[MAX_PATH];
    if (GetModuleFileNameW(nullptr, thisExe, MAX_PATH) == 0) return false;

    std::ifstream file(thisExe, std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(ent.offset, std::ios::beg);
    std::vector<uint8_t> compData(ent.compSize);
    file.read(reinterpret_cast<char*>(compData.data()), ent.compSize);

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(outPath).parent_path(), ec);

    if (ent.isCompressed) {
        std::vector<uint8_t> uncompData(ent.uncompSize);
        if (!titanxfs::CompressionEngine::DecompressLZ4(compData, uncompData)) {
            return false;
        }
        std::ofstream out(std::filesystem::path(outPath), std::ios::binary);
        if (!out.is_open()) return false;
        out.write(reinterpret_cast<const char*>(uncompData.data()), uncompData.size());
        return true;
    } else {
        std::ofstream out(std::filesystem::path(outPath), std::ios::binary);
        if (!out.is_open()) return false;
        out.write(reinterpret_cast<const char*>(compData.data()), compData.size());
        return true;
    }
}

std::wstring GetModuleDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    auto idx = p.find_last_of(L"\\/");
    if (idx != std::wstring::npos) return p.substr(0, idx);
    return L".";
}

std::wstring FindHeroImage() {
    std::wstring curDir = GetModuleDir();
    std::vector<std::wstring> candidates = {
        curDir + L"\\assets\\titanxfs_hero.jpg",
        curDir + L"\\titanxfs_hero.jpg",
        curDir + L"\\..\\assets\\titanxfs_hero.jpg"
    };
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    return L"";
}

bool IsWinFspInstalled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WinFsp", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

[[maybe_unused]] void DrawHeroBanner(Gdiplus::Graphics& graphics, Gdiplus::Image* pImg, float targetX, float targetY, float targetW, float targetH) {
    if (!pImg || pImg->GetLastStatus() != Gdiplus::Ok) {
        Gdiplus::SolidBrush bg(Gdiplus::Color(255, 17, 24, 39));
        graphics.FillRectangle(&bg, targetX, targetY, targetW, targetH);
        return;
    }

    float imgW = static_cast<float>(pImg->GetWidth());
    float imgH = static_cast<float>(pImg->GetHeight());
    if (imgW <= 0.0f || imgH <= 0.0f) return;

    float scale = std::max(targetW / imgW, targetH / imgH);
    float drawW = imgW * scale;
    float drawH = imgH * scale;
    float drawX = targetX + (targetW - drawW) * 0.5f;
    float drawY = targetY + (targetH - drawH) * 0.5f;

    Gdiplus::Region oldClip;
    graphics.GetClip(&oldClip);
    graphics.SetClip(Gdiplus::RectF(targetX, targetY, targetW, targetH));
    graphics.DrawImage(pImg, drawX, drawY, drawW, drawH);
    graphics.SetClip(&oldClip);
}

[[maybe_unused]] bool CreateShortcut(const std::wstring& targetPath, const std::wstring& shortcutPath, const std::wstring& description) {
    IShellLinkW* psl = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&psl));
    if (SUCCEEDED(hr)) {
        psl->SetPath(targetPath.c_str());
        psl->SetDescription(description.c_str());
        psl->SetIconLocation(targetPath.c_str(), 0);
        IPersistFile* ppf = nullptr;
        hr = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf));
        if (SUCCEEDED(hr)) {
            hr = ppf->Save(shortcutPath.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
    }
    return SUCCEEDED(hr);
}

bool AddToSystemPath(const std::wstring& dir) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        wchar_t currentPath[32768]{};
        DWORD bufSize = sizeof(currentPath);
        DWORD type = REG_EXPAND_SZ;
        RegQueryValueExW(hKey, L"Path", nullptr, &type, reinterpret_cast<LPBYTE>(currentPath), &bufSize);
        std::wstring ws(currentPath);
        if (ws.find(dir) == std::wstring::npos) {
            if (!ws.empty() && ws.back() != L';') ws += L';';
            ws += dir;
            RegSetValueExW(hKey, L"Path", 0, type, reinterpret_cast<const BYTE*>(ws.c_str()), static_cast<DWORD>((ws.size() + 1) * sizeof(wchar_t)));
            DWORD_PTR dwResult;
            SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"Environment"), SMTO_ABORTIFHUNG, 2000, &dwResult);
        }
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

bool RegisterUninstallEntry(const std::wstring& installDir) {
    HKEY hKey;
    const wchar_t subkey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TitanXFS";
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, subkey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        auto setStr = [&](const wchar_t* valName, const std::wstring& val) {
            RegSetValueExW(hKey, valName, 0, REG_SZ, reinterpret_cast<const BYTE*>(val.c_str()), static_cast<DWORD>((val.size() + 1) * sizeof(wchar_t)));
        };
        auto setDword = [&](const wchar_t* valName, DWORD val) {
            RegSetValueExW(hKey, valName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(DWORD));
        };

        setStr(L"DisplayName", L"TitanXFS — Silicon Graphics XFS Filesystem Storage Engine");
        setStr(L"DisplayVersion", L"1.0.0");
        setStr(L"Publisher", L"MicaNT-Kernel / Daniel Elliott");
        setStr(L"InstallLocation", installDir);
        setStr(L"UninstallString", L"\"" + installDir + L"\\titanxfs_setup.exe\" /uninstall");
        setStr(L"QuietUninstallString", L"\"" + installDir + L"\\titanxfs_setup.exe\" /uninstall /silent");
        setStr(L"DisplayIcon", installDir + L"\\titanxfs_mount.exe,0");
        setDword(L"EstimatedSize", 16000);
        setDword(L"NoModify", 1);
        setDword(L"NoRepair", 1);

        RegCloseKey(hKey);
        return true;
    }
    return false;
}

[[maybe_unused]] bool PerformInstall(const std::wstring& dstDir, bool addPath, bool installWinFsp, bool, bool, HWND hProg, HWND hStat) {
    auto setStatus = [&](const std::wstring& text, int pos) {
        if (hStat) SetWindowTextW(hStat, text.c_str());
        if (hProg) SendMessageW(hProg, PBM_SETPOS, pos, 0);
    };

    setStatus(L"Preparing destination directory...", 10);
    std::error_code ec;
    std::filesystem::create_directories(dstDir, ec);

    std::wstring srcDir = GetModuleDir();

    // 1. Install WinFsp if requested
    if (installWinFsp && !IsWinFspInstalled()) {
        setStatus(L"Installing WinFsp Filesystem Driver (WHQL)...", 15);
        std::wstring tempMsi = std::filesystem::temp_directory_path().wstring() + L"\\TitanXFS\\winfsp.msi";
        bool foundMsi = false;

        std::vector<EmbeddedFile> entries;
        if (ReadEmbeddedPayload(entries)) {
            for (const auto& ef : entries) {
                if (ef.name.find("winfsp") != std::string::npos && ef.name.ends_with(".msi")) {
                    ExtractEmbeddedFile(ef, tempMsi);
                    foundMsi = true;
                    break;
                }
            }
        }

        if (!foundMsi) {
            std::wstring localMsi = srcDir + L"\\redist\\winfsp-2.1.25156.msi";
            if (std::filesystem::exists(localMsi, ec)) {
                std::filesystem::create_directories(std::filesystem::path(tempMsi).parent_path(), ec);
                std::filesystem::copy_file(localMsi, tempMsi, std::filesystem::copy_options::overwrite_existing, ec);
                foundMsi = true;
            }
        }

        if (foundMsi) {
            STARTUPINFOW si{};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi{};
            std::wstring cmd = L"msiexec.exe /i \"" + tempMsi + L"\" /qn /norestart";
            std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
            cmdBuf.push_back(0);
            if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, 60000);
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
            std::filesystem::remove(tempMsi, ec);
        }
    }

    // 2. Extract Embedded Payload
    std::vector<EmbeddedFile> embeddedFiles;
    if (ReadEmbeddedPayload(embeddedFiles) && !embeddedFiles.empty()) {
        int idx = 0;
        for (const auto& ef : embeddedFiles) {
            int pct = 25 + static_cast<int>((idx * 45) / embeddedFiles.size());
            std::wstring wName(ef.name.begin(), ef.name.end());
            setStatus(L"Extracting " + wName + L"...", pct);
            ExtractEmbeddedFile(ef, dstDir + L"\\" + wName);
            idx++;
        }
    } else {
        setStatus(L"Copying TitanXFS files...", 35);
        std::vector<std::wstring> exes = { L"titanxfs_mount.exe", L"titanxfs_format.exe", L"titanxfs_check.exe", L"titanxfs_svc.exe" };
        for (const auto& exe : exes) {
            std::wstring s = srcDir + L"\\" + exe;
            if (std::filesystem::exists(s, ec)) {
                std::filesystem::copy_file(s, dstDir + L"\\" + exe, std::filesystem::copy_options::overwrite_existing, ec);
            }
        }
        std::wstring heroSrc = FindHeroImage();
        if (!heroSrc.empty()) {
            std::filesystem::create_directories(dstDir + L"\\assets", ec);
            std::filesystem::copy_file(heroSrc, dstDir + L"\\assets\\titanxfs_hero.jpg", std::filesystem::copy_options::overwrite_existing, ec);
        }
    }

    // Copy setup tool itself for uninstaller
    wchar_t thisExe[MAX_PATH];
    GetModuleFileNameW(nullptr, thisExe, MAX_PATH);
    std::filesystem::copy_file(thisExe, dstDir + L"\\titanxfs_setup.exe", std::filesystem::copy_options::overwrite_existing, ec);

    if (addPath) {
        setStatus(L"Configuring System PATH...", 75);
        AddToSystemPath(dstDir);
    }

    // Register Auto-Start Background Service
    setStatus(L"Registering TitanXFS Windows Service...", 85);
    std::wstring svcExe = dstDir + L"\\titanxfs_svc.exe";
    if (std::filesystem::exists(svcExe, ec)) {
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        std::wstring cmd = L"\"" + svcExe + L"\" install";
        std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back(0);
        if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 10000);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }

    setStatus(L"Registering Add/Remove Programs entry...", 92);
    RegisterUninstallEntry(dstDir);

    setStatus(L"Installation complete! XFS drives ready to mount.", 100);
    return true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            BOOL darkMode = TRUE;
            DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
            DwmSetWindowAttribute(hwnd, 19, &darkMode, sizeof(darkMode));
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartupInput gdiInput;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiInput, nullptr) != Gdiplus::Ok) {
        return 1;
    }

    INITCOMMONCONTROLSEX icex{sizeof(icex), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"TitanXfsSetupWindowClass";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(17, 24, 39));
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    HWND hMainWnd = CreateWindowExW(
        0, CLASS_NAME, L"TitanXFS Setup",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 830, 560,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hMainWnd) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 1;
    }

    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return static_cast<int>(msg.wParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    return wWinMain(hInstance, nullptr, GetCommandLineW(), nCmdShow);
}
