#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winternl.h>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <cstdint>
#include "titanxfs/xfs_engine.hpp"

namespace titanxfs {

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)
#define STATUS_OBJECT_NAME_NOT_FOUND     ((NTSTATUS)0xC0000034L)
#define STATUS_OBJECT_NAME_COLLISION     ((NTSTATUS)0xC0000035L)
#define STATUS_ACCESS_DENIED             ((NTSTATUS)0xC0000022L)
#define STATUS_END_OF_FILE               ((NTSTATUS)0xC0000011L)
#define STATUS_BUFFER_TOO_SMALL          ((NTSTATUS)0xC0000023L)
#define STATUS_MEDIA_WRITE_PROTECTED     ((NTSTATUS)0xC00000A2L)
#define STATUS_FILE_IS_A_DIRECTORY       ((NTSTATUS)0xC00000BAL)
#define STATUS_NOT_A_DIRECTORY           ((NTSTATUS)0xC0000103L)
#define STATUS_DIRECTORY_NOT_EMPTY       ((NTSTATUS)0xC0000101L)
#endif

struct FSP_FSCTL_VOLUME_INFO {
    UINT64 TotalSize;
    UINT64 FreeSize;
    UINT16 VolumeLabelLength;
    WCHAR  VolumeLabel[32];
};
static_assert(sizeof(FSP_FSCTL_VOLUME_INFO) == 88, "sizeof(FSP_FSCTL_VOLUME_INFO) must be 88");

struct FSP_FSCTL_FILE_INFO {
    UINT32 FileAttributes;
    UINT32 ReparseTag;
    UINT64 AllocationSize;
    UINT64 FileSize;
    UINT64 CreationTime;
    UINT64 LastAccessTime;
    UINT64 LastWriteTime;
    UINT64 ChangeTime;
    UINT64 IndexNumber;
    UINT32 HardLinks;
    UINT32 EaSize;
};
static_assert(sizeof(FSP_FSCTL_FILE_INFO) == 72, "sizeof(FSP_FSCTL_FILE_INFO) must be 72");

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-extensions"
#pragma clang diagnostic ignored "-Wzero-length-array"
#endif

struct FSP_FSCTL_DIR_INFO {
    UINT16 Size;
    FSP_FSCTL_FILE_INFO FileInfo;
    union {
        UINT64 NextOffset;
        UINT8  Padding[24];
    };
    WCHAR  FileNameBuf[];
};
static_assert(sizeof(FSP_FSCTL_DIR_INFO) == 104, "sizeof(FSP_FSCTL_DIR_INFO) must be 104");

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

struct FSP_FSCTL_VOLUME_PARAMS {
    UINT16 Version;
    UINT16 SectorSize;
    UINT16 SectorsPerAllocationUnit;
    UINT16 MaxComponentLength;
    UINT64 VolumeCreationTime;
    UINT32 VolumeSerialNumber;
    UINT32 TransactTimeout;
    UINT32 IrpTimeout;
    UINT32 IrpCapacity;
    UINT32 FileInfoTimeout;
    UINT32 CaseSensitiveSearch:1;
    UINT32 CasePreservedNames:1;
    UINT32 UnicodeOnDisk:1;
    UINT32 PersistentAcls:1;
    UINT32 ReparsePoints:1;
    UINT32 ReparsePointsAccessCheck:1;
    UINT32 NamedStreams:1;
    UINT32 HardLinks:1;
    UINT32 ExtendedAttributes:1;
    UINT32 ReadOnlyVolume:1;
    UINT32 PostCleanupWhenModifiedOnly:1;
    UINT32 PassQueryDirectoryPattern:1;
    UINT32 AlwaysUseDoubleBuffering:1;
    UINT32 PassQueryDirectoryFileName:1;
    UINT32 FlushAndPurgeOnCleanup:1;
    UINT32 DeviceControl:1;
    UINT32 UmFileContextIsUserContext2:1;
    UINT32 UmFileContextIsFullContext:1;
    UINT32 UmNoReparsePointsDirCheck:1;
    UINT32 UmReservedFlags:5;
    UINT32 AllowOpenInKernelMode:1;
    UINT32 CasePreservedExtendedAttributes:1;
    UINT32 WslFeatures:1;
    UINT32 DirectoryMarkerAsNextOffset:1;
    UINT32 RejectIrpPriorToTransact0:1;
    UINT32 SupportsPosixUnlinkRename:1;
    UINT32 PostDispositionWhenNecessaryOnly:1;
    UINT32 KmReservedFlags:1;
    WCHAR  Prefix[192];
    WCHAR  FileSystemName[16];
    UINT8  Reserved[48];
};
static_assert(sizeof(FSP_FSCTL_VOLUME_PARAMS) == 504, "FSP_FSCTL_VOLUME_PARAMS must be 504 bytes");

