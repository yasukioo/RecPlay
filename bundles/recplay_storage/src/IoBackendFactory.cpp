// Copyright (c) 2026 yasukioo
// Author: yasukioo <yasukioo@outlook.com>

#include "IoBackendFactory.h"

#include <fstream>
#include <vector>

namespace recplay {

namespace {

class FileIoBackend final : public IoBackend {
public:
    bool Open(const std::string& path, bool readOnly) override {
        path_ = path;
        read_only_ = readOnly;
        if (readOnly) {
            stream_.open(path, std::ios::in | std::ios::binary);
        } else {
            stream_.open(path, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        }
        return stream_.is_open();
    }

    IoSize Write(const void* data, size_t len) override {
        if (!stream_.is_open() || read_only_) {
            return -1;
        }
        stream_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len));
        return stream_.good() ? static_cast<IoSize>(len) : -1;
    }

    IoSize Read(void* buf, size_t len) override {
        if (!stream_.is_open()) {
            return -1;
        }
        stream_.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(len));
        return static_cast<IoSize>(stream_.gcount());
    }

    bool Seek(int64_t offset) override {
        if (!stream_.is_open()) {
            return false;
        }
        stream_.seekg(offset, std::ios::beg);
        stream_.seekp(offset, std::ios::beg);
        return stream_.good();
    }

    void Sync() override {
        stream_.flush();
    }

    void Close() override {
        stream_.close();
    }

    void* Mmap(size_t, size_t) override {
        return nullptr;
    }

    void Munmap(void*, size_t) override {}

private:
    std::fstream stream_;
    std::string path_;
    bool read_only_ = false;
};

} // namespace

std::unique_ptr<IoBackend> IoBackendFactory::Create() {
    return std::make_unique<FileIoBackend>();
}

} // namespace recplay
