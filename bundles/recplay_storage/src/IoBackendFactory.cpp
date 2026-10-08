// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "IoBackendFactory.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace recplay {

namespace {

#ifdef _WIN32

class Win32IoBackend final : public IoBackend {
public:
    ~Win32IoBackend() override {
        Close();
    }

    bool Open(const std::string& path, bool readOnly) override {
        Close();

        read_only_ = readOnly;
        const DWORD desired_access = readOnly ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE);
        const DWORD creation_disposition = readOnly ? OPEN_EXISTING : CREATE_ALWAYS;

        file_handle_ = CreateFileA(
            path.c_str(),
            desired_access,
            FILE_SHARE_READ,
            nullptr,
            creation_disposition,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file_handle_ == INVALID_HANDLE_VALUE) {
            file_handle_ = INVALID_HANDLE_VALUE;
            return false;
        }
        return true;
    }

    IoSize Write(const void* data, size_t len) override {
        if (file_handle_ == INVALID_HANDLE_VALUE || read_only_) {
            return -1;
        }

        const auto bytes_to_write = static_cast<DWORD>(len);
        DWORD bytes_written = 0;
        const BOOL ok = WriteFile(file_handle_, data, bytes_to_write, &bytes_written, nullptr);
        return ok ? static_cast<IoSize>(bytes_written) : -1;
    }

    IoSize Read(void* buf, size_t len) override {
        if (file_handle_ == INVALID_HANDLE_VALUE) {
            return -1;
        }

        const auto bytes_to_read = static_cast<DWORD>(len);
        DWORD bytes_read = 0;
        const BOOL ok = ReadFile(file_handle_, buf, bytes_to_read, &bytes_read, nullptr);
        return ok ? static_cast<IoSize>(bytes_read) : -1;
    }

    bool Seek(int64_t offset) override {
        if (file_handle_ == INVALID_HANDLE_VALUE) {
            return false;
        }

        LARGE_INTEGER distance;
        distance.QuadPart = offset;
        return SetFilePointerEx(file_handle_, distance, nullptr, FILE_BEGIN) == TRUE;
    }

    void Sync() override {
        if (file_handle_ != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(file_handle_);
        }
    }

    void Close() override {
        for (const auto& [addr, view] : mapped_views_) {
            (void)addr;
            UnmapViewOfFile(view.base_address);
            if (view.mapping_handle != nullptr) {
                CloseHandle(view.mapping_handle);
            }
        }
        mapped_views_.clear();

        if (file_handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(file_handle_);
            file_handle_ = INVALID_HANDLE_VALUE;
        }
    }

    void* Mmap(size_t offset, size_t length) override {
        if (file_handle_ == INVALID_HANDLE_VALUE || length == 0) {
            return nullptr;
        }

        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        const auto allocation_granularity = static_cast<size_t>(system_info.dwAllocationGranularity);
        const auto aligned_offset = (offset / allocation_granularity) * allocation_granularity;
        const auto delta = offset - aligned_offset;
        const auto mapped_length = length + delta;

        const DWORD protect = read_only_ ? PAGE_READONLY : PAGE_READWRITE;
        const DWORD desired_access = read_only_ ? FILE_MAP_READ : FILE_MAP_WRITE;
        LARGE_INTEGER size;
        size.QuadPart = static_cast<LONGLONG>(aligned_offset + mapped_length);

        HANDLE mapping_handle = CreateFileMappingA(
            file_handle_,
            nullptr,
            protect,
            size.HighPart,
            size.LowPart,
            nullptr);
        if (mapping_handle == nullptr) {
            return nullptr;
        }

        LARGE_INTEGER mapping_offset;
        mapping_offset.QuadPart = static_cast<LONGLONG>(aligned_offset);
        void* base_address = MapViewOfFile(
            mapping_handle,
            desired_access,
            mapping_offset.HighPart,
            mapping_offset.LowPart,
            mapped_length);
        if (base_address == nullptr) {
            CloseHandle(mapping_handle);
            return nullptr;
        }

        auto* user_address = static_cast<std::uint8_t*>(base_address) + delta;
        mapped_views_[user_address] = MappedView{base_address, mapping_handle};
        return user_address;
    }

    void Munmap(void* addr, size_t length) override {
        (void)length;
        const auto it = mapped_views_.find(addr);
        if (it == mapped_views_.end()) {
            return;
        }

        UnmapViewOfFile(it->second.base_address);
        if (it->second.mapping_handle != nullptr) {
            CloseHandle(it->second.mapping_handle);
        }
        mapped_views_.erase(it);
    }

private:
    struct MappedView {
        void* base_address = nullptr;
        HANDLE mapping_handle = nullptr;
    };

    HANDLE file_handle_ = INVALID_HANDLE_VALUE;
    bool read_only_ = false;
    std::unordered_map<void*, MappedView> mapped_views_;
};

#else

class PosixIoBackend final : public IoBackend {
public:
    ~PosixIoBackend() override {
        Close();
    }

    bool Open(const std::string& path, bool readOnly) override {
        Close();

        read_only_ = readOnly;
        const int flags = readOnly ? O_RDONLY : (O_RDWR | O_CREAT | O_TRUNC);
        fd_ = open(path.c_str(), flags, 0644);
        return fd_ >= 0;
    }

    IoSize Write(const void* data, size_t len) override {
        if (fd_ < 0 || read_only_) {
            return -1;
        }
        return static_cast<IoSize>(write(fd_, data, len));
    }

    IoSize Read(void* buf, size_t len) override {
        if (fd_ < 0) {
            return -1;
        }
        return static_cast<IoSize>(read(fd_, buf, len));
    }

    bool Seek(int64_t offset) override {
        if (fd_ < 0) {
            return false;
        }
        return lseek(fd_, offset, SEEK_SET) >= 0;
    }

    void Sync() override {
        if (fd_ >= 0) {
            fsync(fd_);
        }
    }

    void Close() override {
        for (const auto& [addr, view] : mapped_views_) {
            (void)addr;
            munmap(view.base_address, view.length);
        }
        mapped_views_.clear();

        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

    void* Mmap(size_t offset, size_t length) override {
        if (fd_ < 0 || length == 0) {
            return nullptr;
        }

        const int prot = read_only_ ? PROT_READ : (PROT_READ | PROT_WRITE);
        void* mapped = mmap(nullptr, length, prot, MAP_SHARED, fd_, static_cast<off_t>(offset));
        if (mapped == MAP_FAILED) {
            return nullptr;
        }

        mapped_views_[mapped] = MappedView{mapped, length};
        return mapped;
    }

    void Munmap(void* addr, size_t length) override {
        (void)length;
        const auto it = mapped_views_.find(addr);
        if (it == mapped_views_.end()) {
            return;
        }
        munmap(it->second.base_address, it->second.length);
        mapped_views_.erase(it);
    }

private:
    struct MappedView {
        void* base_address = nullptr;
        size_t length = 0;
    };

    int fd_ = -1;
    bool read_only_ = false;
    std::unordered_map<void*, MappedView> mapped_views_;
};

#endif

} // namespace

std::unique_ptr<IoBackend> IoBackendFactory::Create() {
#ifdef _WIN32
    return std::make_unique<Win32IoBackend>();
#else
    return std::make_unique<PosixIoBackend>();
#endif
}

} // namespace recplay