struct _FSP_FILE_SYSTEM {
    PVOID UserContext;
};
typedef struct _FSP_FILE_SYSTEM FSP_FILE_SYSTEM;

struct FSP_FILE_SYSTEM_INTERFACE {
    NTSTATUS (*GetVolumeInfo)(FSP_FILE_SYSTEM *FileSystem, FSP_FSCTL_VOLUME_INFO *VolumeInfo);
    NTSTATUS (*SetVolumeLabel)(FSP_FILE_SYSTEM *FileSystem, PWSTR VolumeLabel, FSP_FSCTL_VOLUME_INFO *VolumeInfo);
    NTSTATUS (*GetSecurityByName)(FSP_FILE_SYSTEM *FileSystem, PWSTR FileName, PUINT32 PFileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize);
    NTSTATUS (*Create)(FSP_FILE_SYSTEM *FileSystem, PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize, PVOID *PFileContext, FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*Open)(FSP_FILE_SYSTEM *FileSystem, PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, PVOID *PFileContext, FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*Overwrite)(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize, FSP_FSCTL_FILE_INFO *FileInfo);
    VOID     (*Cleanup)(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, PWSTR FileName, UINT32 Flags);
    VOID     (*Close)(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext);
    NTSTATUS (*Read)(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length, PULONG PBytesTransferred);
    NTSTATUS (*Write)(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length, BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo, PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*Flush)(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*GetFileInfo)(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*SetBasicInfo)(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, UINT32 FileAttributes, UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime, FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*SetFileSize)(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, UINT64 NewSize, BOOLEAN SetAllocationSize, FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*CanDelete)(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, PWSTR FileName);
    NTSTATUS (*Rename)(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists);
    NTSTATUS (*GetSecurity)(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize);
    NTSTATUS (*SetSecurity)(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor);
    NTSTATUS (*ReadDirectory)(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred);
    NTSTATUS (*ResolveReparsePoints)(FSP_FILE_SYSTEM *FileSystem, PWSTR FileName, UINT32 ReparsePointIndex, BOOLEAN ResolveLastPathComponent, PIO_STATUS_BLOCK PIoStatus, PVOID Buffer, PSIZE_T PSize);
};

class WinFspApi {
public:
    static WinFspApi& Get() {
        static WinFspApi instance;
        return instance;
    }

    bool isAvailable() const noexcept { return hDll_ != nullptr; }

    NTSTATUS FspFileSystemCreate(PWSTR DevicePath, const FSP_FSCTL_VOLUME_PARAMS *VolumeParams, const FSP_FILE_SYSTEM_INTERFACE *Interface, FSP_FILE_SYSTEM **PFileSystem) {
        if (!pfnFspFileSystemCreate) return STATUS_ACCESS_DENIED;
        return pfnFspFileSystemCreate(DevicePath, VolumeParams, Interface, PFileSystem);
    }

    VOID FspFileSystemDelete(FSP_FILE_SYSTEM *FileSystem) {
        if (pfnFspFileSystemDelete) pfnFspFileSystemDelete(FileSystem);
    }

    NTSTATUS FspFileSystemSetMountPoint(FSP_FILE_SYSTEM *FileSystem, PWSTR MountPoint) {
        if (!pfnFspFileSystemSetMountPoint) return STATUS_ACCESS_DENIED;
        return pfnFspFileSystemSetMountPoint(FileSystem, MountPoint);
    }

    NTSTATUS FspFileSystemStartDispatcher(FSP_FILE_SYSTEM *FileSystem, ULONG ThreadCount) {
        if (!pfnFspFileSystemStartDispatcher) return STATUS_ACCESS_DENIED;
        return pfnFspFileSystemStartDispatcher(FileSystem, ThreadCount);
    }

    VOID FspFileSystemStopDispatcher(FSP_FILE_SYSTEM *FileSystem) {
        if (pfnFspFileSystemStopDispatcher) pfnFspFileSystemStopDispatcher(FileSystem);
    }

    BOOLEAN FspFileSystemAddDirInfo(FSP_FSCTL_DIR_INFO *DirInfo, PVOID Buffer, ULONG Length, PULONG PBytesTransferred) {
        if (!pfnFspFileSystemAddDirInfo) return FALSE;
        return pfnFspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred);
    }

private:
    WinFspApi() {
        hDll_ = LoadLibraryW(L"winfsp-x64.dll");
        if (!hDll_) {
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WinFsp", 0, KEY_READ | KEY_WOW64_32KEY, &hKey) == ERROR_SUCCESS ||
                RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WinFsp", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                wchar_t dirPath[MAX_PATH] = {0};
                DWORD bufSize = sizeof(dirPath);
                if (RegQueryValueExW(hKey, L"SxsDir", nullptr, nullptr, reinterpret_cast<LPBYTE>(dirPath), &bufSize) == ERROR_SUCCESS) {
                    std::wstring dllPath = std::wstring(dirPath) + L"bin\\winfsp-x64.dll";
                    hDll_ = LoadLibraryW(dllPath.c_str());
                }
                if (!hDll_) {
                    bufSize = sizeof(dirPath);
                    if (RegQueryValueExW(hKey, L"InstallDir", nullptr, nullptr, reinterpret_cast<LPBYTE>(dirPath), &bufSize) == ERROR_SUCCESS) {
                        std::wstring dllPath = std::wstring(dirPath) + L"bin\\winfsp-x64.dll";
                        hDll_ = LoadLibraryW(dllPath.c_str());
                    }
                }
                RegCloseKey(hKey);
            }
        }
        if (!hDll_) {
            hDll_ = LoadLibraryW(L"C:\\Program Files (x86)\\WinFsp\\bin\\winfsp-x64.dll");
            if (!hDll_) {
                hDll_ = LoadLibraryW(L"C:\\Program Files\\WinFsp\\bin\\winfsp-x64.dll");
            }
        }

        if (hDll_) {
            pfnFspFileSystemCreate = (decltype(pfnFspFileSystemCreate))GetProcAddress(hDll_, "FspFileSystemCreate");
            pfnFspFileSystemDelete = (decltype(pfnFspFileSystemDelete))GetProcAddress(hDll_, "FspFileSystemDelete");
            pfnFspFileSystemSetMountPoint = (decltype(pfnFspFileSystemSetMountPoint))GetProcAddress(hDll_, "FspFileSystemSetMountPoint");
            pfnFspFileSystemStartDispatcher = (decltype(pfnFspFileSystemStartDispatcher))GetProcAddress(hDll_, "FspFileSystemStartDispatcher");
            pfnFspFileSystemStopDispatcher = (decltype(pfnFspFileSystemStopDispatcher))GetProcAddress(hDll_, "FspFileSystemStopDispatcher");
            pfnFspFileSystemAddDirInfo = (decltype(pfnFspFileSystemAddDirInfo))GetProcAddress(hDll_, "FspFileSystemAddDirInfo");
        }
    }

    ~WinFspApi() {
        if (hDll_) FreeLibrary(hDll_);
    }

    HMODULE hDll_{nullptr};
    NTSTATUS (*pfnFspFileSystemCreate)(PWSTR, const FSP_FSCTL_VOLUME_PARAMS*, const FSP_FILE_SYSTEM_INTERFACE*, FSP_FILE_SYSTEM**){nullptr};
    VOID     (*pfnFspFileSystemDelete)(FSP_FILE_SYSTEM*){nullptr};
    NTSTATUS (*pfnFspFileSystemSetMountPoint)(FSP_FILE_SYSTEM*, PWSTR){nullptr};
    NTSTATUS (*pfnFspFileSystemStartDispatcher)(FSP_FILE_SYSTEM*, ULONG){nullptr};
    VOID     (*pfnFspFileSystemStopDispatcher)(FSP_FILE_SYSTEM*){nullptr};
    BOOLEAN  (*pfnFspFileSystemAddDirInfo)(FSP_FSCTL_DIR_INFO*, PVOID, ULONG, PULONG){nullptr};
};

struct FileNodeContext {
    uint64_t inodeNumber{0};
    XfsDinode inode{};
    std::vector<uint8_t> fork;
    std::string path;
    bool isDirectory{false};
};

class WinFspMountSession {
public:
    WinFspMountSession() = default;
    ~WinFspMountSession() { unmount(); }

    bool mount(std::shared_ptr<XfsEngine> engine, const std::string& mountPoint, const std::wstring& volumeLabel = L"XFS Volume") {
        unmount();
        engine_ = engine;
        mountPoint_ = mountPoint;
        volumeLabel_ = volumeLabel;

        auto& fsp = WinFspApi::Get();
        if (!fsp.isAvailable()) {
            std::cerr << "[-] Error: WinFsp runtime library (winfsp-x64.dll) was not found on this system.\n";
            return false;
        }

        std::memset(&volParams_, 0, sizeof(volParams_));
        volParams_.Version = sizeof(volParams_);
        volParams_.SectorSize = static_cast<UINT16>(engine_->BlockSize());
        volParams_.SectorsPerAllocationUnit = 1;
        volParams_.MaxComponentLength = 255;
        volParams_.VolumeCreationTime = 0;
        volParams_.VolumeSerialNumber = 0x58465335; // "XFS5"
        volParams_.FileInfoTimeout = 1000;
        volParams_.CasePreservedNames = 1;
        volParams_.UnicodeOnDisk = 1;
        std::wcsncpy(volParams_.FileSystemName, L"XFS", 15);

        std::memset(&iface_, 0, sizeof(iface_));
        iface_.GetVolumeInfo = MountGetVolumeInfo;
        iface_.GetSecurityByName = MountGetSecurityByName;
        iface_.GetSecurity = MountGetSecurity;
        iface_.Create = MountCreate;
        iface_.Open = MountOpen;
        iface_.Close = MountClose;
        iface_.Cleanup = MountCleanup;
        iface_.Read = MountRead;
        iface_.GetFileInfo = MountGetFileInfo;
        iface_.ReadDirectory = MountReadDirectory;

        wchar_t devicePath[] = L"WinFsp.Disk";
        NTSTATUS status = fsp.FspFileSystemCreate(devicePath, &volParams_, &iface_, &fspFs_);
        if (status != STATUS_SUCCESS) {
            std::cerr << "[-] FspFileSystemCreate failed: 0x" << std::hex << status << "\n";
            return false;
        }

        fspFs_->UserContext = this;

        std::wstring wMountPoint(mountPoint.begin(), mountPoint.end());
        status = fsp.FspFileSystemSetMountPoint(fspFs_, const_cast<PWSTR>(wMountPoint.c_str()));
        if (status != STATUS_SUCCESS) {
            std::cerr << "[-] FspFileSystemSetMountPoint failed: 0x" << std::hex << status << "\n";
            fsp.FspFileSystemDelete(fspFs_);
            fspFs_ = nullptr;
            return false;
        }

        status = fsp.FspFileSystemStartDispatcher(fspFs_, 0);
        if (status != STATUS_SUCCESS) {
            std::cerr << "[-] FspFileSystemStartDispatcher failed: 0x" << std::hex << status << "\n";
            fsp.FspFileSystemDelete(fspFs_);
            fspFs_ = nullptr;
            return false;
        }

        isMounted_ = true;
        std::cout << "[+] TitanXFS Volume mounted at " << mountPoint << "\\ [XFS - WinFsp Bridge]\n";
        return true;
    }

    void unmount() {
        if (isMounted_ && fspFs_) {
            auto& fsp = WinFspApi::Get();
            fsp.FspFileSystemStopDispatcher(fspFs_);
            fsp.FspFileSystemDelete(fspFs_);
            fspFs_ = nullptr;
            isMounted_ = false;
            std::cout << "[+] TitanXFS Volume cleanly unmounted from " << mountPoint_ << "\n";
        }
    }

    [[nodiscard]] bool IsMounted() const noexcept { return isMounted_; }
    [[nodiscard]] const std::string& MountPoint() const noexcept { return mountPoint_; }

private:
    static NTSTATUS MountGetVolumeInfo(FSP_FILE_SYSTEM *fs, FSP_FSCTL_VOLUME_INFO *volInfo) {
        auto* self = static_cast<WinFspMountSession*>(fs->UserContext);
        volInfo->TotalSize = self->engine_->TotalBytes();
        volInfo->FreeSize = self->engine_->TotalBytes() / 2;
        volInfo->VolumeLabelLength = static_cast<UINT16>(self->volumeLabel_.size() * sizeof(WCHAR));
        std::wcsncpy(volInfo->VolumeLabel, self->volumeLabel_.c_str(), 31);
        return STATUS_SUCCESS;
    }

    static NTSTATUS MountGetSecurityByName(FSP_FILE_SYSTEM *fs, PWSTR fileName, PUINT32 pFileAttributes, PSECURITY_DESCRIPTOR secDesc, SIZE_T *pSecDescSize) {
        (void)fs; (void)secDesc;
        if (pSecDescSize) *pSecDescSize = 0;
        if (pFileAttributes) {
            if (std::wcscmp(fileName, L"\\") == 0 || std::wcscmp(fileName, L"/") == 0) {
                *pFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            } else {
                *pFileAttributes = FILE_ATTRIBUTE_NORMAL;
            }
        }
        return STATUS_SUCCESS;
    }

    static NTSTATUS MountGetSecurity(FSP_FILE_SYSTEM*, PVOID, PSECURITY_DESCRIPTOR, SIZE_T *pSecDescSize) {
        if (pSecDescSize) *pSecDescSize = 0;
        return STATUS_SUCCESS;
    }

    static NTSTATUS MountCreate(FSP_FILE_SYSTEM *fs, PWSTR fileName, UINT32, UINT32, UINT32, PSECURITY_DESCRIPTOR, UINT64, PVOID *pFileContext, FSP_FSCTL_FILE_INFO *fileInfo) {
        return MountOpen(fs, fileName, 0, 0, pFileContext, fileInfo);
    }

    static VOID MountCleanup(FSP_FILE_SYSTEM*, PVOID, PWSTR, UINT32) {
    }

    static NTSTATUS MountOpen(FSP_FILE_SYSTEM *fs, PWSTR fileName, UINT32, UINT32, PVOID *pFileContext, FSP_FSCTL_FILE_INFO *fileInfo) {
        auto* self = static_cast<WinFspMountSession*>(fs->UserContext);

        std::wstring wPath(fileName);
        std::string path(wPath.begin(), wPath.end());
        std::replace(path.begin(), path.end(), '\\', '/');
        if (path.empty()) path = "/";

        uint64_t ino = 0;
        if (!self->engine_->ResolvePath(path, ino)) {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        auto ctx = std::make_unique<FileNodeContext>();
        ctx->inodeNumber = ino;
        ctx->path = path;
        if (!self->engine_->ReadInode(ino, ctx->inode, ctx->fork)) {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
        ctx->isDirectory = (ctx->inode.di_mode & 0170000) == 0040000;

        if (fileInfo) {
            uint64_t winTime = (static_cast<uint64_t>(ctx->inode.di_mtime_sec) + 11644473600ULL) * 10000000ULL;
            fileInfo->FileAttributes = ctx->isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            fileInfo->FileSize = ctx->inode.di_size;
            fileInfo->AllocationSize = (fileInfo->FileSize + 4095) & ~4095ULL;
            fileInfo->IndexNumber = ino;
            fileInfo->HardLinks = ctx->inode.di_nlink;
            fileInfo->CreationTime = winTime;
            fileInfo->LastAccessTime = winTime;
            fileInfo->LastWriteTime = winTime;
            fileInfo->ChangeTime = winTime;
        }

        *pFileContext = ctx.release();
        return STATUS_SUCCESS;
    }

    static VOID MountClose(FSP_FILE_SYSTEM*, PVOID fileContext) {
        if (fileContext) {
            delete static_cast<FileNodeContext*>(fileContext);
        }
    }

    static NTSTATUS MountRead(FSP_FILE_SYSTEM *fs, PVOID fileContext, PVOID buffer, UINT64 offset, ULONG length, PULONG pBytesTransferred) {
        auto* self = static_cast<WinFspMountSession*>(fs->UserContext);
        auto* ctx = static_cast<FileNodeContext*>(fileContext);

        std::vector<uint8_t> data;
        if (!self->engine_->ReadFileData(ctx->inode, ctx->fork, offset, length, data)) {
            return STATUS_END_OF_FILE;
        }

        std::memcpy(buffer, data.data(), data.size());
        if (pBytesTransferred) *pBytesTransferred = static_cast<ULONG>(data.size());
        return STATUS_SUCCESS;
    }

    static NTSTATUS MountGetFileInfo(FSP_FILE_SYSTEM*, PVOID fileContext, FSP_FSCTL_FILE_INFO *fileInfo) {
        auto* ctx = static_cast<FileNodeContext*>(fileContext);
        uint64_t winTime = (static_cast<uint64_t>(ctx->inode.di_mtime_sec) + 11644473600ULL) * 10000000ULL;
        fileInfo->FileAttributes = ctx->isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
        fileInfo->FileSize = ctx->inode.di_size;
        fileInfo->AllocationSize = (fileInfo->FileSize + 4095) & ~4095ULL;
        fileInfo->IndexNumber = ctx->inodeNumber;
        fileInfo->HardLinks = ctx->inode.di_nlink;
        fileInfo->CreationTime = winTime;
        fileInfo->LastAccessTime = winTime;
        fileInfo->LastWriteTime = winTime;
        fileInfo->ChangeTime = winTime;
        return STATUS_SUCCESS;
    }

    static NTSTATUS MountReadDirectory(FSP_FILE_SYSTEM *fs, PVOID fileContext, PWSTR, PWSTR marker, PVOID buffer, ULONG length, PULONG pBytesTransferred) {
        auto* self = static_cast<WinFspMountSession*>(fs->UserContext);
        auto* ctx = static_cast<FileNodeContext*>(fileContext);
        if (!ctx->isDirectory) return STATUS_NOT_A_DIRECTORY;

        std::vector<XfsDirEntryInfo> entries;
        if (!self->engine_->ReadDirectory(ctx->inodeNumber, entries)) {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        auto& fsp = WinFspApi::Get();
        if (pBytesTransferred) *pBytesTransferred = 0;
        bool passMarker = (marker == nullptr || *marker == 0);

        for (const auto& ent : entries) {
            std::wstring wName(ent.name.begin(), ent.name.end());
            if (!passMarker) {
                if (wName == marker) passMarker = true;
                continue;
            }

            UINT8 dirBuf[sizeof(FSP_FSCTL_DIR_INFO) + 256 * sizeof(WCHAR)];
            auto* dirInfo = reinterpret_cast<FSP_FSCTL_DIR_INFO*>(dirBuf);
            std::memset(dirInfo, 0, sizeof(FSP_FSCTL_DIR_INFO));

            XfsDinode nodeInode{};
            std::vector<uint8_t> childFork;
            uint64_t itemSize = 0;
            uint64_t winTime = (1700000000ULL + 11644473600ULL) * 10000000ULL;
            bool isSubDir = (ent.fileType == 2);

            if (self->engine_->ReadInode(ent.inodeNumber, nodeInode, childFork)) {
                itemSize = nodeInode.di_size;
                isSubDir = (nodeInode.di_mode & 0170000) == 0040000;
                if (nodeInode.di_mtime_sec > 0) {
                    winTime = (static_cast<uint64_t>(nodeInode.di_mtime_sec) + 11644473600ULL) * 10000000ULL;
                }
            } else if (isSubDir) {
                itemSize = 4096;
            }

            dirInfo->Size = static_cast<UINT16>(104 + (wName.size() * sizeof(WCHAR)));
            dirInfo->FileInfo.FileAttributes = isSubDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            dirInfo->FileInfo.FileSize = itemSize;
            dirInfo->FileInfo.AllocationSize = (itemSize + 4095) & ~4095ULL;
            dirInfo->FileInfo.IndexNumber = ent.inodeNumber;
            dirInfo->FileInfo.HardLinks = 1;
            dirInfo->FileInfo.CreationTime = winTime;
            dirInfo->FileInfo.LastAccessTime = winTime;
            dirInfo->FileInfo.LastWriteTime = winTime;
            dirInfo->FileInfo.ChangeTime = winTime;
            std::memcpy(reinterpret_cast<uint8_t*>(dirInfo) + 104, wName.c_str(), wName.size() * sizeof(WCHAR));

            if (!fsp.FspFileSystemAddDirInfo(dirInfo, buffer, length, pBytesTransferred)) {
                return STATUS_SUCCESS;
            }
        }

        fsp.FspFileSystemAddDirInfo(nullptr, buffer, length, pBytesTransferred);
        return STATUS_SUCCESS;
    }

    std::shared_ptr<XfsEngine> engine_;
    std::string mountPoint_;
    std::wstring volumeLabel_;
    bool isMounted_{false};
    FSP_FILE_SYSTEM *fspFs_{nullptr};
    FSP_FSCTL_VOLUME_PARAMS volParams_{};
    FSP_FILE_SYSTEM_INTERFACE iface_{};
};

} // namespace titanxfs
#endif
